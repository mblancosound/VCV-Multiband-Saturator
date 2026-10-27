# MICRO-SPEC — Oversampling 1x/2x/4x/8x (`Multiband_Saturator`)

Iteración: **W-OS** · Estado: **IMPLEMENTADO** (plugin v2.1.0) · Cierra **C3** de
`specs/multiband_saturator_spec.md` y añade oversampling configurable por patch.
Ficheros de producción: `src/Oversampling.hpp` (nuevo, header-only), `src/Saturator.hpp`,
`src/Saturator.cpp`, `src/SaturatorWidget.cpp` (nuevo). `Makefile` sin cambios: usa `src/*.cpp`.

---

## 1. Objetivo y decisión de diseño

La no linealidad `tanh(x·drive)` genera armónicos por encima de Nyquist que **pliegan** dentro de
la banda audible (alias). El oversampling sube la señal a `N·fs`, aplica la cadena y vuelve a `fs`.

Dos opciones de diseño:

| Opción | Alcance | Conversores | Coste | Problema |
|---|---|---|---|---|
| A | Sobremuestrear **sólo el `tanh`** de cada banda | 3 bandas × 2 canales × (up+down) = **12** | alto | 12 FIRs, 3 latencias que hay que alinear entre sí y con el dry |
| B (elegida) | Sobremuestrear **la cadena completa** estéreo | 2 canales × (up+down) = **4** | menor | los crossovers pasan a correr a `fs·N` ⇒ hay que re-diseñarlos (cierra C3) |

Se elige **B**: mismos 4 conversores para cualquier patrón de bandas, un único punto de latencia
(15 muestras) y una sola alineación dry/wet. Además, al ejecutar los biquads a `fs·N`, sus
coeficientes deben recalcularse con la frecuencia real (`fs = sampleRate · factor`), lo que obliga a
mover su configuración a `onSampleRateChange()` + `applyOversampling()` y **resuelve C3**.

El factor es **1x (off) por defecto**; el oversampling es opt-in. `1x` conserva la ruta DSP clásica sin conversión ni latencia.

---

## 2. Estructura de `src/Oversampling.hpp`

Header-only, namespace `coaldynamics`, sin estado global. Constantes:

| Constante | Valor | Significado |
|---|---|---|
| `OS_QUALITY` | `16` | taps por fase de `dsp::Upsampler` / `dsp::Decimator` (Q) |
| `OS_CUTOFF` | `0.95f` | corte del FIR relativo al Nyquist **base** (0.95 × 24 kHz = **22.8 kHz** a 48 kHz) |
| `OS_LATENCY` | `OS_QUALITY - 1 = 15` | latencia del par up+down en **muestras base** |

Tipos:

| Tipo | Factores | Contenido |
|---|---|---|
| `OversampledStage<N, Q>` | `N` = 2, 4, 8 | `upL/upR` (`dsp::Upsampler<N,Q>`) + `decL/decR` (`dsp::Decimator<N,Q>`), todos con `OS_CUTOFF`; `reset()` limpia los 4; `upsample()` y `downsampleL/R()` |
| `BypassStage<Q>` | 1 | sin conversión: copia la muestra base y devuelve `in[0]`; `reset()` no-op; `FACTOR = 1` |

Ambos exponen la **misma interfaz** (`FACTOR`, `upsample`, `downsampleL`, `downsampleR`, `reset`), de
modo que `Saturator::processBands<Stage>()` se escribe **una sola vez** y el `switch` de `process()`
sólo elige la etapa. En `Saturator.hpp` se instancian las cuatro (`stage1/2/4/8`); en cada bloque sólo
se ejecuta la activa, y al cambiar de factor se resetea la nueva.

---

## 3. Cadena de procesado

```
IN_L/R
  │
  ├─ dry ──────────────────────────────► delay OS_LATENCY (15) ──┐
  │                                                              │
  └─ up ×N (FIR Q=16, cutoff 0.95)                               │
        │                                                        │
        ├─ LP 250 Hz  Q 0.707 ─► tanh(x·driveLow)  ─► pan/vol/CV ─┤
        ├─ BP 1000 Hz Q 1.0   ─► tanh(x·driveMid)  ─► pan/vol/CV ─┤ suma
        └─ HP 4000 Hz Q 0.707 ─► tanh(x·driveHigh) ─► pan/vol/CV ─┘
                                                          │
                                                    down ×N
                                                          │
                          MIX: dry·(1−mix) + wet·mix ◄────┘
                                                          │
                                                   OUTPUT_VOL ─► OUT_L/R
```

- Crossovers diseñados a **`fs = sampleRate · N`**; `drive = 1 + 10·DRIVE` sin cambios.
- Pan/vol/CV se resuelven **una vez por muestra base** en `process()` (`OSParams`) y se aplican a cada
  submuestra sobremuestreada; no se interpolan al dominio sobremuestreado.
- `1x`: `BypassStage` no convierte; `dryDelay = 0`; la ruta DSP es la de v2.0.1.
- La suma de bandas se hace **antes** de decimar, con los tres tanh ya aplicados.
- El dry se retrasa `OS_LATENCY` (15) para que el MIX no cancele en peine; con `1x`, 0.

---

## 4. UI y persistencia

| Aspecto | Implementación |
|---|---|
| Selección | menú contextual del módulo: `SaturatorWidget::appendContextMenu()` → `createIndexSubmenuItem("Oversampling", {"Off (1x)", "2x", "4x", "8x"}, ...)` (radio con check) |
| Defecto | **1x (off)** (`int oversampling = 1`) |
| Hilo UI/JSON | `setOversampling(factor)` valida (1/2/4/8), ignora si no cambia y sólo escribe `pendingOversampling` (`std::atomic<int>` desde v2.4.0) |
| Hilo de audio | al principio del siguiente bloque, `process()` lo consume con `exchange(0)` y llama a `applyOversampling()`, que valida el factor: resetea etapa, filtros, buffers dry e índice, y recalcula coeficientes |
| Persistencia | `dataToJson()` → `{"oversampling": n}`; `dataFromJson()` lee la clave, valor inválido → 1 |
| Patches antiguos | sin la clave ⇒ se queda el defecto **1x (off)** (no hay migración ni error) |
| Compatibilidad | **no** se tocan `ParamIds`, `InputIds`, `OutputIds` ni `LightIds`; ningún parámetro nuevo |
| Panel | sin control dedicado (no hay hueco en el panel): oversampling sólo por menú contextual |

La selección no es instantánea: el menú refleja `oversampling` (factor **aplicado**), no
`pendingOversampling`, así que el check y el audio cambian juntos al inicio del bloque siguiente. El
reset de estado es la causa del **click** descrito en §6.

---

## 5. Mediciones

Entorno: 48 kHz, `OS_QUALITY = 16`, `OS_CUTOFF = 0.95`, seno de **5 kHz a 5 V de pico**,
`drive = 11` (DRIVE al 100 %), FFT de 16384 puntos con ventana Blackman-Harris. "Alias en banda" =
peor componente **no armónico** por debajo de 20 kHz, relativo al fundamental.

| Factor | Latencia (muestras base) | Alias en banda | HF 15 kHz | HF 20 kHz |
|---|---:|---:|---:|---:|
| 1x | 0 | −14.7 dB | 0.00 dB | 0.00 dB |
| 2x | 15 | −24.8 dB | −0.14 dB | −3.97 dB |
| 4x | 15 | −32.8 dB | −0.12 dB | −3.89 dB |
| 8x | 15 | −43.3 dB | −0.11 dB | −3.85 dB |

Lecturas:

- **Latencia exacta de 15 muestras base** en 2x, 4x y 8x, medida por impulso y por correlación cruzada
  de ruido blanco (coinciden); 0 en 1x. A 48 kHz son 0.3125 ms.
- El **par up/down es transparente en banda**: 0.00 dB hasta 10 kHz, −0.12 dB @15 kHz, −3.89 dB @20 kHz.
- Con `MIX = 0.5` la respuesta es **plana, sin peine**; la diferencia de RMS (−0.5 dB) es atribuible al
  leve filtrado del FIR, no a desalineación.
- El alias mejora **10.1 dB de 1x a 2x**, **18.1 dB a 4x** y **28.6 dB a 8x**.
- **Verificado en Rack (2026-10-27)**: misma señal a 1x y a 4x, medida con osciloscopio a la salida del
  módulo → sin diferencia de nivel ni de forma de onda. Confirma que el dry queda alineado con el wet y
  que el par up/down no altera la señal en banda. El número exacto (15 muestras) sale de la medición por
  impulso y correlación, no del osciloscopio, que no resuelve 0.31 ms.
- Build: `make clean && make` → **0 errores, 0 warnings** (`-Wall -Wextra`); `make dist` →
  `dist/Coal_Dynamics-2.1.0-win-x64.vcvplugin`.

---

## 6. Limitaciones conocidas

| Limitación | Detalle / mitigación |
|---|---|
| **Click al cambiar de factor** | `applyOversampling()` resetea FIRs, biquads y buffers dry. Es un cambio de configuración, no un parámetro automatizable; si molesta, bajar MIX/OUTPUT durante el cambio |
| **CPU ~lineal con el factor** | El coste DSP por muestra base crece aproximadamente ×N (2x ≈2×, 8x ≈8×) más el FIR; no hay cifra medida de % de CPU, sólo la tendencia. El uso por defecto es 1x (off) |
| **Rolloff a 20 kHz** | El FIR corta en 22.8 kHz: −3.89 dB @20 kHz (−0.12 dB @15 kHz). audible sólo en el extremo superior; bajar `OS_CUTOFF` empeora el rolloff |
| **Alias residual cerca de Nyquist** | El primer lóbulo de alias del FIR (25 kHz → 23 kHz a 4x) deja ~−25 dB por encima de 20 kHz (inaudible). `OS_CUTOFF = 0.8` lo lleva a ~−43 dB a cambio de −1.9 dB @15 kHz y −6.4 dB @20 kHz |
| **`tanh` es una no linealidad sin memoria** | Genera armónicos por encima de `4·fs/2` que pliegan **dentro del propio dominio sobremuestreado**; por eso 8x mejora ~10 dB más que 4x. No se elimina con más calidad de FIR, sólo con más factor |
| **Latencia no compensada por Rack** | Rack no aplica compensación automática entre módulos: las 15 muestras se suman al camino. El dry se alinea dentro del módulo, no respecto a otras rutas |
| **Constantes en compilación** | `OS_QUALITY` y `OS_CUTOFF` no son parámetros de usuario; cambiarlas exige rebuild y re-medir la tabla de §5 |

---

## 7. Criterios de aceptación verificables

1. **Build**: `export PATH=/mingw64/bin:$PATH; RACK_DIR=/c/Rack-SDK2.6.6 make clean && make` →
   0 errores, 0 warnings; `make dist` genera `dist/Coal_Dynamics-2.2.1-win-x64.vcvplugin`.
2. **Defecto**: módulo nuevo sin patch → `oversampling == 1` y el submenú marca **Off (1x)**.
3. **UI**: clic derecho → `Oversampling` muestra las cuatro opciones; seleccionar una cambia el check y
   el audio en el siguiente bloque (con DRIVE alto y señal aguda el cambio de alias es audible).
4. **Latencia**: impulso a la entrada → el pico del wet aparece en la muestra **15** para 2x, 4x y 8x, y
   en la **0** para 1x; con `MIX = 0.5` no hay cancelación en peine (RMS plano dentro de ±1 dB).
5. **Alias**: con la configuración de §5, 4x ≤ **−32 dB** y 8x ≤ **−43 dB**; 1x se mantiene en ~−15 dB.
6. **FIR**: 0.00 dB hasta 10 kHz, ≥ −0.2 dB @15 kHz, ≥ −4.0 dB @20 kHz (tolerancia ±0.5 dB sobre §5).
7. **Persistencia**: guardar con 8x → recargar el patch → sigue en 8x; un patch sin la clave
   `oversampling` carga en **1x (off)**; un valor inválido (`3`, `0`, negativo) cae a 1x.
8. **Compatibilidad**: `git diff` no muestra cambios en los enums de IDs; un patch de v2.0.1 carga sin
   reasignar parámetros ni entradas.
9. **1x**: sin conversión ni latencia; la ruta DSP coincide con la de v2.0.1 (mismos crossovers, drive,
   pan, vol, mix y salida).

---

## 8. Cómo re-medir

Arnés **offline**, fuera del repo (p. ej. en `%TEMP%`). No requiere Rack ni `make`: sólo cabeceras del
SDK y `g++`. Usa `dsp::Upsampler` / `dsp::Decimator` de `dsp/resampler.hpp` (o directamente
`src/Oversampling.hpp`) y `dsp::BiquadFilter` de `dsp/filter.hpp`.

```bash
g++ -std=c++11 -D_USE_MATH_DEFINES \
  -I C:/Rack-SDK2.6.6/include \
  -I C:/Rack-SDK2.6.6/dep/include \
  -I C:/Coal_Dynamics/Multiband_Saturator/src \
  os_harness.cpp -o os_harness.exe
```

Pasos:

1. **Cadena**: para cada factor `N ∈ {1,2,4,8}`, procesar un seno de 5 kHz / 5 V pico con
   `drive = 11`, crossovers 250/1000/4000 Hz a `fs·N`, `tanh` por banda y suma; descartar el transitorio
   (≥ 4096 muestras) y capturar 16384 muestras.
2. **FFT**: ventana Blackman-Harris, buscar el bin del fundamental y todos los armónicos; reportar la
   peor componente no armónica **< 20 kHz** relativa al fundamental en dB (tabla de §5).
3. **Latencia**: impulso unitario → índice del pico del wet (restar 0 si 1x); repetir con ruido blanco
   y correlación cruzada. Debe dar **15** en 2x/4x/8x.
4. **FIR**: pasar el par up/down sin `tanh` con senos de 1/5/10/15/20 kHz y medir dB respecto a la
   entrada; comprobar la transparencia en banda y el rolloff.
5. **Mix**: con `MIX = 0.5`, barrer la fase relativa dry/wet; la RMS debe quedar plana (sin peine).
6. Si se cambia `OS_QUALITY`, `OS_CUTOFF` o la ley de drive, **actualizar la tabla de §5** y las
   notas de §6.

---

## 9. Trazabilidad y alcance

- **Cierra C3**: los biquads ya no se configuran con `APP->engine` en el constructor; `sampleRate` se
  toma de `onSampleRateChange()` y `updateFilterFrequencies()` es el único punto que toca coeficientes,
  con `fs = sampleRate · oversampling`. Ver nota en `specs/multiband_saturator_spec.md` §2 (C3), §1.3 y §C-7.
- **Estructura**: `src/Saturator.cpp` queda sólo con DSP; la UI y el menú contextual viven en
  `src/SaturatorWidget.cpp`; `src/Oversampling.hpp` es nuevo.
- **Versión**: 2.1.0. `Makefile` no cambia (compila `src/*.cpp`).
- **Fuera de alcance**: oversampling por banda, corte/calidad editables por el usuario, compensación de
  latencia a nivel de patch, parámetro/automatización para el factor, traducción del menú.

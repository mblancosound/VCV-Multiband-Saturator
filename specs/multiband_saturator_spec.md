# Multiband Saturator (VCV Rack 2) — Auditoría, Riesgos y Plan de Trabajo

Fecha: 2026-10-17 · Autor: ARQUITECTO · Estado: SPEC (sin implementar)
Workspace: `C:\Coal_Dynamics\Multiband_Saturator` · Git: 1 commit (`b209141 Initial commit: Multiband Saturator v2.0.0`)

---

## 1. Spec breve del proyecto

### 1.1 Qué es (corrección de la premisa)

**No es un plugin JUCE/CLAP/VST.** Es un **módulo para VCV Rack 2** (ecosistema VCV Rack, no VCV Rack Pro):

| Aspecto | Valor |
|---|---|
| Formato | Plugin Rack 2 (`plugin.json` + `plugin.dll` → `.vcvplugin`) |
| SDK | `RACK-SDK2.6.6` (existe en `C:\Rack-SDK2.6.6`) |
| Lenguaje | C++11 (`-std=c++11`, flags de `arch.mk`) |
| Build | GNU Make + plugin.mk (`jq` requerido) |
| Targets | `make` → `plugin.dll`; `make dist` → `.vcvplugin` (zstd) |
| Entry point | `void init(Plugin*)` en `src/plugin.cpp` |

### 1.2 Módulo

- Slug plugin `Coal_Dynamics`, módulo `Multiband_Saturator`, v2.0.0, MIT, autor Manuel Blanco.
- Panel **18 HP × 3U** (`box.size = Vec(270, 380)`; `RACK_GRID_WIDTH=15`, `RACK_GRID_HEIGHT=380`).
- Superficie: **17 params, 8 inputs, 2 outputs, 6 lights (sin widgets)**.
- DSP (`Saturator::process`):
  1. crossover fijo por canal: LP 250 Hz Q0.707, BP 1000 Hz Q1.0, HP 4000 Hz Q0.707 (`dsp::BiquadFilter`);
  2. `tanh(x * drive)`, `drive = 1 + 10·DRIVE` (1 → 11);
  3. pan (`(1∓pan/2)`) y vol por banda, con CV y atenuador por banda;
  4. mezcla dry/wet lineal + gain de salida.
- UI: `Knob48` / `Knob20` (SVG propios), `CustomInputPort` / `CustomOutputPort` (PJ301M), 4 tornillos.

### 1.3 Estructura

```
Makefile          Rack build: chequeo de `jq`, RACK_DIR, src/*.cpp, DISTRIBUTABLES += res (sin cambios en v2.1.0)
plugin.json       manifiesto (plugin + 1 módulo)
LICENSE.txt       1 línea: "MIT License"   <-- incompleto
README.md         build/estructura/install + sección de oversampling
src/plugin.hpp    Knob48, Knob20, CustomInput/OutputPort, TornilloNegro, externs
src/plugin.cpp    init(Plugin*) -> createModel<Saturator, SaturatorWidget>
src/Saturator.hpp enums ParamIds/InputIds/OutputIds/LightIds + estado DSP + etapas OS + declaración del widget
src/Saturator.cpp ctor (config*), process() y DSP  <-- sólo DSP desde v2.1.0
src/SaturatorWidget.cpp **SaturatorWidget** (UI) + menú contextual de oversampling  <-- nuevo en v2.1.0 (split del widget)
src/Oversampling.hpp constantes OS_* + `OversampledStage<N,Q>` / `BypassStage`  <-- nuevo en v2.1.0 (header-only)
res/              Saturator.svg (panel 270x380), Knob_20/32/48px.svg, PJ301M_in/out.svg, tornillo_negro.svg
```

**Cambio de estructura (v2.1.0):** el widget salió de `Saturator.cpp` a `src/SaturatorWidget.cpp` (W0.5)
y se añadió `src/Oversampling.hpp`. `Saturator.cpp` queda como unidad exclusivamente DSP y la UI (incluido
el menú de oversampling) es una unidad disjunta. `Makefile` no cambia: compila `src/*.cpp`.

### 1.4 Build verificado (medido en esta máquina)

Toolchain: MSYS2 (`C:\msys64\usr\bin\make.exe`, `mingw64\bin\g++`), SDK `C:/Rack-SDK2.6.6`, **`jq` en `mingw64\bin`**.

```bash
export PATH=/mingw64/bin:$PATH
cd <repo> && RACK_DIR=/c/Rack-SDK2.6.6 make          # o: make dist
```

| Medición | Valor |
|---|---|
| Build limpio (2 TUs) | **7.2 s** |
| Recompilar 1 fichero | **2.7 s** |
| `make` sin cambios | **0.9 s** |
| `make dist` | **1.6 s** → `.vcvplugin` 118 KiB, `plugin.dll` stripped 244 KiB |
| Warnings `-Wall -Wextra` | **0** (build limpio verificado) |

Artefactos de verificación reutilizables (en temp, fuera del repo):
`C:\Users\manub\AppData\Local\Temp\opencode\mbs-build\` → `overlay.svg`, `crop_cv.svg`, `crop_audio.svg` (+ PNG ya renderizados) para el chequeo panel↔código.

---

## 2. Riesgos y fallos (priorizados, con evidencia)

### BUILD / TOOLCHAIN

| ID | Sev | Riesgo | Evidencia / mitigación |
|---|---|---|---|
| B1 | **ALTA** | `make` falla tal cual: `plugin.mk` necesita `jq`, que está en `C:\msys64\mingw64\bin` y **no** está en el PATH del shell msys → `SLUG could not be found in manifest` | Reproducido. Mitigar: `export PATH=/mingw64/bin:$PATH` (o `MSYSTEM=MINGW64`) y documentarlo en README/CI |
| B2 | BAJA | `Makefile`: `FLAGS += -Idep/include` pero `dep/` está vacío (y `.gitignore` ignora `dep/build/`) | Config muerta; borrar la línea o añadir marcador |
| B3 | MEDIA | `M_PI` en `src/plugin.hpp` depende de `-D_USE_MATH_DEFINES` que inyecta `arch.mk`. Compilar a mano (IDE/g++) falla | Reproducido: `'M_PI' was not declared in this scope`. Contrato: compilar **solo** con `make` |
| B4 | MEDIA | 3 binarios distintos en el árbol: `plugin.dll` raíz (1.618.190 B), `Saturator/plugin.dll` (1.662.689 B), `dist/.../plugin.dll` (244.736 B). Gitignored pero presentes | `git ls-files` confirma que **no** están versionados. Riesgo: validar contra un artefacto obsoleto |
| B5 | BAJA | Sin CI, sin target `lint`/`check`, 1 commit | Añadir workflow |
| B6 | BAJA | `RACK_DIR ?= /c/Rack-SDK2.6.6` (ruta MSYS) rompe en Linux/macOS o si el SDK se mueve | Documentar override por plataforma |

### CÓDIGO / DSP

| ID | Sev | Riesgo | Evidencia |
|---|---|---|---|
| **C1** | **CRÍTICA** | **Mapeo de entradas CV permutado respecto al panel.** La fila de jacks en `SaturatorWidget` declara, por columna: `LOW_VOL_INPUT`(52.6), `MID_VOL_INPUT`(82.4), `HIGH_VOL_INPUT`(119.7), `LOW_PAN_INPUT`(149.5), `MID_PAN_INPUT`(187.6), `HIGH_PAN_INPUT`(217.4). El panel rotula **por banda** "PAN" (izq.) y "VOL" (der.) → lo correcto es `LOW_PAN, LOW_VOL, MID_PAN, MID_VOL, HIGH_PAN, HIGH_VOL`. La fila de **atenuadores** CV ya usa el orden correcto por banda, luego la fila de jacks es la que falla | Render de `res/Saturator.svg` (`crop_cv.png`): etiquetas PAN(izq)/VOL(der) por banda. Consecuencia:.patchear CV en el jack bajo "PAN" afecta al VOL de otra banda |
| C2 | MEDIA | **La sección CV no tiene artwork.** El panel sólo tiene líneas divisoria + etiquetas LOW/MID/HIGH/PAN/VOL; los 6 jacks CV (y=273) y los 6 knobs CV (y=299.6) caen en zona vacía, 18–45 px por debajo de sus etiquetas | Verificado en `crop_cv.png` (crop svg y 225–320) |
| C3 | INFO | **RESUELTO (W-OS, `oversampling_spec.md`)** — Los crossovers se configuran en `onSampleRateChange()` y en `applyOversampling()`; `updateFilterFrequencies()` es el único punto que toca coeficientes y usa `fs = sampleRate · factor` (factor de sobremuestreo). Ya no se lee `APP->engine` en el ctor ni se depende del SR global en construcción | Verificado con oversampling 1x/2x/4x/8x; build limpio 0 warnings |
| C4 | MEDIA | 6 luces declaradas (`ENUMS(SATURE_LIGHT, 6)`) y escritas en `process()`, pero **nunca se añade ningún `addChild(createLight...)`** → código muerto. Además ambas luces de banda usan sólo el canal L (0 y 1 idénticas), la métrica `|wet - x·drive|` satura a 1 de inmediato, y hay typo (`SATURE`) | `src/Saturator.cpp:153-169` vs `172-259` (sin luces) |
| C5 | MEDIA | **No hay bypass** y el estado por defecto no es transparente: DRIVE=0 ⇒ `drive=1` ⇒ `tanh` ya distorsiona. **THD medido = 33 %** con entrada 10 Vpp (H3 = −10,7 dB); MIX default = 1 ⇒ el sumatorio de 3 bandas por defecto | THD por DFT (N=4096), `tanh(5·sin)` |
| C6 | MEDIA | **Ley de drive comprimida:** DRIVE 50 % (x6) → 35,15 % THD; DRIVE 100 % (x11) → 39,25 % THD. La mitad superior del knob es casi inaudible; no hay calibración en dB ni make-up gain | Misma medición |
| C7 | BAJA | Sumatorio de bandas sin normalizar: \|H_LP250\|+\|H_BP1000\|+\|H_HP4000\| tiene **pico +1,00 dB @1 kHz** y **suelo −2,06 dB @2,5 kHz**. Con DRIVE=0 y MIX=1 la salida ≠ entrada hasta ±2 dB y hay ripeo de fase en solapes | Biquad RBJ, fs=44,1 kHz, Q 0,707/1,0/0,707 |
| C8 | BAJA | Sin DC-blocker tras `tanh`: entradas unipolares/con offset (VCO, fuentes con offset) generan offset DC a la salida (`tanh(DC·drive)` esFuertemente compresivo) | Inspección de código |
| C9 | INFO | **RESUELTO (`cv_scaling_fix_spec.md`, W-CVSCALE)** — Escalado CV unificado en /10: pan y vol usan el mismo divisor, 10 V = 1 unidad. Era `V/5` en pan (saturaba a ±5 V) frente a `V/10` en vol. | `src/Saturator.cpp:107-131` |
| C10 | BAJA | Mezcla dry/wet **lineal**; con señales correlacionadas y wet desfasado hay caída cerca de posiciones medias (falta ley de igual potencia) | `src/Saturator.cpp:145-147` |
| C11 | BAJA | `randomizeEnabled = false` en los 17 params → "Randomize module" (Ctrl+R) no hace nada | `src/Saturator.cpp:32-48` |
| C12 | BAJA | `extern Plugin *pluginInstance; extern Model *modelSaturator;` duplicados en `plugin.cpp` y `plugin.hpp`, **definición única** en `Saturator.cpp` → frágil ante un tercer TU (símbolo duplicado) | Lectura de código |
| C13 | BAJA | `using namespace rack;` a nivel global en ambos headers (contaminación de espacio de nombres) | `plugin.hpp:5`, `Saturator.hpp:5` |
| C14 | INFO | `std::tanh`/`std::abs` compilan por include transitivo de `<cmath>`; frágil ante cambios de SDK | Compila hoy con SDK 2.6.6 |
| C15 | INFO | Módulo **no polifónico** (convención normal para efectos Rack). Fallback mono en `IN_R` ✓ correcto | `src/Saturator.cpp:87` |
| C16 | INFO | Crossovers fijos (250/1000/4000 Hz) y **no documentados** en README | `plugin.json`/`README.md` |

### CONFIG / META

| ID | Sev | Riesgo | Evidencia |
|---|---|---|---|
| M1 | MEDIA | `LICENSE.txt` = 1 línea ("MIT License") sin texto de licencia ni copyright, mientras `plugin.json` declara `"license": "MIT"` | Lectura del fichero |
| M2 | BAJA | `plugin.json` sin `brandSlug`/logo; slug con guion bajo (`Coal_Dynamics`) — legal pero no convencional para VCV Library | Lectura |
| M3 | BAJA | Deriva de README: atribuye el widget a `plugin.cpp` (está en `Saturator.cpp`), no menciona `jq`, ni crossovers fijos, ni la ausencia de bypass/luces | Lectura |
| M4 | BAJA | `res/Knob_32px.svg` (6.342 B) no se referencia en el código (asset muerto). Ojo: `Knob_32px` y `Knob_48px` tienen **MD5 distintos** (verificado) | Hashes |
| M5 | INFO | El panel no tiene atributos `data-*` → el dev tool de Rack no muestra nombres de componentes; el chequeo panel↔código es manual (overlay) | `Saturator.svg`: 0 coincidencias de `data-` |

### Conv坐标系 — nota de convención (importante para escritores)

Rack 2 usa **origen arriba-izquierda, `y` hacia abajo** (igual que SVG/Inkscape ≥1.0), por lo que **las coordenadas del widget se usan tal cual, sin voltear**. Verificado por render con overlay: las coordenadas de `SaturatorWidget` coinciden con el artwork (OUTPUT/MIX coincide exactamente; PAN/VOL coincide con ~1–2 px). **No "arreglar" las Y.** Fuentes: [Rack v2 `ZoomWidget::draw` (sin flip)](https://raw.githubusercontent.com/vcvrack/Rack/v2/src/widget/ZoomWidget.cpp), [VCV Community: "Rack origin is in the left-top corner"](https://community.vcvrack.com/t/placing-components/9656).

---

## 3. Plan de ficheros y timings

Regla: **unidades disjuntas** para poder escribir en paralelo. Estado actual: `src/Saturator.cpp` contiene DSP **y** widget ⇒ cualquier trabajo en C++ choca.

### W0 (hecho por ARQUITECTO) — `specs/multiband_saturator_spec.md`
Sin código. Incluidos los overlays reutilizables en `C:\Users\manub\AppData\Local\Temp\opencode\mbs-build\`.
Timing: 45 min (realizado).

### W0.5 (opcional, recommended antes de paralelizar) — refactor "split TU"
Crear `src/SaturatorWidget.cpp` (mueve `SaturatorWidget::SaturatorWidget`) y dejar `src/Saturator.cpp` sólo con DSP.
Resultado: **C++ separable en dos ficheros disjuntos**. Riesgo: bajo (código movido tal cual, sin cambios de coordenadas).
Timing: **10–15 min**. Bloqueante para W1/W2/W3 en paralelo.

### Unidades de trabajo (con W0.5 aplicado → todas disjuntas)

| ID | Ficheros (únicos) | Contenido | Timing |
|---|---|---|---|
| **W1** | `src/SaturatorWidget.cpp` | Fix **C1**: reordenar IDs de la fila de jacks CV a orden por banda. Actualizar comentario con la tabla de mapeo | **10–15 min** |
| **W2** | `src/Saturator.cpp`, `src/Saturator.hpp` | **C3** mover config de filtros a `onSampleRateChange(e)`; **C8** DC-blocker; ~~**C9** unificar escalado CV (`/5` o `/10` en ambos)~~ (**cerrado** por W-CVSCALE, `cv_scaling_fix_spec.md`); **C10** mezcla de igual potencia. Sin tocar el widget | **45–75 min** |
| **W3** | `src/Saturator.cpp`, `src/Saturator.hpp`, `src/SaturatorWidget.cpp` | **C4**: o implementar luces (6 `MediumLight` + `configLight` con nombres + usar L/R reales) **o** eliminarlas. **C11** decidir `randomizeEnabled`; **C12/C13** hygiene (definición única de `pluginInstance`, sin nuevos `using namespace`) | **20–40 min** (implementar luces) / **10 min** (borrar) |
| **W4** | `res/Saturator.svg` | **C2**: artwork de la sección CV (6 jack placeholders + 6 knob placeholders) alineado con (52.6/82.4/119.7/149.5/187.6/217.4 @ y=273) y (52.6/82.4/120.1/149.9/187.6/217.4 @ y=299.6); opcionalmente atributos `data-*` (**M5**). Sólo vector paths (Rack no soporta texto/gradientes/filtros) | **60–120 min** (trabajo de diseño) |
| **W5** | `Makefile`, `README.md` | **B1** documentar `PATH=/mingw64/bin` / `MSYSTEM=MINGW64`; **B2** limpiar `-Idep/include`; **B6** nota multi-plataforma de `RACK_DIR`; **M3** corregir deriva del README (widget en `Saturator.cpp`, crossovers fijos, sin bypass); añadir `.clang-format` (Google, 2 espacios) | **15–25 min** |
| **W6** | `LICENSE.txt`, `plugin.json` | **M1** texto MIT completo + línea de copyright; **M2** valorar `brandSlug` | **5–10 min** |
| **W7** | `.github/workflows/build.yml` (nuevo) | **B5** CI Windows: MSYS2 + `RACK_DIR=C:/Rack-SDK2.6.6` + `make` + `make dist`, artifact `.vcvplugin` | **20–30 min** |

**Conflictos**: sin W0.5, W1/W2/W3 se pisan en `src/Saturator.cpp` → un único escritor de C++.
W4 (SVG) y W5/W6/W7 son disjuntos del C++ y pueden ir en paralelo sin riesgo.

**Total M-cycle**: ~2,5–4 h de trabajo efectivo (sin W4: ~1,5–2,5 h). Versión: bump en `plugin.json` sólo al publicar (VCV Library congela versiones en revisión); cambios de patch IDs son compatibles, cambios de orden **no**.

---

## 4. Contratos para escritores

### C-1 Build (obligatorio)
- Sólo `make` / `make dist`. Nunca g++/IDE manual (falta `-D_USE_MATH_DEFINES`, ver B3).
- Entorno verificado:
  ```bash
  export PATH=/mingw64/bin:$PATH     # jq; sin esto make falla
  RACK_DIR=/c/Rack-SDK2.6.6 make
  ```
- Criterio de "no roto": build limpio con **0 warnings** (`-Wall -Wextra` los pone `arch.mk`) y artefacto en `dist/Coal_Dynamics-2.1.0-win-x64.vcvplugin`.
- No commitear binarios (`*.dll`, `*.vcvplugin`, `build/`, `dist/`, `Saturator/`) — ya están gitignored; no añadir excepciones.
- No tocar `dep/` (vacío) ni añadir dependencias: el build no tiene red.

### C-2 Compatibilidad de patch (inviolable)
- `ParamIds`, `InputIds`, `OutputIds`, `LightIds` de `src/Saturator.hpp` están **persistidos** en patches de Rack por índice. **No renumerar, no reordenar, no eliminar**. Parámetros nuevos: añadir **al final** antes de `NUM_PARAMS` y subir el primer argumento de `config()`.
- Nombres de `configParam`/`configInput` shown en tooltips: pueden cambiar; IDs no.

### C-3 Mapeo de jacks CV (fuente de verdad, tras W1)

| x (px) | y | Widget | ID correcto |
|---|---|---|---|
| 52.6 | 273 | `CustomInputPort` | `LOW_PAN_INPUT` |
| 82.4 | 273 | `CustomInputPort` | `LOW_VOL_INPUT` |
| 119.7 | 273 | `CustomInputPort` | `MID_PAN_INPUT` |
| 149.5 | 273 | `CustomInputPort` | `MID_VOL_INPUT` |
| 187.6 | 273 | `CustomInputPort` | `HIGH_PAN_INPUT` |
| 217.4 | 273 | `CustomInputPort` | `HIGH_VOL_INPUT` |

 knobs CV: 52.6=`LOW_PAN_CV`, 82.4=`LOW_VOL_CV`, 120.1=`MID_PAN_CV`, 149.9=`MID_VOL_CV`, 187.6=`HIGH_PAN_CV`, 217.4=`HIGH_VOL_CV` (sin cambios).

### C-4 Geometría del panel
- `box.size` **debe** seguir siendo `Vec(270, 380)` (18 HP × 3U; ancho múltiplo de 15, alto exactamente 380 o `RackWidget::addModule` lanza excepción).
- Sistema de coordenadas: **y hacia abajo, sin voltear** (ver nota §2). Cualquier `y` nuevo se lee directamente del SVG.
- Si se cambia una coordenada en `SaturatorWidget.cpp`, hay que cambiar la pieza correspondiente en `res/Saturator.svg` y viceversa. Verificar con el overlay (`overlay.svg`/`crop_cv.svg` en temp) o captura en Rack.
- Los SVGs deben quedar en `<g>`, sin texto rasterizado, sin `filter`, sin `foreignObject`, sin CSS externo (restricciones del renderer de Rack).

### C-5 Símbolos globales
- `Plugin *pluginInstance` y `Model *modelSaturator` se **definen en un único TU**. Hoy: `src/Saturator.cpp`; los demás sólo `extern` (idealmente en un `plugin.hpp` y sin repetir los `extern` en `.cpp`).
- Si se añade un TU nuevo: incluir cabeceras, no re-declarar `using namespace rack;`.

### C-6 DSP
- No cambiar la ley `drive = 1 + 10·DRIVE` ni las frecuencias de crossover sin actualizar README **y** las baselines de abajo.
- Filtros: configurar en `onSampleRateChange(e)` con `e.sampleRate` y re-calcular en
  `applyOversampling()`; frecuencias normalizadas `f / (fs · factor)` (factor de sobremuestreo, 1 en 1x).
- No introducir `APP->engine` dentro de `process()`; en el ctor sólo `config*`.
- Preservar: entrada `IN_R` cae a mono si no está conectada.

### C-7 Baselines medidos (no regresión; re-verificar si se toca DSP)
- THD(tanh(A·sin)), N=4096: A=5, drive=1 → **33,08 %** (H3 −10,7 dB) · drive=6 → **35,15 %** · drive=11 → **39,25 %**.
- Respuesta del sumatorio de bandas (drive 0): **+1,00 dB @1 kHz**, **−2,06 dB @2,5 kHz**, ~0 dB a 20 Hz y 20 kHz.
- Timings de build: limpio 7,2 s · 1 fichero 2,7 s · no-op 0,9 s · dist 1,6 s.

> **Nota (v2.1.0, oversampling):** la baseline de respuesta del **sumatorio de bandas** (`+1,00 dB @1 kHz`,
> `−2,06 dB @2,5 kHz`) **queda obsoleta cuando el oversampling está activo**. Los crossovers se diseñan a
> `fs · factor`, no a `fs`, así que no aplica a 2x/4x/8x y **debe re-medirse con el factor por defecto
> (4x)**. Sólo sería comparable en 1x. La THD aislada de `tanh` no cambia, pero cualquier medida de la
> cadena completa (THD, respuesta, alias) debe rehacerse a 4x. Ver `specs/oversampling_spec.md` §5–§8.

### C-8 Criterios de aceptación (verificables)
1. `make clean && make` → 0 errores, 0 warnings; `make dist` produce el `.vcvplugin`.
2. En Rack, patchear CV en el jack bajo la etiqueta "PAN" mueve el **pan** de esa banda (no el vol) —Cvisto en W1, tabla C-3.
3. Render con overlay: ningún controlWidgets se separa >2 px del centro de su elemento de artwork (excepción actual a verificar: pareja OUTPUT, posible deriva horizontal de 2–8 px).
4. Si se implementa bypass: señal seca idéntica bit a bit con bypass activo (o documentar que no existe).
5. Cualquier cambio de DSP re-mide THD y la respuesta de bandas y actualiza la tabla de baselines.

### C-9 Fuera de alcance (esta iteración)
Polifonía, crossovers editables por usuario, telemetría/make-up gain, lights rediseñadas con LED en el panel, submit a VCV Library, CI multi-plataforma (Linux/macOS).

---

## 5. Resumen para el orquestador

1. **Prioridad 1 (bug real, 10–15 min):** las 6 entradas CV están mapeadas al revés respecto al panel (3 VOL luego 3 PAN, en vez de PAN/VOL por banda). Un solo fichero, cambio mecánico. → **W1**.
2. **Prioridad 2 (higiene DSP, ~1 h):** filtros en `onSampleRateChange`, DC-blocker, escalado CV unificado, mezcla de igual potencia. → **W2**.
3. **Prioridad 3 (build, 15–25 min):** documentar/ automatizar el requisito de `jq` (build roto hoy sin `PATH=/mingw64/bin`). → **W5**.
4. **Prioridad 4:** sección CV del panel sin artwork (W4, 1–2 h de diseño) y luces declaradas pero inexistentes (W3).
5. **Antes de paralelizar escritores de C++:** hacer W0.5 (mover el widget a `src/SaturatorWidget.cpp`, 10–15 min) o asignar un único escritor a todo el C++.
6. Metadata: licencia MIT truncada (W6), README con deriva (W5), asset `Knob_32px.svg` sin uso.
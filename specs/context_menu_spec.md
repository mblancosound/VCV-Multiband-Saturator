# Spec: Menú contextual v2.3.0 — Solo / Copy Curve / Info

Estado: DISEÑO CERRADO (arquitecto). Base: v2.2.1 commit `ee52b1e`.
Contrapartes: `specs/curves_spec.md` (curvas/TRIM), `specs/oversampling_spec.md` (latency/aliasing).
No duplica ninguna de las dos: aquí solo se define el estado `solo`, el menú y la info.

## 1. Objetivo

Añadir al menú contextual del módulo, **sin params nuevos, sin tocar panel/SVG/enums**:

1. **Solo por banda** (Off/Low/Mid/High), persistente en JSON, respetando MIX.
2. **Copiar curva entre bandas**: Low→all, Mid→all, High→all.
3. **Info de solo lectura**: sample rate, factor de oversampling, latencia en muestras, versión.

## 2. Fuera de alcance

- Params/Puertos/Luces nuevos (enums intactos).
- `res/` (SVGs).
- Rampa/crossfade en el cambio de curva (solo se aplica a Solo; el click de curva sigue aceptado, ver `README.md` §Curves).
- Bypass/estado "transparente" (C5) y ley de drive (C6): fuera de alcance, no se toca `specs/multiband_saturator_spec.md` (sus filas C5/C6 no aplican a esta feature).
- CC/NoUndoManager: en v2.3.0 los setters del menú no eran undoables (coherente con curvas y
  oversampling en 2.2.1). **Estado actual**: desde v2.4.0 sí lo son, y v2.5.0 añade el toggle de
  Link curves; ver `undo_spec.md` y `link_curves_spec.md`.

## 3. Decisiones (cerradas)

| #  | Decisión | Justificación |
|----|----------|---------------|
| D1 | Solo real **respetando MIX**: dry a 0, bandas no elegidas a 0, la banda elegida pasa por el MIX actual. MIX=0 ⇒ silencio. | Cerrado por el usuario. |
| D2 | Rampa lineal de **5 ms por muestra base** (`step = args.sampleTime * 200.f` = 240 pasos a 48 kHz), actualizada **una vez por `process()` antes de `processBands`**, constante en las N submuestras del bloque. | 5 ms es estándar para fades anticlick (inaudible, ~1/2 ciclo de 100 Hz), y el costo O(1)/muestra. Orden: antes de `processBands` para que las ganancias de TODAS las submuestras del bloque raízen del mismo valor → no hay artefacto al cambiar de stage (la rampa vive a freq. base, independiente del oversampling activo). |
| D3 | Ganancia aplicada a `lowOutL/R`, `midOutL/R`, `upperOutL/R` (`Saturator.cpp:249-256`) **después** de pan/vol y **antes** de la suma (258-259). Los medidores (`Saturator.cpp:261-274`) usan `wetLowL/wetMidL/wetUpperL` (pre-pan, pre-solo) → **siguen midiendo saturación pre-solo**. Confirmado por lectura: `sat*Meter` referencia `wet*(...)` y `curveDrive(...)`, no `*Out*`. | Aislamiento de la función "medidor". |
| D4 | Dry: `outL = dryL * (1-mix) * dryGain + wetL * mix` (y R, `Saturator.cpp:365-367`). Con MIX=0 y Solo activo: `outL = dryL * dryGain` → 0 exacto tras la rampa (y el residual de rampa decae en ≤5 ms). | Cerrado por el usuario. |
| D5 | Miembros: `std::atomic<int> solo{-1};`, `float soloGain[3] = {1.f,1.f,1.f};`, `float dryGain = 1.f;` en `Saturator.hpp` tras `curves[3]`. Solo flujo atómico (escrito por UI/JSON vía `setSolo(int)`, leído con `memory_order_relaxed` en audio). | Mismo patrón que `curves[]`. No requiere `pending*` porque la rampa hace de transición suave; no hay realloc/init caro. |
| D6 | JSON: escribir `"solo"` (-1|0|1|2); leer con validación (no-integer ≠, fuera de -1..2 ⇒ -1; falta la clave ⇒ -1). Setter `setSolo(int)` (valida y store), espejo de `setCurve()`. | Consistencia; también lo usa el menú. |
| D7 | Inicialización al cargar patch con Solo activo: **`soloGain`/`dryGain` quedan en 1 y la rampa baja al objetivo (5 ms)**. No se fuerza estado en `dataFromJson`. | Cumple "arrancar desde el estado actual": la banda elegida mantiene gain 1 (sin click), el dry y las demás bajan con rampa. Forzar ganancias a 0 en `dataFromJson` crearía un hueco UI→audio incontrolado. |
| D8 | UI: submenú `Solo` con `{"Off","Low","Mid","High"}` vía `createIndexSubmenuItem` (helpers.hpp:369); mapeo index 0↔-1, 1↔0, 2↔1, 3↔2 (explícito, sin aritmética oculta). | Coherente con Oversampling/Curves; 4 entradas ya son un índice. |
| D9 | Copy Curve: **submenú** `Copy Curve` con 3 items (`Low to all`, `Mid to all`, `High to all`) vía `createSubmenuItem` (helpers.hpp:337) + `createMenuItem` (helpers.hpp:203/221). Copia `curves[src]` a las 3 bandas con `setCurve()`; **sin JSON nuevo** (usa `"curves"`), **sin rampa** (el click de curva está aceptado, igual que el selector manual). | Menú superior más compacto y agrupación semántica; evita copias accidentales de un click. Los 3 items planos añadirían ruido a un menú que ya tiene 4 secciones. |
| D10 | Info: `createMenuLabel(string::f("%.1f kHz | %dx | %d samples | v%s", saturator->sampleRate * 0.001f, saturator->oversampling, (oversampling>1 ? OS_LATENCY : 0), pluginInstance->version.c_str()))`**al final** del menú, **con `new ui::MenuSeparator` antes**. | `sampleRate` es público (`Saturator.hpp:95`); `oversampling` solo lo escribe `applyOversampling()` en el hilo de audio (invariante 2.2.1) y el menú ya lo lee (Widget.cpp:105). `%dx` con 1/2/4/8 imprime `1x/2x/4x/8x` (equivale al formato "%s"). Separador: sí, para separar visualmente de la sección de curvas. |
| D11 | Versión **2.3.0** en `plugin.json`, README y nombre de dist (auto derivado por `jq` en el Makefile; no se edita el Makefile). | Cerrado por el usuario. |

## 4. Contrato Solo

- Estado: `solo` ∈ {-1, 0, 1, 2} (-1 = Off, 0 = Low, 1 = Mid, 2 = High), atómico `relaxed`.
- Objetivos por muestra base (en `process()`, antes del switch de oversampling):

  ```
  target[b]     = (soloBand < 0 || soloBand == b) ? 1.f : 0.f
  targetDry     = (soloBand < 0) ? 1.f : 0.f
  paso          = args.sampleTime * 200.f        // 0.004167 a 48 kHz
  soloGain[b]  += clamp hacia target[b] (lineal, un paso por muestra)
  dryGain      += clamp hacia targetDry
  ```

- Aplicación:
  - Bandas (`processBands`): `lowOutL = wetLowL * p.volLow * min(1,1-panLow) * p.soloGainLow` (ídem mid/high, el resto no cambia).
  - Dry (`process()`): `outL = dryL*(1-mix)*dryGain + wetL*mix`.
  - Ganancias viajan en `OSParams` como 3 floats nuevos (`soloGainLow`, `soloGainMid`, `soloGainHigh`); el dry usa el miembro `dryGain` del módulo en `process()`.
- Invariante C: el coeficiente de la rampa no depende del oversampling (se actualiza a freq. base); cambiar 1x↔8x *mientras* suena el Solo no produce discontinuidad propia (el reset de filtros ya produce click, documentado en oversampling_spec).
- JSON roundtrip: `dataToJson()` escribe `solo.load()`; `dataFromJson()` valida -1..2 (default/clamp -1) y llama `setSolo()`.
- Hilo: `solo` escrito por UI/JSON (`setSolo`), leído por audio cada muestra base. `soloGain[]`/`dryGain` son privados del audio.

## 5. Contrato Copy Curve

- Origen válidos: src ∈ {0,1,2} (Low/Mid/High). Acción: `setCurve(b, curves[src].load())` para b = 0,1,2.
- No toca `"solo"` ni los medidores. Sin rampa (click aceptado).
- Errores: `setCurve` ya clampa banda/id; no hay path de fallo nuevo.

## 6. Contrato Info

Formato exacto: `%.1f kHz | %dx | %d samples | v%s`, p.ej. `48.0 kHz | 1x | 0 samples | v2.3.0`.
- sample rate: `saturator->sampleRate / 1000` (float público).
- factor: `saturator->oversampling` (1/2/4/8 — invariante).
- latencia: `0` a 1x, `coaldynamics::OS_LATENCY` (=15) a 2x/4x/8x.
- versión: `pluginInstance->version.c_str()` (Plugin.hpp:36), mantiene `#include "plugin.hpp"`.

## 7. Plan de ficheros

| Fichero | Cambio | Disjunto con |
|---|---|---|
| `src/Saturator.hpp` | 3 hunks: OSParams (+3 floats), estado solo + setSolo decl | cpp/widget/docs |
| `src/Saturator.cpp` | 6 hunks: setSolo impl, rampa en process(), ganancias en processBands, dryGain en mix, dataToJson, dataFromJson | hpp/widget/docs |
| `src/SaturatorWidget.cpp` | 1 hunk: appendContextMenu (Solo, Copy Curve, Info) | hpp/cpp/docs |
| `plugin.json` | "version": "2.2.1" → "2.3.0" | todos |
| `README.md` | versión, features, sección nueva de menú, filename dist | todos |
| `specs/context_menu_spec.md` | este fichero (nuevo) | — |

`res/` intacto. `specs/multiband_saturator_spec.md` INTACTO (C5/C6 no aplican).

## 8. Tabla de casos / criterios de aceptación verificables

| # | Caso | Criterio | Verificación |
|---|------|----------|--------------|
| 1 | Build | 0 errores/0 warnings (`-Wall -Wextra`) | `make clean && make`, salida vacía de warnings |
| 2 | Solo Low, MIX=1, seno 1 kHz por las 3 bandas | Solo pasa la banda Low; bandas Mid/High y dry mudos | Scope/Recorder en Rack |
| 3 | Solo Low, MIX=0.5 | Nivel ≈ 0.5 del caso MIX=1 y **sin componente dry** (null contra salida MIX=0 separada) | Recorder + resta de buffers |
| 4 | Solo Low, MIX=0 | Silencio (exacto tras la rampa, restos solo en los primeros ~240 samples) | Recorder, `max(abs(y)) == 0` tras 5 ms |
| 5 | Rampa sin click | Max salto muestra-a-muestra ≤ pendiente de señal + `amplitud/240` (≈0.021 V @5 V/48 kHz) | Recorder antes/después de togglear Solo; script de diferencias por muestra |
| 6 | JSON roundtrip | Guardar/cargar conserva `"solo"`; selector de menú muestra el mismo estado | Guardar patch, recargar, menú |
| 7 | JSON sin clave | Carga a Off (-1) | Patch v2.2.1 existente |
| 8 | JSON inválido | `7`, `-3`, `"x"` ⇒ Off | Editar el .vcv a mano |
| 9 | Copy Low→all (y Mid/High) | Los 3 submenús de curva muestran la misma curva; `"curves"` JSON = [id,id,id] | Menú + patch JSON |
| 10 | Info | Texto exacto `48.0 kHz | 1x | 0 samples | v2.3.0` a 48k/1x (y `15 samples` a 4x) | Menú en Rack |
| 11 | Enums intactos | `git diff` sin cambios en ParamIds/InputIds/OutputIds/LightIds | `git diff src/Saturator.hpp` |
| 12 | Sin allocator/lock en process() | No hay `std::string`, `new`, ni mutex en `process()`/`processBands()` (la math del ramp es branch/min/max) | Revisión de código + profiling opcional |
| 13 | Medidores | Las 6 luces de saturación siguen respondiendo a su banda **aunque Solo la silencie** | Rack: Solo en una banda y ver la luz de otra saturar |

**Estado de verificación (2026-10-27).** Verificados: **1** (build 0 errores / 0 warnings), **11** y
**12** (diff de enums y revisión de `process()`). En Rack, con osciloscopio, la misma señal a 1x y a 4x
no muestra diferencia de nivel ni de forma de onda: eso cierra la alineación dry/wet junto con la
medición por impulso de `oversampling_spec.md` §5 (15 muestras exactas a 2x/4x/8x, 0 a 1x).
**Pendientes de prueba audible en Rack: 2–10 y 13.**

## 9. Riesgos

| Riesgo | Mitigación |
|---|---|
| Bleed durante la rampa: durante ~5 ms la mezcla solo/normal es parcial (no hay crossfade duro). | Aceptable: es un fade lineal, contenido < 5 ms. |
| DC blocker memoriza el camino wet anterior; al un-solo puede haber un transitorio de transición ≤ amplitud de rampa. | Cubierto por la propia rampa (D2); cubierto por el caso 5. |
| Copiar curvas con Solo activo: puede "clickear" (los clicks de curva están aceptados). | Documentado como en v2.2.1 (README §Curves); no rampa. |
| Lectura de `oversampling`/`sampleRate` desde UI en label Info | Mismo patrón aceptado ya en `SaturatorWidget.cpp:105` (lectura de `oversampling`). |
| `solo` no atómico no — se usa `std::atomic<int>` + `relaxed`, idéntico a `curves[]`. | Sin riesgo. |
| El usuario espera "Solo" tradicional **ignorando MIX**; aquí respeta MIX (decisión D1). | Documentado README §Solo ("Solo respects MIX"). |

## 10. Timings / M-cycle (estimados, verificación = README timings)

Make timings medidos en README (v2.2.1): limpio 7.2 s, incremental **2.7 s por fichero**, no-op 0.9 s, `make dist` 1.6 s.
Con 2 ficheros tocados (hpp + cpp compilan juntos) y dependientes: recompilación ~5.4 s + link; widget.cpp +1 → ~8 s.
Ciclo completo de cambio (ediciones D3 + build + instal manual en Rack + tests manuales de la tabla §8): ~10-15 min dominado por pruebas en Rack, no por compilación.

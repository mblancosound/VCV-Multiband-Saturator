# Spec — Undo (Ctrl+Z) de las acciones del menú contextual (v2.4.0)

Base: v2.3.0 (`44c7003`). Prototipo validado compilando el repo copiado en
`%TEMP%\opencode\undo_proto` con `make RACK_DIR=/c/Rack-SDK2.6.6`:
**0 warnings / 0 errores** con los flags del proyecto (`-Wall -Wextra
-Wsuggest-override`).

## Objetivo

Que las cuatro acciones del menú contextual (cambio de curva, Copy Curve, Solo,
Oversampling) sean deshacibles/rehacibles con Ctrl+Z / Ctrl+Shift+Z usando el
historial estándar de Rack (`History`, la misma lista que "Undo move module" etc.).

## Alcance

- `src/SaturatorWidget.cpp`: acciones como clases en `namespace { }` + helpers de
  registro + setters de los menús.
- `src/Saturator.hpp` y `src/Saturator.cpp`: **una única corrección de
  concurrencia** en `pendingOversampling` (ver «Corrección de la carrera»).
- Bump de versión a **2.4.0** en `plugin.json` (y frase en README) junto con el
  cambio de código.

## Fuera de alcance (cerrado)

- Link curves, mute por banda, DC blocker, reset, cualquier cambio de DSP/params/
  JSON/enums/SVG. `process()` solo cambia **cómo consume** `pendingOversampling`
  (ver «Corrección de la carrera»); el audio no cambia. Ningún miembro nuevo en
  `Saturator`: `moduleId`, `oldValue` y `newValue` viven en la acción.
- `dataFromJson()` no registra historial: no hay ningún `push` fuera de los
  lambdas del menú.
- El historial de Rack no se serializa con el patch (verificado:
  `history::State` no tiene `dataToJson`/`fromJson`; solo exporta nombres como
  `getUndoName()`), así que **no se necesita `dataToJson()` en las acciones**.
  Al recargar un patch el historial sigue existiendo en la sesión, igual que con
  los cambios de params nativos de Rack.

## Hechos de API verificados

| Hecho | Fuente |
| --- | --- |
| `history::Action{ name, undo(), redo() }`; `name` se usa en "Undo …" | `C:\Rack-SDK2.6.6\include\history.hpp` 31–37 |
| `history::ModuleAction : Action` con `int64_t moduleId` (sin punteros a módulo: pueden quedar colgando) | history.hpp 63–68 |
| `history::State::push(Action*)` **no ejecuta la acción**, solo registra (y trunca el redo). Patrón: aplicar el cambio y luego `push()` | `src/history.cpp` v2.6.6 (GitHub), `void State::push` |
| Undo/Redo nativos de Rack resuelven el módulo por ID con `APP->engine->getModule(moduleId)` + guard `if (!module) return;` | `ParamChange::undo/redo`, `ModuleBypass::undo/redo` en history.cpp |
| `Module *Engine::getModule(int64_t moduleId)` | `include\engine\Engine.hpp` 122 |
| `Module::id` público (`int64_t id = -1`) | `include\engine\Module.hpp` 40 |
| `APP->history` es `history::State*` | `include\context.hpp` 50 |
| Ctrl+Z/Ctrl+Shift+Z se resuelven en `Scene::onKey` → `State::undo()/redo()` | Rack src (escena/CtrlBindings), comportamiento estándar |

## Decisiones cerradas y justificación

**A) Cuatro acciones, no una genérica** (`CurveAction`, `CopyCurveAction`,
`SoloAction`, `OversamplingAction`): cada una son ~12 líneas de complemento por
método; una genérica necesitaría `std::function` capturando el puntero del
módulo —prohibido por el diseño de `history.hpp`— o un enum-dispatching opaco.
Cuatro clases explícitas son ~90 líneas, legibles y sin capturas colgantes.

**B) El estado previo se captura en el helper del menú**, leyendo el atómico
ANTES de llamar al setter, y comparando con el destino. Solo el hilo de UI
escribe `curves`/`solo` desde el menú, así que la secuencia leer→comparar→
escribir→push es atómica en la práctica. El valor capturado antes = valor real
de "antes".

**C) Copy Curve = UNA acción con `int oldCurves[3]` + `int newCurve`** (no
`ComplexAction`). Más directa: 1 entrada de historial garantizada, se integra
de forma natural con la regla "no-op si nada cambia" (ComplexAction requerirría
evaluar `isEmpty()` y gestionar un undone parcial), y su undo es trivial
(escribir los 3 identificadores antiguos) sin depender del orden inverso de
`ComplexAction::undo()`.

**D) undo/redo resuelven el módulo por `moduleId` en cada llamada**:
`dynamic_cast<Saturator*>(APP->engine->getModule(moduleId))`; si es NULL,
no-op silencioso (mismo guard que `ParamChange::undo()`). El dynamic_cast es
defensivo (el ID solo lo registran módulos Saturator, Rack no reutiliza ids),
pero es barato y simétrico con el constructor del widget.
`getModule()` de Engine hace lock correcto desde el hilo de UI.

**E) No hay `dataToJson()` en las acciones**: `history::State` no serializa
acciones al guardar el patch (el `json_t*` de `history.hpp` es solo de
`ModuleAdd`). Confirmado.

**F) Nombres (salen literales en "Undo …")**:
- `"change curve"` → "Undo change curve" (consistente con "Undo change parameter" de Rack).
- `"copy curve"` → "Undo copy curve".
- `"solo band"` → "Undo solo band".
- `"change oversampling"` → "Undo change oversampling".

**G) El check del submenú tras undo**: basta con que el getter lea el atómico.
Rack llama al getter cada vez que se abre el submenú, que es cuando el usuario
puede ver el check. No se necesita `menu->close()` ni refresco.

**H) Undo de Oversampling reutiliza `setOversampling()`** (solo deja
`pendingOversampling` pendiente; DSP sin tocar) **más un helper
`requestOversampling()`** que resuelve un caso de orden dentro del hilo de UI: si
el undo llega ANTES de que el hilo de audio aplique la petición,
`setOversampling()` haría early-return (`factor == oversampling`) y la petición
antigua sobreviviría; el helper cancela la petición pendiente
(`pendingOversampling.store(0)`) cuando el factor pedido ya es el aplicado.
`pendingOversampling` es un campo público documentado en `Saturator.hpp` como
"lo escribe la UI o el JSON". La atomicidad del campo **no** la da este helper:
la da `Saturator` (ver «Corrección de la carrera»).

**I) Sin entrada si el valor no cambia**: se detecta en el helper antes de
escribir (comparando valor capturado vs destino; en oversampling se usa el
valor *efectivo*: `pendingOversampling != 0 ? pendingOversampling :
oversampling`). Escoger la opción ya activa queda en literalmente no-op
(sin escritura ni entrada de historial).

## Contrato por acción

Cada menú llama a un helper `pushXxxHistory(Saturator*, …)` que hace exactamente:
1. Captura el valor(es) actual(es). 2. Si el destino es igual → return (no-op).
3. Aplica el cambio vía los setters públicos de `Saturator`.
4. Registra la acción (`new XxxAction` + `moduleId = saturator->id` + old/new)
   en `APP->history->push(action)`.

| Acción | name (en "Undo …") | old | new | undo = | redo = |
| --- | --- | --- | --- | --- | --- |
| Low/Mid/High Curve | `change curve` | `curves[band]` | índice elegido | `setCurve(band, old)` | `setCurve(band, new)` |
| Copy Curve | `copy curve` | `curves[0..2]` | id de banda origen (1 solo valor) | `setCurve(b, oldCurves[b])` ×3 | `setCurve(b, newCurve)` ×3 |
| Solo | `solo band` | `solo` (-1..2) | índice−1 | `setSolo(old)` | `setSolo(new)` |
| Oversampling | `change oversampling` | factor efectivo | factor | `requestOversampling(old)` | `requestOversampling(new)` |

Invariante: después de un undo, el estado del módulo queda idéntico a antes de la
acción y el histórico es coherente con `isSaved()` de Rack (Rack gestiona el
indicador "unsaved changes").

## Plan de ficheros

| Fichero | Unidad de trabajo | Operación |
| --- | --- | --- |
| `src/SaturatorWidget.cpp` | Acciones + helpers en `namespace { }` tras los includes; `appendContextMenu()` entera. | modificar |
| `src/Saturator.hpp` | `pendingOversampling` → `std::atomic<int>` | modificar |
| `src/Saturator.cpp` | `process()` (exchange), `setOversampling()` (store), guarda de validación en `applyOversampling()` | modificar |
| `plugin.json` | bump `"version"` a `2.4.0` | modificar |
| `README.md` (ya actualizado en este commit de specs) | frase Ctrl+Z + nota "not undoable" de Curves | hecho |

Disyunción: `SaturatorWidget.cpp` y `Saturator.hpp`/`Saturator.cpp` son paquetes
de ficheros disjuntos; el `.hpp` y el `.cpp` se serializan entre sí por ser la
misma unidad de trabajo.

## Corrección de la carrera de `pendingOversampling`

Detectada por J1 al revisar esta tanda. `pendingOversampling` era un `int` plano
escrito desde el hilo de UI y consumido desde el hilo de audio:

```cpp
if (pendingOversampling != 0) {      // hilo de audio
  int factor = pendingOversampling;
  pendingOversampling = 0;           // puede pisar un store de la UI
  applyOversampling(factor);
}
```

Dos fallos posibles: (a) si la UI escribe entre la lectura y el `= 0`, la
petición se pierde; (b) si el audio lee el campo justo cuando vale 0 pese a
haber una petición en vuelo, `applyOversampling(0)` deja `fs = 0` en
`updateFilterFrequencies()` → `250.f / 0` → inf → NaN propagado por los biquads.

Arreglo mínimo, sin cambio de audio ni de JSON:

| Fichero | Cambio |
| --- | --- |
| `Saturator.hpp` | `std::atomic<int> pendingOversampling{0}` (mismo contrato público: la UI y el JSON lo escriben) |
| `Saturator.cpp` `process()` | consume con `pendingOversampling.exchange(0, std::memory_order_relaxed)`: un store de la UI cae antes o después, nunca se pierde ni se lee a medias |
| `Saturator.cpp` `setOversampling()` | `pendingOversampling.store(factor, std::memory_order_relaxed)` |
| `Saturator.cpp` `applyOversampling()` | guarda `if (factor != 1 && factor != 2 && factor != 4 && factor != 8) return;` como defensa contra `fs = 0` |
| `SaturatorWidget.cpp` | `.load()` / `.store()` en `pushOversamplingHistory()` y `requestOversampling()` |

El factor se sigue aplicando al principio del siguiente bloque y
`applyOversampling()` no cambia su cuerpo: no hay diferencia audible ni de
latencia (sigue 15 muestras a 2x/4x/8x).

## Tabla de casos verificables (manual, con plugin.dll de la build)

| # | Acción | Esperado |
| --- | --- | --- |
| 1 | Cambiar Low Curve Tanh→Hard, Ctrl+Z | Vuelve a Tanh; lema "Undo change curve"; Ctrl+Shift+Z lo rehace. Check del submenú OK al reabrir. |
| 2 | Copy Curve → "Low to all" (Mid/High distintas), Ctrl+Z UNA vez | Las 3 bandas vuelven a sus valores en un solo paso (no 3). |
| 3 | Solo Off→Low, Ctrl+Z | vuelve a Off, rampa 5 ms audiblemente limpia; rehacer vuelve a Low. |
| 4 | Oversampling 1x→8x, esperar ~1 s (aplicado), Ctrl+Z | Vuelve a 1x aplicada; siguiente bloque sin glitch raro (resetea filtros, click aceptado como hoy). |
| 5 | Oversampling 1x→8x + Ctrl+Z MUY rápido (antes del bloque) | No queda pendingOversampling huérfano: no cambia a 8x. |
| 6 | Reelegir la opción ya activa en cualquiera de los 4 menús | Sin entrada nueva (el siguiente Ctrl+Z deshace el paso anterior, no "nada"). |
| 7 | Cargar un patch no deshace nada / Ctrl+Z tras cargar | No rompe; sin entradas nuevas registradas. |
| 8 | Borrar el módulo con cambios en el historial, Ctrl+Z | No crash. El undo de "add module" de Rack gobierna. |
| 9 | `make` | 0 warnings con `-Wall -Wextra` (gate del repo). |

**Estado de verificación (2026-10-27).** Los 9 casos se han probado en Rack 2.6.6 con el build de
v2.4.0 y funcionan sin error aparente, incluido el caso 5 (Ctrl+Z antes de que el audio aplique el
cambio): el módulo se queda en 1x y no queda ninguna petición huérfana. El caso 9 lo cubre el gate
automático (`make clean && make` → 0 warnings / 0 errores).

## Riesgos

- **Carrera de `pendingOversampling`** (caso 5): **corregida** en esta tanda con
  `std::atomic<int>` + `exchange(0)` (ver «Corrección de la carrera»).
  `requestOversampling()` solo resuelve el orden dentro del hilo de UI (cancelar
  una petición que aún no se ha aplicado); no aporta atomicidad.
  Riesgo residual: si `Saturator::setOversampling()` cambia su semántica, revisar helper.
- **Interacción con el redo truncado**: `push()` borra las acciones futuras tras
  undo/redo alternados; es el comportamiento estándar de Rack (igual que con los
  params nativos). Comportamiento esperable para el usuario.
- **Perf**: registrar una acción es O(1) y solo al click; sin coste DSP añadido.
  `process()` solo cambia el consumo de `pendingOversampling` (ver «Corrección de
  la carrera»); el resto del camino de audio queda igual.

## Checks finales para el agente implementador (antes del commit)

1. `make clean && make` → 0 warnings / 0 errores (sin tocar `Saturator.cpp/.hpp`).
2. `git diff -- src/Saturator.cpp src/Saturator.hpp` → vacío.
3. Los 9 casos de la tabla, con el `plugin.dll` fresco en Rack 2.6.6.
4. Bump `plugin.json` a `2.4.0` y compilar `make dist` (el nombre del .vcvplugin
   usa la versión del json).

# Spec — Link curves (v2.5.0)

Base: v2.4.0 (`5483fb4`). Prototipo validado compilando el repo copiado en
`%TEMP%\opencode\link_proto` con `make RACK_DIR=/c/Rack-SDK2.6.6` (MSYS2):
**0 warnings / 0 errores** con los flags del proyecto (`-Wall -Wextra
-Wsuggest-override`). El prototipo es desechable y fuera del repo.

## Objetivo

Un modo **Link curves** en el menú contextual: mientras está activo, cambiar la
curva de una banda (Low/Mid/High, cualquiera de los tres selectores) cambia la
de las tres a la vez, con UNA sola entrada de undo que restaura las tres bandas.

## Alcance

- `src/SaturatorWidget.cpp`: toggle del menú (item con check) + `pushCurveHistory()`
  que, con el modo activo, escribe las 3 bandas y registra `MultiCurveAction` a
  través de `applyCurvesAndPush()`.
- `src/Saturator.hpp` + `src/Saturator.cpp` (misma unidad): miembro
  `bool linkCurves` + clave JSON `"linkCurves"`.
- `plugin.json`: bump a **2.5.0**. `README.md`: frase del modo (mismo commit).

## Fuera de alcance (cerrado)

- Nada de DSP, params, enums, SVG ni threading: `linkCurves` lo escriben la
  UI y el JSON (hilo de UI), el hilo de audio nunca lo lee → `bool` plano,
  sin atómico (mismo criterio que el resto de estado solo-UI).
- Copy Curve, Solo y Oversampling no cambian de comportamiento (Copy Curve
  sigue usando su propia acción y helper, con lema "copy curve").
- El toggle **sí** es undoable ("toggle link curves", C) y **sí** se guarda con
  el patch (F).
- No se toca `specs/undo_spec.md` ni `specs/context_menu_spec.md`.

## Hechos de API verificados (SDK 2.6.6 real, no inventado)

| Hecho | Fuente |
| --- | --- |
| `createBoolMenuItem(text, rightText, getter, setter, ...)` existe y es el wrapper getter/setter con checkmark (delega en `createCheckMenuItem`, que añade `CHECKMARK_STRING` al `rightText`) | `C:\Rack-SDK2.6.6\include\helpers.hpp` 299–304 y 253–284 |
| Firma exacta: `template <class TMenuItem = ui::MenuItem> TMenuItem* createBoolMenuItem(std::string text, std::string rightText, std::function<bool()> getter, std::function<void(bool state)> setter, bool disabled = false, bool alwaysConsume = false)` | helpers.hpp 300 |
| Payload `MultiCurveAction`: `int oldCurves[3]` + `int newCurve`; undo/redo escriben las 3 bandas vía `setCurve()` | `src/SaturatorWidget.cpp` 76–97 |
| `history::Action::name` es `std::string` **público**: se puede reasignar antes del `push()` | `include\history.hpp` (~31–37, campo público de `Action`) |
| `history::State::isSaved()` compara `actionIndex` con `savedIndex` (índice de la acción al guardar) | `include\history.hpp` 180, 194 |
| `json_boolean()` / `json_is_boolean()` / `json_boolean_value()` de jansson | compilados en el prototipo (0/0), jansson |
| `dataToJson()` guarda `"curves"`, `"oversampling"`, `"solo"`; `dataFromJson()` valida rangos y no registra historial | `src/Saturator.cpp` 182–230 |

## Decisiones cerradas y justificación

**A) Persistencia: clave nueva `"linkCurves"` con default off.** En
`dataToJson()`: `json_object_set_new(rootJ, "linkCurves", json_boolean(linkCurves));`.
En `dataFromJson()`: solo exactamente `true`/`false` JSON cambia el modo;
**ausente**, **no booleano** (`"x"`, string `"true"`, `1`) → **off** (mismo
estilo estricto que `"solo"`). Un patch v2.4.0 no trae la clave → carga con el
modo off, **sin cambios** en curvas/sonido. Nombre en camelCase, como el resto
de claves compuestas del proyecto.

**B) Activar el toggle NO copia Low→Mid/High.** El setter llama a
`pushLinkCurvesHistory(saturator, state);` — que solo escribe `linkCurves` y
registra el toggle — (`state` es el estado nuevo que
`createBoolMenuItem` pasa — ver contrato del check más abajo). Justificación:
activar un modo no debe destruir dos curvas sin petición explícita del usuario;
para eso existe **Copy Curve** (todavía en el menú). El link solo gobierna los
cambios *posteriores*; tras el primer cambio enlazado las 3 igualan, así que la
asimetría solo existe hasta el primer uso. Se documenta en el comentario del
item y en README.

**C) El toggle entra en el historial de undo.** Revisado respecto a la primera
versión de esta spec. El criterio de v2.4.0 es «todo lo que el menú cambia y se
guarda con el patch es undoable», y `linkCurves` se guarda. Dejarlo fuera tenía
dos costes: (a) incoherencia con los otros cuatro items del menú y (b) el
indicador de cambios sin guardar no se movería, porque Rack deriva `isSaved()` de
`history::State` (`actionIndex` vs `savedIndex`): cerrar sin guardar perdería el
modo sin avisar. Acción propia `LinkCurvesAction` (`oldState`/`newState` bool,
name **"toggle link curves"**) registrada por `pushLinkCurvesHistory()`. El
historial es lineal: un Ctrl+Z deshace el último gesto, sea un cambio de curvas
o el propio toggle, igual que "Undo bypass module" de Rack. Alcance del link:
**solo los tres selectores de curva**; Copy Curve, Solo y Oversampling
conservan sus helpers y lemas exactos.

**D) Undo de un cambio enlazado: UNA entrada que restaura las tres bandas.** La
postcondición de undo/redo es idéntica a la de Copy Curve (restaurar
`oldCurves[3]` / escribir `newCurve` a las 3), así que no hay clase nueva:
`CopyCurveAction` se renombró a **`MultiCurveAction`** y ambos caminos pasan por
el helper **`applyCurvesAndPush(saturator, oldCurves, newCurve, name)`**, que
concentra el no-op de las tres bandas y el `push()`. El lema se pasa por
parámetro desde el call site: `"copy curve"` (Copy Curve) y **`"change curves"`**
(cambio enlazado). Resultado: "Undo change curves" — no el engañoso "Undo copy
curve", ni el singular "Undo change curve", que ya significa una banda. Un solo
Ctrl+Z restaura las 3 bandas de golpe (potencialmente 3 valores distintos).

**Nota de implementación.** El refactor de J2 dejó `pushCurveHistory` (rama link)
y `pushCopyCurveHistory` en una línea cada uno, con el no-op de las tres bandas
en un único punto de verdad y sin reasignar `name` tras el `new`. El fichero
perdió ~10 líneas netas (de 467 a 457 líneas totales; 404 → 394 no vacías). El
diff exacto está en `git show fbdf30d -- src/`.

**E) UI: item con check via `createBoolMenuItem`, colocado al frente del grupo
de curvas.** Menú queda:

```
Oversampling ▸
────────────────
Link curves   [check]
Low Curve ▸ ...Mid Curve ▸ ...High Curve ▸
────────────────
Solo ▸
Copy Curve ▸
```

Justificación de posición: vive con los tres selectores que gobierna (dentro
del bloque curvas, tras el separador existente de la línea 327); un submenú
"Curves" agrupador movería los tres selectores existentes y rompería hábitos
de la 2.4.0. API elegida en vez de `createMenuItem(label, "On"/"Off")` porque
la API nativa ya dibuja el checkmark (`CHECKMARK_STRING`) y sigue el patrón de
`createIndexSubmenuItem` (que usa checks internamente); verificar contra
helpers.hpp 299–304, **no inventado**. Nota de semántica: el `setter` se llama
con el estado nuevo según `getter` (createBoolMenuItem construye
`setter(!getter())` internamente), así que el lambda pasa ese estado a
`pushLinkCurvesHistory()`, que lo registra como "toggle link curves".

**F) Persistencia e historial: el indicador de cambios funciona.** Al pasar por
`history::State::push()`, activar o desactivar el modo mueve `actionIndex` y el
patch aparece como modificado, exactamente igual que curvas, Solo y
Oversampling. `dataFromJson()` sigue sin registrar historial (regla v2.4.0):
cargar un patch no es una acción de usuario.

**G) Versión: 2.5.0 (minor), no 2.4.1.** Criterio del repo: cada feature
user-facing con cambio visible ha subido minor (2.3.0 menú contextual,
2.4.0 undo); los patch (2.2.1) fueron fixes. Link curves es un modo nuevo con
clave JSON nueva → minor. `make dist` pasa a llamarse
`Coal_Dynamics-2.5.0-win-x64.vcvplugin`.

**H) Plan de ficheros y disyunción** — ver tabla abajo; unidades totalmente
disjuntas por fichero.

## Contrato

1. `Saturator::linkCurves` (`bool` plano, default `false`): lo escriben la UI
   (toggle) y el JSON; el audio no lo lee. El setter del toggle llama a
   `pushLinkCurvesHistory()`: no-op si el estado no cambia (regla v2.4.0).
2. `pushCurveHistory(Saturator*, band, newCurve)` (único punto de las 3 curvas):
   - **link off** (default): comportamiento v2.4.0 exacto — escribe solo
     `band` y push `CurveAction` ("change curve"); no-op si el valor ya es el
     actual.
   - **link on**: llama a `applyCurvesAndPush(saturator, oldCurves, newCurve,
     "change curves")`, que lee las 3 curvas una vez; si las 3 ya son
     `newCurve` → no-op (regla v2.4.0, "sin entrada si no cambia"), y si no,
     escribe las 3 con `setCurve(b, newCurve)` y registra una
     `MultiCurveAction` con `oldCurves[]` por banda.
3. JSON: `"linkCurves"` se escribe SIEMPRE (bool). Entrada estricta:
   solo `json_is_boolean()` cambia el modo; todo lo demás → off. Cargar patch
   no registra historial (regla v2.4.0, intacta).
4. Sin excepción ni cambio en `Saturator.hpp` en la parte de audio: no hay
   atómicos nuevos, no hay callbacks nuevos.

| Entrada de historial nueva | name (en "Undo …") | old | new | undo = | redo = |
| --- | --- | --- | --- | --- | --- |
| Cambio enlazado (link on, cualquier selector) | `change curves` | `oldCurves[0..2]` | id elegido (1 valor, a las 3 bandas) | `setCurve(b, oldCurves[b])` ×3 | `setCurve(b, newCurve)` ×3 |
| Toggle Link curves | `toggle link curves` | `linkCurves` (bool) | estado nuevo | `linkCurves = oldState` | `linkCurves = newState` |

Todo lo demás (tabla de acciones v2.4.0) queda sin cambios.

## Plan de ficheros

| Fichero | Unidad de trabajo | Operación |
| --- | --- | --- |
| `src/Saturator.hpp` | U1 (con `Saturator.cpp`) | añadir `bool linkCurves = false;` + comentario de contrato |
| `src/Saturator.cpp` | U1 (con `Saturator.hpp`) | `dataToJson()` clave `"linkCurves"`; `dataFromJson()` lectura estricta |
| `src/SaturatorWidget.cpp` | U2 (disjunta de U1) | `pushCurveHistory()` extenda + item `Link curves` en `appendContextMenu()` |
| `plugin.json` | U3 | bump `"version": "2.5.0"` |
| `README.md` | U3 (con plugin.json) | frase del modo v2.5.0 + nota del check |

Disyunción: U1 (Saturator.hpp+cpp) y U2 (SaturatorWidget.cpp) no comparten
fichero → escritores en paralelo seguros. U3 no toca código. La spec y el
prototipo (`%TEMP%\opencode\link_proto`) viven solo aquí (agente).

## Implementación final

El código se aplicó en el commit `fbdf30d` y la fuente de verdad es el propio
fichero: este spec no lo duplica para que no pueda desincronizarse.

| Símbolo | Fichero | Función |
| --- | --- | --- |
| `Saturator::linkCurves` (`bool`, default `false`) | `src/Saturator.hpp` | modo; lo escriben la UI y el JSON, el hilo de audio no lo lee |
| `"linkCurves"` en `dataToJson()` / `dataFromJson()` | `src/Saturator.cpp` | se escribe siempre; lectura estricta (`json_is_boolean`), todo lo demás → off |
| `MultiCurveAction` | `src/SaturatorWidget.cpp` | las tres bandas en una entrada; el lema se pasa por parámetro |
| `LinkCurvesAction` | `src/SaturatorWidget.cpp` | toggle del modo (`"toggle link curves"`) |
| `applyCurvesAndPush()` | `src/SaturatorWidget.cpp` | único punto de verdad del no-op de tres bandas y del `push()` |
| `pushCurveHistory()` | `src/SaturatorWidget.cpp` | una banda sin link; tres bandas con link |
| `pushLinkCurvesHistory()` | `src/SaturatorWidget.cpp` | registra el toggle (no-op si el estado no cambia) |
| `pushCopyCurveHistory()` | `src/SaturatorWidget.cpp` | Copy Curve reutiliza el helper con `"copy curve"` |
| `createBoolMenuItem("Link curves", ...)` | `src/SaturatorWidget.cpp` | check + setter con historial |

## Tabla de casos verificables (manual, con plugin.dll de la build 2.5.0)

| # | Acción | Esperado (criterio observable) |
| --- | --- | --- |
| 1 | Menú → activar **Link curves** | El item muestra checkmark; **las curvas no cambian** (Low≠Mid se conserva). Lema del item legible sin submenú extra. |
| 2 | Con link ON, Low Curve Tanh→Hard | Las 3 bandas pasan a Hard; lema **"Undo change curves"**; una sola entrada. |
| 3 | Caso 2, Ctrl+Z una vez | Las 3 bandas vuelven a sus 3 valores distintos originales en un paso; Ctrl+Shift+Z las pone a Hard otra vez. |
| 4 | Con link ON, Mid Curve o High Curve (no solo Low) | El cambio se aplica a las 3 (el modo no distingue origen del selector). |
| 5 | Religar la opción ya activa (las 3 ya valen X y cambias Low a X) | Sin entrada nueva: el siguiente Ctrl+Z deshace el paso anterior, no "nada" (regla v2.4.0). |
| 6 | Desactivar Link curves | Checa off; curvas tal cual estaban (sin mutación). Cambiar Low ahora solo toca Low: "Undo change curve" (singular) y una banda. |
| 7 | Activar Link curves y Ctrl+Z | Lema "Undo toggle link curves"; el modo vuelve a off y las curvas no se tocan. Ctrl+Shift+Z lo reactiva. |
| 8 | Guardar patch con link ON → cerrar → recargar | `"linkCurves": true` en el fichero; el check reaparece. Edición manual a `"linkCurves": "x"` o `1` → carga con link off. Patch sin la clave (v2.4.0) → carga con link off y curvas intactas. |
| 9 | Roundtrip del patch: guardar/recargar sin tocar nada | Curvas, oversampling, solo y link idénticos; sin entrada de historial al cargar (regla v2.4.0). |
| 10 | Borrar el módulo con cambios enlazados en el historial, Ctrl+Z | No crash (guard `dynamic_cast` de `saturatorFromModuleId`, igual que v2.4.0). |
| 11 | `make clean && make` | 0 warnings / 0 errores con `-Wall -Wextra` (gate del repo). |
| 12 | `make dist` con plugin.json 2.5.0 | `dist/Coal_Dynamics-2.5.0-win-x64.vcvplugin` (nombre con la versión del json). |

**Estado de verificación (2026-10-27).** Probados en Rack 2.6.6 con el build de v2.5.0, sin errores:
**1** (activar el modo: check visible y curvas intactas), **2** (cambio enlazado desde Low: las tres
bandas cambian y el lema es "Undo change curves"), **3 parcial** (un solo Ctrl+Z restaura las tres; no
se probó Ctrl+Shift+Z), **6** (desactivar: cada banda vuelve a ser independiente) y **8 parcial**
(guardar con el modo ON y recargar: el check reaparece; un patch v2.4.0 carga con el modo off). El
gate de build (**11**) lo cubre `make clean && make` → 0 warnings / 0 errores, y el nombre del paquete
(**12**) `make dist`. **Pendientes de prueba manual: 3 (Ctrl+Shift+Z), 4, 5, 7, 9, 10 y los valores
inválidos de 8.**

## Riesgos

- **Lema de historial pasado por parámetro** (tras el refactor de J2): `name` se
  fija dentro de `applyCurvesAndPush()` a partir del `const char *` del call site,
  antes del `push()`. Ya no hay reasignación tras `new`. Riesgo residual: si se
  añade un tercer llamador, debe pasar su propio lema; el default del constructor
  (`MultiCurveAction`) es `"change curves"`.
- **Ambigüedad si el usuario activa link estando las curvas distintas**: no
  queda claro qué curva queda hasta el siguiente cambio. Mitigado con B (sin
  copia al activar) + item visible al frente del grupo de curvas.
- **JSONKey**: nombre `"linkCurves"` elegido camelCase, como el resto de claves
  compuestas del proyecto (`"curves"`, `"oversampling"` y `"solo"` son de una
  palabra y no cambian). Si alguien espera otra formación/clave, es un rename
  de 2 líneas + caso 8.
- **Perf**: cero en audio (bool no leído ahí, no atómico nuevo). El helper
  enlazado hace 3 lecturas atómicas por cambio (solo al clickear), O(1).
- **Toggle y cambios enlazados comparten historial lineal**: un Ctrl+Z deshace
  el último gesto (curva enlazada o toggle), no "todo el modo". Es el
  comportamiento estándar de Rack y el que el usuario eligió en v2.4.0; no se
  agrupan gestos.

## Checks del implementador (histórico, previos al commit `fbdf30d`)

Se cumplieron todos en el commit de v2.5.0. El punto 5 se refiere solo a aquel
commit: en la pasada de coherencia documental posterior se añadieron notas de
estado a `specs/undo_spec.md` y `specs/context_menu_spec.md`, sin reescribir su
registro histórico.

1. `make clean && make` → **0 warnings / 0 errores** (tomado como gate).
2. Los 12 casos de la tabla, con el plugin.dll fresco en Rack 2.6.6.
3. `git diff` → solo `Saturator.hpp`, `Saturator.cpp`, `SaturatorWidget.cpp`,
   `plugin.json`, `README.md` (nada más).
4. Bump `plugin.json` a `2.5.0` y compilar `make dist`; verificar el nombre
   del `.vcvplugin`.
5. No reescribir `specs/undo_spec.md` ni `specs/context_menu_spec.md`; los
   lemas "copy curve" y "change curve" NO cambian; solo se añade
   "change curves", que es una entrada distinta.

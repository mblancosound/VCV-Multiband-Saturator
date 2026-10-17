# MICRO-SPEC — Previsualización (tooltip) del knob de PAN (`Multiband_Saturator`)

Iteración: W-PANDISP (complemento de `specs/pan_law_fix_spec.md`, que tocó la ley de paneo pero **no**
la previsualización).
Ficheros de producción a tocar: **`src/Saturator.hpp`** y **`src/Saturator.cpp`** (2 ficheros).
Toolchain verificada: SDK `C:\Rack-SDK2.6.6` (Rack 2.6.6), `g++ -std=c++11 -Wall -Wextra -Wsuggest-override`.

---

## 1. Objetivo, causa raíz y alcance

### 1.1 Objetivo

Que el tooltip de los tres knobs de **PAN** (`LOW_PAN`, `MID_PAN`, `HIGH_PAN`) muestre una etiqueta
estéreo legible y finita en **todo** el recorrido del knob:

- centro exacto ⇒ `C`
- extremo izquierdo ⇒ `50L`, extremo derecho ⇒ `50R`
- resto ⇒ interpolación lineal `|v|·50` redondeado a entero + sufijo `L`/`R` (`v=-0.5 ⇒ 25L`,
  `v=+0.76 ⇒ 38R`); si el entero redondeado es 0 ⇒ `C` (nunca `0L`/`0R`).

### 1.2 Causa raíz (evidencia, ya investigada — no se re-investiga)

`src/Saturator.cpp:11,15,19` declaran los tres pans así:

```cpp
configParam(LOW_PAN,  -1.f, 1.f, 0.f, "Low Pan", "", -1.f, 1.f);   // idem MID_PAN (15) y HIGH_PAN (19)
```

Firma real (`C:\Rack-SDK2.6.6\include\engine\Module.hpp:125`):

```cpp
template <class TParamQuantity = ParamQuantity>
TParamQuantity* configParam(int paramId, float minValue, float maxValue, float defaultValue,
                            std::string name = "", std::string unit = "",
                            float displayBase = 0.f, float displayMultiplier = 1.f,
                            float displayOffset = 0.f)
```

→ el **séptimo argumento es `displayBase`**, no un "rango bipolar". Con `-1.f, 1.f` queda
`displayBase = -1`, `displayMultiplier = 1`, `displayOffset = 0`.

`C:\Rack-SDK2.6.6\include\engine\ParamQuantity.hpp:41-49` documenta la fórmula:

```
displayValue = f(value) * displayMultiplier + displayOffset
  f(value) = value             si displayBase == 0
  f(value) = log_{-displayBase}(value)   si displayBase < 0
  f(value) = displayBase^{value}         si displayBase > 0
```

Con `displayBase = -1` la base es `-(-1) = 1`, luego `log₁(x) = ln(x)/ln(1) = ln(x)/0` ⇒ división por
cero. Medición de esa fórmula degenerada en IEEE-754 precisión simple (equivalente a lo que hace el
SDK, `float`):

| v | ln(v) | ln(1) | display = ln(v)/ln(1) |
|---:|---:|---:|---|
| +1.00 | 0 | 0 | `NaN` (0/0) |
| +0.76 | −0.2744 | 0 | `-inf` |
| +0.50 | −0.6931 | 0 | `-inf` |
| +0.02 | −3.9120 | 0 | `-inf` |
| 0.00 | −inf | 0 | `-inf` |
| −0.02 | `NaN` | 0 | `NaN` |
| −0.50 | `NaN` | 0 | `NaN` |
| −0.76 | `NaN` | 0 | `NaN` |
| −1.00 | `NaN` | 0 | `NaN` |

Es decir: el display es **siempre no finito** en todo `[-1,1]`, exactamente el síntoma reportado
(`inf` en el centro, `NaN` en el resto; el reparto exacto inf/NaN depende de `v`, y el signo de la
`inf` del valor suavizado por el Engine en el momento del hover). El string no puede venir ni del
nombre (`"Low Pan"`) ni de la unidad (`""`), luego proviene del formateo del valor de display, que
Rack compone en `getDisplayValueString()` (`ParamQuantity.hpp:100`, `virtual`). *Nota de
trazabilidad*: el punto exacto donde Rack concatena label + valor + unidad está en el `.cpp` cerrado
del SDK (no se distribuye en `include/`; `include/ui/Tooltip.hpp` sólo declara el struct), por lo que
la atribución al tooltip es inferencia, pero es la única vía por la que un `float` no finito puede
acabar en pantalla.

### 1.3 Alcance exacto

**Se toca, y solo esto:**

| Fichero | Cambio |
|---|---|
| `src/Saturator.hpp` | +1 struct `PanParamQuantity` (derivado de `ParamQuantity`), antes de `struct Saturator;` |
| `src/Saturator.cpp` | −3 / +3 líneas: `configParam` → `configParam<PanParamQuantity>` en los 3 pans (líneas 11, 15, 19) |

**FUERA de alcance — NO tocar (explícito):**

- `SaturatorWidget` (`src/Saturator.cpp:186+`): ni `createParamCentered<Knob20>`, ni `minAngle/maxAngle`,
  ni `box.size`, ni la geometría de la UI.
- `src/plugin.hpp` (`Knob20`, `Knob48`, `CustomInputPort`, `SvgScrew`…), `src/plugin.cpp`.
- `res/` (SVG, textos del panel), `Makefile`, `plugin.json`, `README.md`, `LICENSE.txt`.
- `specs/multiband_saturator_spec.md` y `specs/pan_law_fix_spec.md` (esta iteración no los modifica).
- Los `ParamIds` / `InputIds` / `OutputIds` / `LightIds` de `Saturator.hpp` (están persistidos en los
  patches) y su orden.
- El **rango** (`-1..1`) y el **default** (`0`) de los pans, y por tanto el DSP: `process` sigue leyendo
  `params[LOW_PAN].getValue()` crudo (líneas 107, 111, 115) y el clamp `±1` no cambia.
- La ley de paneo ya arreglada en `specs/pan_law_fix_spec.md` (`G_L=min(1,1−p)`, `G_R=min(1,1+p)`).
- Los atenuadores CV `*_PAN_CV` / `*_VOL_CV` (líneas 25-30): llevan `displayBase=0`,
  `displayMultiplier=100`, unidad `"%"` y ya muestran ±100 % correctamente. Sin cambios.
- Los demás knobs (`*_DRIVE`, `*_VOL`, `OUTPUT_VOL`, `MIX_PARAM`): sin cambios.
- Las 18 líneas `getParamQuantity(X)->randomizeEnabled = false`: sin cambios.

---

## 2. Contrato exacto de código

### 2.1 Qué se sobrescribe (decisión)

**Se sobrescribe únicamente `std::string getDisplayValueString() override`.** No se sobrescriben
`getDisplayValue()` ni `setDisplayValueString()`.

Motivo: al dejar los argumentos de display en sus valores por defecto (`displayBase=0`,
`displayMultiplier=1`, `displayOffset=0`, ver §2.2), la ruta base de `getDisplayValue()` es
`displayValue = value * 1 + 0 = value`, es decir **idéntica al valor crudo**: cualquier consumidor
secundario (campo de texto del menú contextual, `Quantity::getString()`, `moveValue`, etc.) recibe un
número sensato en `[-1,1]` sin necesidad de tocar nada. Sobrescribir `getDisplayValue()` sería
redundante, y sobrescribir `setDisplayValue()`/`setDisplayValueString()` para taught "50R" →
`+1.0` introduciría una segunda fuente de verdad del rango y más código sin destino pedido.

**Limitación conocida que esto deja (se acepta conscientemente):** el texto editable del menú
contextual ("Set value") y cualquier otro consumidor de la API de *display value* follow talking el
valor crudo (`1`, `0.76`, `-0.5`), no la etiqueta `50L/38R/C`; y escribir `50R` en ese campo no
producirá un pan a la derecha (el parseo numérico no reconhece el sufijo; el comportamiento exacto
ante texto no parseable está en el `.cpp` cerrado de Rack y no es verificable desde `include/`).
El tooltip —lo pedido— queda correcto. Si más adelante se quiere el campo "Set value" en formato
`NNL/NNR/C`, es una iteración aparte que añadiría `setDisplayValueString()`; queda **fuera de esta
spec**.

### 2.2 Los argumentos finales de `configParam`

Se **eliminan** los argumentos `-1.f, 1.f` (7.º y 8.º) y se dejan los parámetros de display en sus
valores por defecto del SDK:

| Argumento | Valor actual | Valor nuevo | Razón |
|---|---|---|---|
| 6.º `unit` | `""` | `""` (sin cambio) | Sin unidad: la letra `L`/`R` y la `C` ya son la unidad; `%` sería incorrecto |
| 7.º `displayBase` | `-1.f` | **omitido** ⇒ `0.f` | `−1` es logaritmo base 1 = división por cero (§1.2). `0` = lineal |
| 8.º `displayMultiplier` | `1.f` | **omitido** ⇒ `1.f` | Con `displayBase=0` el multiplicador 1 deja `displayValue = value`; el tooltip ya no lo usa, pero los consumidores de la API de display siguen coherentes |
| 9.º `displayOffset` | (implícito `0.f`) | (implícito `0.f`) | Sin cambio |

Es decir, la llamada resultante queda con 6 argumentos, igual que los `*_DRIVE`/`*_VOL` de las
líneas 10 y 12, lo que además unifica el estilo del bloque.

### 2.3 Snippet literal

#### (a) Definición del struct — `src/Saturator.hpp`, **insertar justo antes de la línea `struct Saturator;`** (actual línea 7, tras `using namespace rack;` y su línea en blanco)

```cpp
/** Format for the PAN knobs: bipolar balance on a 0-50 scale, "C" at center.
Overrides only the display string; the raw value in [-1, 1] and its range are unchanged.
*/
struct PanParamQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    float v = getValue();
    if (v < -1.f)
      v = -1.f;
    if (v > 1.f)
      v = 1.f;
    int n = (int) std::lround((v < 0.f ? -v : v) * 50.f);
    if (n == 0)
      return "C";
    return string::f("%d%s", n, v < 0.f ? "L" : "R");
  }
};
```

**Dónde va y por qué (`src/Saturator.hpp`):**

- `Saturator.cpp:1` es `#include "Saturator.hpp"`, luego lo que se defina ahí es visible **antes**
  del `Saturator::Saturator()` de la línea 7, que es donde se instancia `configParam<PanParamQuantity>`.
  `configParam<T>` (Module.hpp:124-146) hace `new T` y escribe `q->ParamQuantity::campo = ...`: exige
  **`ParamQuantity` completo** en el punto de uso, y lo está vía `rack.hpp` → `engine/Module.hpp`
  (línea 3 de `Saturator.hpp`).
- Precedente en el fichero: `Saturator.hpp` ya declara structs propios (`SaturatorWidget:9`,
  `Saturator:13`). Un `ParamQuantity` derivado es un struct propio del módulo, no un widget.
- Se descarta `src/plugin.hpp`: es la capa de **UI/widget** (widgets SVG) e *incluye* `Saturator.hpp`
  (línea 7). Depender de él para un tipo que usa el ctor del motor invierte la layering
  (`plugin.hpp` → `Saturator.hpp`) y funciona sólo por el orden accidental de los `#include` del
  `.cpp`. Además `plugin.hpp` ya está modificado en el working tree (ver §4-e), así que tocarlo
  ampliaría el radio de conflicto.

**Notas de compilación del snippet:**

- `ParamQuantity` sin cualificar: el fichero tiene `using namespace rack;` (línea 5) y
  `ParamQuantity` vive en `rack::engine`; se resuelve igual que `Module`/`ModuleWidget` en el mismo
  fichero. (Alternativa equivalente y más explícita: `struct PanParamQuantity : engine::ParamQuantity`.)
- `std::lround` (C++11, `<cmath>`): **no hace falta ningún `#include` nuevo**. `rack.hpp` →
  `common.hpp:8` → `#include <cmath>` (verificado). Si un compilador futuro lo reclamara, se añade
  `#include <cmath>` junto a `#include <rack.hpp>`; no es necesario para el toolchain de este proyecto
  y se deja fuera para no inflar el diff.
- `string::f` es `rack::string::f` (`include/string.hpp:17`), alcanzable por el mismo
  `using namespace rack;`. No hay ningún `using namespace std;` en las cabeceras del SDK
  (verificado), luego `string::f` no es ambiguo con `std::string`. Está marcado
  `__attribute__((format(printf,1,2)))`, así que el par `%d`/`%s` con `int` y `const char*` se
  valida en compilación.
- Equivalencia numérica: como `|v| ≥ 0`, `std::lround(|v|·50)` y `(int)(|v|·50 + 0.5f)` coinciden
  (half-up, sin caso negativo). Se elige `std::lround` por intención explícita.
- `override` explícito (el build usa `-Wsuggest-override`).

#### (b) Los tres `configParam` — `src/Saturator.cpp`, reemplazar exactamente las líneas **11**, **15** y **19**

```cpp
  configParam<PanParamQuantity>(LOW_PAN, -1.f, 1.f, 0.f, "Low Pan", "");
  configParam<PanParamQuantity>(MID_PAN, -1.f, 1.f, 0.f, "Mid Pan", "");
  configParam<PanParamQuantity>(HIGH_PAN, -1.f, 1.f, 0.f, "High Pan", "");
```

Sólo cambia el template (`<PanParamQuantity>`) y desaparecen los dos argumentos finales `-1.f, 1.f`.
`minValue`, `maxValue`, `defaultValue` y `name` quedan **idénticos**. El `""` de `unit` se mantiene
explícito (aunque podría omitirse, porque `name` y `unit` son ambos `std::string` y no habría
ambigüedad) por simetría con las líneas 10/12/25-30.

Los tres `getParamQuantity(LOW_PAN|MID_PAN|HIGH_PAN)->randomizeEnabled = false;` (líneas 33, 36, 39)
siguen funcionando sin cambios: `getParamQuantity` devuelve `ParamQuantity*` y el campo
`randomizeEnabled` vive en la base.

### 2.4 Invariantes que NO deben cambiar

| Elemento | Estado exigido |
|---|---|
| `params[LOW_PAN|MID_PAN|HIGH_PAN].getValue()` | idéntico; rango `[-1,1]`, default `0` |
| `panLow/Mid/High` (Saturator.cpp:107-118) | sin cambios (usan `getValue()` crudo, no display) |
| Ley de paneo `min(1,1∓p)` | sin cambios |
| Serialización JSON del patch (`toJson/fromJson`) | sin cambios: guarda el valor crudo, no el display |
| `displayBase/Multiplier/Offset` de los 3 pans | `0 / 1 / 0` |
| `name`, `unit`, `getLabel()` de los 3 pans | `"Low Pan"/"Mid Pan"/"High Pan"`, `""` |
| 17 `configParam` restantes | sin cambios, incluidos los 6 `*_PAN_CV`/`*_VOL_CV` |

---

## 3. Tabla de verificación del formato

Rega: `n = lround(|v| · 50)`; `n == 0 ⇒ "C"`; si no, `n` + `"L"` (v<0) / `"R"` (v>0).
Valores calculados de forma independiente (PowerShell, precisión simple, sin compilar el plugin) —
**coinciden con la salida de la regla** de §2.3:

| v | `\|v\|·50` | n | cadena mostrada |
|---:|---:|---:|:---|
| −1.00 | 50 | 50 | `50L` |
| −0.76 | 38 | 38 | `38L` |
| −0.50 | 25 | 25 | `25L` |
| −0.02 | 1 | 1 | `1L` |
| 0.00 | 0 | 0 | **`C`** |
| +0.02 | 1 | 1 | `1R` |
| +0.50 | 25 | 25 | `25R` |
| +0.76 | 38 | 38 | `38R` |
| +1.00 | 50 | 50 | `50R` |

Sobre el caso `±0.02` (el propietario pidió justificarlo si aparecía `C`): con redondeo a entero
**no** aparece `C`; `|0.02|·50 = 1.0` ⇒ `1L` / `1R`. La zona muerta es sólo `|v| < 0.01`
(`0.009 ⇒ C`, `0.01 ⇒ 1R`, medido), es decir el rótulo `C` cubre 2 % del recorrido total, lo que es
imperceptible y evita el `0L/0R`.

Fronteras comprobadas (igualmente por cálculo independiente): `0.98 ⇒ 49R`, `0.99 ⇒ 50R`,
`0.009 ⇒ C`, `0.01 ⇒ 1R`, `-0.009 ⇒ C`, `-0.01 ⇒ 1L`. Consecuencia aceptada del redondeo a
entero: la etiqueta `50R` cubre `v ∈ [0.99, 1.0]` (y `50L` su espejo) — no hay error, es la
resolución del formato (51 rótulos en todo el recorrido).

---

## 4. Criterios de aceptación verificables

- **(a) Build limpio.** En MSYS2/Git-Bash:
  `export PATH=/mingw64/bin:$PATH` y `RACK_DIR=/c/Rack-SDK2.6.6 make clean && make`
  → **0 errores y 0 warnings** (el build usa `-Wall -Wextra -Wsuggest-override`; referencia medida
  hoy en esta máquina: build limpio completo = **4 s**, 2 TUs + link, log de 3 líneas, 0 warnings,
  0 errors). Genera `plugin.dll`. Gate único, sin red.
- **(b) Tabla del §3 cumplida.** Se verifica con un script de cálculo independiente que implemente
  literalmente `n = lround(|v|·50)`, `n==0 ⇒ "C"`, sufijo `L`/`R` para los 9 valores de `v` y las 6
  fronteras, y compruebe las 9 cadenas de la tabla. No requiere compilar el plugin.
- **(c) Comportamiento en Rack.** Con el ratón sobre `LOW_PAN` / `MID_PAN` / `HIGH_PAN`:
  en el centro exacto muestra `C`; en el tope izquierdo `50L`; en el tope derecho `50R`;
  **en ninguna** posición del recorrido aparece `inf`, `-inf`, `nan`, `NaN` ni un string vacío; en
  las posiciones intermedias aparecen `1L…49L` / `1R…49R`. El nombre del knob sigue siendo
  `Low Pan`/`Mid Pan`/`High Pan` y no aparece unidad.
- **(d) Nada más cambia.** El tooltip de los 6 atenuadores `*_PAN_CV`/`*_VOL_CV` sigue mostrando
  `-100 %…100 %`; el de `*_DRIVE`, `*_VOL`, `Output` y `Mix` sigue mostrando su `x.xx %`; el knob se
  sigue moviendo en el mismo rango angular y el paneo audible es idéntico (mismo `getValue()`).
- **(e) Diff acotado.** `git status --porcelain` y `git diff --stat` **antes y después** de la
  edición deben diferir exactamente en:
  `src/Saturator.hpp` (+15/−1 aprox.: 1 struct de 13 líneas + línea en blanco) y
  `src/Saturator.cpp` (3 líneas modificadas: 11, 15, 19).
  Nada más: ni `res/`, ni `Makefile`, ni `plugin.json`, ni `README.md`, ni `LICENSE.txt`, ni
  `src/plugin.{hpp,cpp}`, ni `specs/multiband_saturator_spec.md`, ni `specs/pan_law_fix_spec.md`.
  Ojo: el working tree **ya** estaba modificado respecto a `HEAD` antes de esta iteración (LICENSE,
  Makefile, README, `res/*.svg`, `src/Saturator.cpp`, `src/plugin.hpp`); la comparación es
  antes/después de esta edición, no contra `HEAD`. `plugin.dll` y `build/` son artefactos de
  `make`, no fuente.

---

## 5. Riesgos

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| `configParam<T>` exige `ParamQuantity` **completo** en el punto de uso; si el struct se declarara en un `.cpp` posterior o en `plugin.hpp` con otro orden de includes → error de compilación (`invalid use of incomplete type`) | baja | build (falla ruidosamente) | §2.3: struct en `Saturator.hpp`, que `Saturator.cpp:1` incluye **antes** del ctor; verificado que `rack.hpp` ya trae `engine/Module.hpp` |
| Olvidar quitar `−1.f, 1.f` de los 3 pans → `displayBase` sigue a −1 y el tooltip sigue roto (el override no se usa en esa ruta… no: sí se usa, pero `getDisplayValue()` seguiría degenerado para los consumidores de la API de display) | baja | medio | §2.2 lo fija explícitamente; criterio (c) lo detecta en el tooltip y criterio (d) en la coherencia del resto |
| Efectos residuales de `displayBase` en `setDisplayValueString()`/`getDisplayValue()` si no se sobrescriben | alta (es el estado por defecto de esta decisión) | bajo | **Aceptado y documentado** (§2.1): con `displayBase=0` la ruta base da `displayValue = value`, coherente. Limitación conocida: el campo "Set value" usa el valor crudo y no acepta `50R` |
| Redondeo/`float`: `v = 0.99` ⇒ `\|v\|·50 = 49.5` ⇒ `50R`; `lround` es half-away-from-zero, determinista y sin acumulación (una sola operación sobre el valor ya suavizado) | baja | bajo | Determinista y sin estado: cada llamada recalcula desde `getValue()`; no hay acumulación posible. Las fronteras están medidas (§3) |
| `v` fuera de `[−1,1]` por smoothing o por `setValue` directo (CV a ±10 V) → `n` podría ser 51 o mayor | muy baja | bajo | El clamp `±1` de las dos primeras líneas del cuerpo lo corta; el `clamp(…, −1, 1)` de `process:107-118` ya limita el valor effective |
| Que el revisor espere `50 %` con unidad, o un rótulo numérico con signo (`−25`) | baja | bajo | §1.1 y §3 fijan el formato; el enunciado del propietario es `50L/50R/C` |
| Conflicto con otra iteración que toque `Saturator.hpp` (el fichero está limpio respecto a `HEAD`; `Saturator.cpp` no) | baja | bajo | El struct va antes de `struct Saturator;` y no toca IDs; cualquier otro cambio en `Saturator.hpp` debe ir en un hunk distinto (final del fichero) |

---

## 6. Ficheros a tocar y tiempo

**Ficheros: 2 (disjuntos de todo lo demás).**

| Fichero | Cambio | Líneas |
|---|---|---|
| `src/Saturator.hpp` | + struct `PanParamQuantity` antes de `struct Saturator;` | +15 / −0 |
| `src/Saturator.cpp` | `configParam` → `configParam<PanParamQuantity>` en pans, y quitar `−1.f, 1.f` | 3 modificadas |

**Tiempo estimado** (medido sobre esta máquina):

| Tarea | Tiempo | Cómo se mide |
|---|---|---|
| Edit de `Saturator.hpp` + `Saturator.cpp` | 5–8 min | reloj hasta el diff del §4-e |
| `make clean && make` | **≈4 s** (medido hoy: 4 s, 0 warnings, 0 errors) | `/usr/bin/time make clean && make` |
| Script de verificación del §3 (criterio b) | 3–5 min | reloj hasta las 9+6 filas impresas |
| Verificación en Rack (criterios c y d) | 2–3 min | reloj hasta comprobar los 3 knobs × (centro, 2 extremos, 4 intermedios) + 6 atenuadores |

**Total: ≈12–18 min.** Edit + build + script son automatizables y no requieren al operador; sólo (c) y
(d) necesitan abrir Rack.

**Cómo se verifica la estimación:** el número de 4 s de compilación está medido, no estimado
(`make clean` + `make`, log de 3 líneas, `grep -ci warning` = 0, `grep -ci error` = 0,
`plugin.dll` regenerado, 1 654 775 bytes). Los tiempos de edición y de verificación en Rack son
estimaciones del plan de trabajo y se confirman con reloj en la primera ejecución; si el diff y el
script salen como en §4-e y §3, el total se queda por debajo de 20 min.
# MICRO-SPEC — Escalado CV unificado `/10` en PAN y VOL (`Multiband_Saturator`)

Iteración: W-CVSCALE (cierra el hallazgo **C9**; no toca la ley de paneo de `specs/pan_law_fix_spec.md`
ni los tooltips de `specs/pan_display_fix_spec.md`).
Fichero de producción a tocar: **solo `src/Saturator.cpp`** (+ `specs/multiband_saturator_spec.md`
para trazabilidad, que es documentación, no producción).

---

## 1. Objetivo y alcance

Hoy el CV de **pan** escala con divisor **5** y el de **vol** con divisor **10**. Con un atenuador al
100 %, ±10 V de CV de pan dan ±2 unidades de pan y saturan el clamp a partir de **±5 V**: el recorrido
estéreo completo no se puede recorrer con el rango habitual de un LFO (±5 V) y el comportamiento no
coincide con el de VOL. Este fix **unifica el divisor en `/10` en los 6 parámetros**: 10 V = 1 unidad de
parámetro, igual que en VOL y que en la convención de los ±5 V de los cables de VCV.

Fórmula única, **bipolar sin offset**, para los 6 parámetros:

```
valor = clamp( knob + (V / 10) * atenuador ,  min ,  max )
```

Decisiones del propietario, **documentadas tal cual, sin reinterpretar**:

| Decisión | Valor | Efecto |
|---|---|---|
| Divisor | `/10` en PAN **y** VOL | 10 V = 1 unidad de parámetro |
| Semántica | bipolar **sin offset**: 0 V = sin cambio | no hay punto medio desplazado |
| Atenuadores `*_PAN_CV` / `*_VOL_CV` | rango −1..+1, default **0** | sin cambios (`configParam`, líneas 25–30) |
| Sin jack patcheado | el atenuador **no** afecta al parámetro | se mantiene tal cual; `getVoltage()` devuelve 0 con el jack desconectado ⇒ término CV = 0. **NO** introducir `isConnected()` ni offsets |

**FUERA de alcance — NO tocar:**

- Mapeo de jacks (`SaturatorWidget`, `res/Saturator.svg`), coordenadas del widget, `box.size`.
- Ley de paneo `G_L = min(1, 1−p)` / `G_R = min(1, 1+p)` (líneas 134–141), ya arreglada.
- Rangos y defaults de los 6 parámetros (`configParam`, líneas 11–20) y de los 6 atenuadores (25–30).
- `src/Saturator.hpp`, `src/plugin.{hpp,cpp}`, `Makefile`, `plugin.json`, `res/`, `README.md`.
- `panLow/Mid/High` y `volLow/Mid/High` como *lugares*: sólo cambia el divisor literal.
- Los 3 bloques de `vol` (líneas 120–131): **ya usan `/10.f` y quedan intactos**.

**Consecuencia asumida (no es un defecto, es el contrato):** con el divisor antiguo, ±5 V de CV de pan
ya saturaban al extremo; con `/10` el tope necesita ±10 V. El recorrido L↔R completo pasa a requerir un
CV de ±10 V. A cambio, PAN y VOL se comportan igual y 10 V = 1 unidad en los seis casos. Los patches
guardados son compatibles (los atenuadores están a 0 por defecto ⇒ el término CV es 0 y nada cambia al
cargar un patch existente).

---

## 2. Contrato: tabla de los 6 parámetros

Con `P` = valor crudo del knob, `V` = voltaje en el jack, `A` = atenuador (0 por defecto).
Valores de la tabla con **knob al centro (P = 0)** y **atenuador al +100 % (A = +1)**.
Todos los valores calculados por sustitución literal de la fórmula, sin compilar.

| # | Parámetro | Fórmula resultante | Rango / clamp | 0 V | +10 V | −10 V |
|---|---|---|---|---:|---:|---:|
| 1 | `LOW_PAN` | `clamp(P + (V/10)·A, −1, 1)` | `[-1,1]` | `0` (sin cambio) | `+1` → **tope derecho** | `−1` → **tope izquierdo** |
| 2 | `MID_PAN` | `clamp(P + (V/10)·A, −1, 1)` | `[-1,1]` | `0` | `+1` → tope derecho | `−1` → tope izquierdo |
| 3 | `HIGH_PAN` | `clamp(P + (V/10)·A, −1, 1)` | `[-1,1]` | `0` | `+1` → tope derecho | `−1` → tope izquierdo |
| 4 | `LOW_VOL` | `clamp(P + (V/10)·A, 0, 1)` | `[0,1]` | `0` (sin cambio) | `+1` → **100 %** | `0` → silencioso (saturado por clamp) |
| 5 | `MID_VOL` | `clamp(P + (V/10)·A, 0, 1)` | `[0,1]` | `0` | `+1` → 100 % | `0` → silencioso |
| 6 | `HIGH_VOL` | `clamp(P + (V/10)·A, 0, 1)` | `[0,1]` | `0` | `+1` → 100 % | `0` → silencioso |

Lecturas exigidas de la tabla:

- **Los 6 usan el mismo divisor `10`.** No queda ningún `/5` en el fichero.
- **0 V ⇒ término CV nulo en los 6**, sea cual sea `A`: el parámetro vale exactamente `P`. Ningún offset.
- **PAN**: `+10 V` con `A=+1` da `clamp(P+1, −1, 1)`; desde `P=0` es exactamente `+1` (tope derecho, canal L mudo por la ley de paneo) y `−10 V` es exactamente `−1` (tope izquierdo). Con `P` distinto de 0 el tope se alcanza antes por el clamp — es correcto.
- **VOL**: `+10 V` con `A=+1` da `clamp(P+1, 0, 1)`; desde `P=0` es exactamente `1` (100 %). `−10 V` da `clamp(−1, 0, 1)` = `0`: **la banda se silencia**, no se invierte el signo (el rango es unipolar, no bipolar).
- **A = 0** (default) ⇒ las 6 filas devuelven `P` con cualquier `V`.
- **Jack desconectado** ⇒ `V = 0` ⇒ las 6 filas devuelven `P` con cualquier `A`.

| Bandera de tensión | PAN | VOL |
|---|---|---|
| LFO bipolar típica ±5 V | ±0.5 unidades | ±0.5 unidades |
| Slew / offset +5 V unipolar | +0.5 unidades | +0.5 unidades |

Es decir: **PAN y VOL quedan simétricos**; hoy divergen por un factor 2. Ése es el bug que se cierra.

---

## 3. Diff exacto a aplicar

Un único fichero de producción. **Tres líneas modificadas**, todas en el bloque de PAN de
`Saturator::process` (`src/Saturator.cpp`). Se cambia **sólo el literal del divisor**: ni el nombre de
la entrada, ni el del atenuador, ni el orden de las operaciones, ni el `clamp`, ni la indentación.

### (a) `src/Saturator.cpp`, línea **108** (dentro de `panLow`, bloque 107–110)

```diff
   float panLow = clamp(params[LOW_PAN].getValue() +
-                          (inputs[LOW_PAN_INPUT].getVoltage() / 5.f) *
+                          (inputs[LOW_PAN_INPUT].getVoltage() / 10.f) *
                               params[LOW_PAN_CV].getValue(),
                       -1.f, 1.f);
```

### (b) `src/Saturator.cpp`, línea **112** (dentro de `panMid`, bloque 111–114)

```diff
   float panMid = clamp(params[MID_PAN].getValue() +
-                          (inputs[MID_PAN_INPUT].getVoltage() / 5.f) *
+                          (inputs[MID_PAN_INPUT].getVoltage() / 10.f) *
                               params[MID_PAN_CV].getValue(),
                       -1.f, 1.f);
```

### (c) `src/Saturator.cpp`, línea **116** (dentro de `panHigh`, bloque 115–118)

```diff
   float panHigh = clamp(params[HIGH_PAN].getValue() +
-                            (inputs[HIGH_PAN_INPUT].getVoltage() / 5.f) *
+                            (inputs[HIGH_PAN_INPUT].getVoltage() / 10.f) *
                                 params[HIGH_PAN_CV].getValue(),
                         -1.f, 1.f);
```

**Nada más.** Las 6 líneas de los 3 bloques de `vol` (120–131, divisores `/ 10.f` en 121, 125, 129) no
se tocan: ya cumplen el contrato. El número de líneas del fichero no cambia, luego las líneas 120–131
siguen siendo 120–131 después del cambio y `git diff` debe mostrar exactamente **3 líneas `-` y 3 `+`**.

`git diff --stat` esperado: `src/Saturator.cpp | 6 +++---` (3 modificadas). Cero líneas añadidas o
borradas.

---

## 4. Criterios de aceptación verificables

- **(a) Build limpio.** En MSYS2/Git-Bash:
  `export PATH=/mingw64/bin:$PATH` y `RACK_DIR=/c/Rack-SDK2.6.6 make clean && make`
  → **0 errores y 0 warnings** (el build usa `-Wall -Wextra -Wsuggest-override`), genera `plugin.dll`.
  Gate único, sin red. Referencia medida en esta máquina: build limpio completo ≈ **4 s**, 2 TUs + link,
  `grep -ci warning` = 0, `grep -ci error` = 0.
- **(b) Contrato unificado en el fichero.** `grep -c '/ 5\.f' src/Saturator.cpp` → **0** y
  `grep -c '/ 10\.f' src/Saturator.cpp` → **6** (3 en PAN + 3 en VOL). Y `git diff` muestra
  **exactamente 3 pares de líneas**, todas de la forma `getVoltage() / 5.f` → `getVoltage() / 10.f`,
  en las líneas 108, 112 y 116. Ninguna otra línea del fichero aparece en el diff.
- **(c) Tabla del §2 cumplida.** Se verifica con un script de cálculo independiente que implemente
  literalmente `clamp(P + (V/10)*A, min, max)` para los 6 parámetros y compruebe las 18 celdas
  (6 filas × 3 voltages) más las 2 banderas de §2 (A = 0, jack desconectado). No requiere compilar el
  plugin ni dependencias nuevas.
- **(d) Chequeo manual en Rack (el criterio clave).** Con el módulo cargado, patcheando un **+10 V**
  constante en el jack `PAN` de una banda y su atenuador `*_PAN_CV` al **100 %**: el **pan de esa banda
  se va a tope a la derecha** (canal izquierdo mudo de esa banda, no "más bajo"). Lo mismo para el jack
  `VOL` de esa banda: el **volumen llega al 100 %** (no se oye cambio apreciable al subir más el
  atenuador). Repetido para `LOW`, `MID` y `HIGH` (3 bandas × 2 parámetros = 6 comprobaciones).
- **(e) Simetría y saturación.** Con **−10 V** y atenuador al 100 %: el **pan va a tope a la
  izquierda** y el **vol baja a 0** (esa banda se silencia). Ningún otro canal se ve afectado.
- **(f) 0 V y sin jack no hacen nada.** Con **0 V** en cualquier jack, y con el atenuador a cualquier
  valor, el parámetro vale exactamente el del knob. Con **ningún jack patcheado** y el atenuador al
  100 %, el knob manda íntegro (sin salto, sin offset, sin mute) en los 6 parámetros. Con el atenuador a
  **0 %** el CV no tiene efecto (valor por defecto, sin cambios).
- **(g) Nada más cambia.** La ley de paneo `min(1, 1∓p)` responde igual que antes (canal opuesto a 0
  exacto en el tope); `MIX`, `OUTPUT_VOL`, `DRIVE`, crossovers, luces, `box.size`, tooltips, mapeo de
  jacks y el rótulo del panel son idénticos. Los patches existentes cargan sin cambio audible, porque
  los 6 atenuadores están a 0 por defecto.

---

## 5. Trazabilidad

- **`specs/multiband_saturator_spec.md`, línea 96 (fila C9)**: cambia de **"BAJA / abierto"** a
  **resuelto por este fix**. Texto a dejar:
  `| C9 | BAJA | **RESUELTO (W-CVSCALE, `cv_scaling_fix_spec.md`)** — Escalado CV unificado en `/10`:
  pan y vol usan ambos el mismo divisor, 10 V = 1 unidad. Era `V/5` en pan (saturaba a ±5 V) frente a
  `V/10` en vol. | `src/Saturator.cpp:107-131` |`
  La columna Sev puede bajar a **INFO** (ya no es un riesgo abierto) — decisión del orquestador; el
  contenido del hallazgo no se borra, se marca como cerrado.
- Queda obsoleto en `specs/multiband_saturator_spec.md` §1.2 / §C-3 cualquier texto que describa el CV
  de paneo como `V/5`. Revisar con `grep -n '/ 5' specs/*.md` y ajustar sólo si aparece.
- **Siguen válidos y no se tocan**: los baselines medidos de §C-7 (se midieron con pan = 0 y
  atenuadores a 0, y en esas condiciones el término CV sigue siendo 0 exacto), el mapeo de jacks §C-3
  (C1), C2, C10, C11, geometría §C-4 y el resto de la spec. No hace falta remedir nada.
- `specs/pan_law_fix_spec.md` §3 (invariante «cálculo de pan sin cambios: `clamp(PAN + (CV/5)*PAN_CV)`»)
  y §5-d («con +5 V … extremo derecho») quedan **superados** por esta iteración: el criterio (d) nuevo
  exige **+10 V** para el tope. **Ya marcados como superados en ese fichero** (líneas 24, 68 y 137–138),
  con nota que remite a esta iteración.
- `specs/pan_display_fix_spec.md`: sin impacto (trata de tooltips, no del divisor).

---

## 6. Plan de ficheros, riesgos y tiempos

### 6.1 Plan de ficheros

| Fichero | Cambio | Tamaño | Riesgo de conflicto |
|---|---|---|---|
| `src/Saturator.cpp` | 3 literales `/ 5.f` → `/ 10.f` (líneas 108, 112, 116) | 3 líneas modificadas, +3/−3 | Bajo: bloque 107–118 exclusivo de esta iteración. Ninguna otra unidad en curso debe tocar `Saturator::process` |
| `specs/multiband_saturator_spec.md` | fila C9 (línea 96) → resuelto | 1 línea | Nulo (documentación) |
| `specs/cv_scaling_fix_spec.md` | este fichero | nuevo | Nulo |

**Los dos ficheros de código son disjuntos del resto**: el cambio de producción es un literal dentro de
3 líneas contiguas del bloque de PAN; la edición de `specs/` no colisiona con nada.

### 6.2 Riesgos

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| Perder recorrido de CV (±5 V ya no satura el pan) | **alta (es el contrato pedido)** | medio, consciente | Documentado en §1 y §5; decisión del propietario, no un defecto |
| Cambiar por error un divisor de `vol` o tocar el `clamp` | baja | alto | §3 fija 3 líneas exactas; criterio (b) comprueba `6` ocurrencias de `/ 10.f` y que el diff tiene sólo 3 pares |
| Tocar `Saturator.hpp` por el `configParam` de los atenuadores | baja | medio | Fuera de alcance explícito (§1); default 0 y rango −1..+1 ya son correctos |
| Introducir `isConnected()` o un offset "para arreglar" el jack desconectado | baja | alto | Prohibido explícitamente (§1); criterio (f) lo detecta (salto audible al patchear/despatchear) |
| Un LFO de ±5 V ya no recorre el pan completo | alta (esperable) | bajo | Documentado en la tabla de banderas de §2; no es un error |

### 6.3 Timings estimados

| Tarea | Tiempo | Cómo se mide |
|---|---|---|
| Edit de `src/Saturator.cpp` (3 líneas) | **2–3 min** | reloj hasta que `git diff --stat` muestra `6 +++---` |
| `make clean && make` | **≈4 s** (medido hoy en esta máquina: 0 warnings, 0 errors) | `/usr/bin/time make clean && make` + `grep -ci warning/error` |
| Script de verificación del §2 (criterio c) | 3–5 min | reloj hasta imprimir las 18 celdas + las 2 banderas |
| Edit de `specs/multiband_saturator_spec.md` (fila C9) | **1 min** | reloj hasta el `grep -n 'C9' specs/multiband_saturator_spec.md` |
| Verificación manual en Rack (criterios d–g) | 3–5 min | reloj hasta las 6 comprobaciones de (d), las 2 de (e), las de (f) y (g) |

**Total: ≈10–16 min.** Edit + build + script son automatizables y no requieren al operador; sólo (d)–(g)
necesitan abrir Rack.

**Cómo se verifica la estimación:** el tiempo de compilación (≈4 s) está **medido**, no estimado
(build limpio completo de 2 TUs + link, `plugin.dll` regenerado). Los tiempos de edición y de
verificación en Rack son estimaciones del plan de trabajo y se confirman con reloj en la primera
ejecución. Criterio de cierre: diff acotado (§3) + build sin warnings + las 18 celdas del §2
confirmadas + las 6 comprobaciones de (d) en Rack.

---

## 7. Resumen accionable para el orquestador

1. **Un solo fichero de producción**: `src/Saturator.cpp`, líneas **108, 112, 116**. Sustituir
   `/ 5.f` por `/ 10.f`. Nada más.
2. Las 3 líneas de `vol` (121, 125, 129) **ya son correctas**: no tocarlas.
3. Un segundo fichero, documentación: `specs/multiband_saturator_spec.md` fila **C9** (línea ~96) →
   resuelto por este fix.
4. Gates: `make clean && make` con **0 errores y 0 warnings**; `grep -c '/ 5\.f'` = 0 y
   `grep -c '/ 10\.f'` = 6; y en Rack, **+10 V con atenuador al 100 % ⇒ pan a tope derecho y vol al
   100 %** para las 3 bandas.
5. Consecuencia que hay que comunicar: el tope de paneo por CV pasa de ±5 V a **±10 V**. Es
   intencional.
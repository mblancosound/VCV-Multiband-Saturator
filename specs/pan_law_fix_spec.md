# MICRO-SPEC — Corrección de la ley de paneo (`Multiband_Saturator`)

Iteración: W-PAN (sustituye a la descripción de pan de `specs/multiband_saturator_spec.md` §1.2, punto 3).
Fichero de producción a tocar: **solo `src/Saturator.cpp`**.

---

## 1. Objetivo y alcance

Sustituir la ley de paneo por banda `(1∓pan/2)` —que en los extremos `p=±1` produce ganancias
1.5/0.5, un boost de +3.98 dB de potencia total y una fuga de −6 dB en el canal que debería estar
mudo— por una **ley de balance lineal con corte total**: `G_L = min(1, 1−p)` y `G_R = min(1, 1+p)`,
con `p ∈ [−1,1]` y `p > 0` = derecha. Se aplica a `wet*vol` de cada una de las tres bandas. En
centro (p=0) el resultado es 1/1, idéntico al actual, de modo que los baselines medidos siguen
válidos. El cambio es local a las 6 líneas de salida por banda.

**FUERA de alcance en esta iteración (no tocar):**
- Artwork / labels de la sección CV del panel (`res/Saturator.svg`) y C2 de la spec grande.
- Luces (`lightDivider`, `SATURE_LIGHT`), su cálculo a partir de `wetLowL`/`wetMidL`/`wetHighL`.
- `onSampleRateChange` (no existe; no introducirlo).
- DC-blocker (no existe; no introducirlo).
- Compensación / mezcla de **igual potencia** (esta ley es de balance, no de potencia).
- `configParam` de `*_PAN_CV`: el default **se queda en 0** (decisión del propietario), y el
  escalado del CV de pan se mantiene. *(Superado por `cv_scaling_fix_spec.md` (W-CVSCALE): el divisor es hoy `/10.f`.)*
- Cualquier otro parámetro, filtro, crossover, drive, `vol`, `mix`, `outVol`.
- Renumerar/reordenar/eliminar IDs de `Saturator.hpp` (`ParamIds`, `InputIds`, `OutputIds`,
  `LightIds`): están persistidos en patches. Tampoco se toca `Saturator.hpp`.
- `box.size` y toda la geometría de la UI.
- `specs/multiband_saturator_spec.md`, `README.md` y cualquier otro fichero.

---

## 2. Contrato exacto de código

**Punto de cambio:** `src/Saturator.cpp`, función `Saturator::process`, bloque de 6 líneas que va
desde `float lowOutL = wetLowL * volLow * (1.f - panLow * 0.5f);` hasta
`float highOutR = wetHighR * volHigh * (1.f + panHigh * 0.5f);` (actualmente líneas **133–140**,
incluyendo las dos líneas en blanco que separan las bandas: 133-134, blank, 136-137, blank, 139-140).

Se reemplaza **todo** ese bloque por el siguiente, listo para transcribir:

```cpp
  // Ley de pan: balance lineal con corte total (G_L=min(1,1-p), G_R=min(1,1+p)); nunca gana >1
  float lowOutL = wetLowL * volLow * std::min(1.f, 1.f - panLow);
  float lowOutR = wetLowR * volLow * std::min(1.f, 1.f + panLow);

  float midOutL = wetMidL * volMid * std::min(1.f, 1.f - panMid);
  float midOutR = wetMidR * volMid * std::min(1.f, 1.f + panMid);

  float highOutL = wetHighL * volHigh * std::min(1.f, 1.f - panHigh);
  float highOutR = wetHighR * volHigh * std::min(1.f, 1.f + panHigh);
```

Decisión explícita de forma: se usa **`std::min(1.f, 1.f ∓ pan)`**. Es la forma canónica, ya
empleada en el fichero (`std::min` se usa en `process` y en `SaturatorWidget`), y es
`1 − max(0, ±p)` reescrito; no se mezclan las dos formas en el fichero. `panLow/Mid/High` ya llegan
clampados a `[−1,1]` por las líneas 107–118, que no se modifican: no hace falta un clamp extra y
añadirlo sería ruido. `std::min` con `float` y constante `1.f` evita la promoción a `double` en
comparaciones. El comentario de una línea va inmediatamente antes de `float lowOutL`.

---

## 3. Invariantes que NO deben cambiar

| Elemento | Estado exigido tras el cambio |
|---|---|
| `p = 0` | `G_L = G_R = 1` (idéntico a la ley actual) |
| Cálculo de `panLow/Mid/High` (líneas 107–118) | estructura sin cambios; divisor `/10` desde W-CVSCALE: `clamp(PAN + (CV/10)*PAN_CV, −1, 1)` — ver `cv_scaling_fix_spec.md` |
| `*_PAN_CV` default | 0 (sin cambios en `configParam`) |
| Camino dry (líneas 145–147) | `in*(1−mix) + wet*mix`, lineal |
| Suma de bandas (142–143) | `wetL = lowOutL+midOutL+highOutL`,idem R |
| `mix`, `outVol`, `outputs[OUT_L/R]` (145–151) | sin cambios |
| Drive, `tanh`, crossovers, `vol` (96–131) | sin cambios |
| Luces (153+) | sin cambios; se calculan desde `wetLowL/wetMidL/wetHighL`, no desde `*Out*` |
| `Saturator.hpp`, IDs, `box.size`, `SaturatorWidget` | sin cambios |

Consecuencia a asumir: aplicar un gain 0 a `wetLowL` no modifica el valor `wetLowL`, luego las
luces no se apagan al paneo extremo. Es el comportamiento deseado (la luz mide saturación, no
posición estéreo).

---

## 4. Tabla de verificación de la ley nueva

Valores calculados de forma independiente (PowerShell, sin compilar el plugin):
`GL = min(1, 1−p)`, `GR = min(1, 1+p)`, `P = GL² + GR²`, `dB = 10·log10(P / P₀)` con `P₀ = 2` (p=0).

| p | G_L | G_R | P = L²+R² | dB vs p=0 | L+R (amp. suma) |
|---:|---:|---:|---:|---:|---:|
| −1.00 | 1.000 | 0.000 | 1.0000 | −3.01 | 1.000 |
| −0.75 | 1.000 | 0.250 | 1.0625 | −2.75 | 1.250 |
| −0.50 | 1.000 | 0.500 | 1.2500 | −2.04 | 1.500 |
| 0.00 | 1.000 | 1.000 | 2.0000 | 0.00 | 2.000 |
| +0.50 | 0.500 | 1.000 | 1.2500 | −2.04 | 1.500 |
| +0.75 | 0.250 | 1.000 | 1.0625 | −2.75 | 1.250 |
| +1.00 | 0.000 | 1.000 | 1.0000 | −3.01 | 1.000 |

Lecturas exigidas:
- Extremos (`p = ±1`): el canal opuesto queda a **0.000 exacto** y el otro a 1.000.
- Potencia total en los extremos: **1.0000 → −3.01 dB** respecto a p=0 (2.0000). Nunca > 1.
- Ninguna ganancia individual supera 1 en toda la tabla (máximo absoluto = 1.000).
- Simetría especular: filas ±0.5 y ±0.75 dan P idéntico; sólo se intercambian L↔R.
- Ley antigua en los mismos puntos, para contraste: `p=1` daba `GL=0.5, GR=1.5, P=2.5 (+0.97 dB)` y
  fuga de −6 dB en el canal que debería estar mudo. Es exactamente lo que se elimina.

---

## 5. Criterios de aceptación verificables

- **(a) Build limpio.** En MSYS2/Git-Bash:
  `export PATH=/mingw64/bin:$PATH` y `RACK_DIR=/c/Rack-SDK2.6.6 make clean && make`
  → 0 errores y **0 warnings**, genera `plugin.dll`. Gate único del proyecto, sin red.
- **(b) Tabla del punto 4 cumplida.** Se verifica con un script de cálculo independiente que
  implemente literalmente `G_L = min(1, 1−p)` y `G_R = min(1, 1+p)` para los 7 valores de p y
  compruebe las 4 lecturas listadas (extremos a 0, P=1.0, dB=−3.01, ningún gain >1). No requiere
  compilar el plugin ni dependencias nuevas.
- **(c) Comportamiento en Rack.** Con el PAN de una banda al extremo derecho: el canal izquierdo de
  esa banda queda **mudo** (no "más bajo") y el nivel del canal derecho **no sube** respecto a p=0.
  Lo mismo espejado al extremo izquierdo. Con el PAN de las tres bandas a 0 el sonido es
  indistinguible del anterior al cambio.
- **(d) CV de paneo responde.** Con el atenuador `*_PAN_CV` al **100 %** (valor 1.0) y **+5 V** en el
  jack de PAN de esa banda, el pan alcanza el extremo derecho (clamp en `+1`); con **−5 V**, el
  extremo izquierdo. Con +10 V la salida se mantiene en `+1` (clamp), sin boost adicional. Con el
  atenuador a 0 el CV no tiene efecto (default actual, sin cambios).
- **(e) Baselines §C-7 siguen válidos** porque todos se midieron con pan = 0, y en p = 0 la ley da
  `G_L = G_R = 1`, idéntico a `1 ∓ 0·0.5 = 1`. Los valores publicados (suma de bandas +1.00 dB a
  1 kHz, −2.06 dB a 2.5 kHz, ~0 dB a 20 Hz/20 kHz, THD 33.08 % a drive=1) no se ven afectados; no
  hace falta remedir, aunque una comprobación de humo con pan=0 es recomendable.

---

## 6. Trazabilidad

Queda obsoleto en `specs/multiband_saturator_spec.md`:
- §1.2, punto 3 del DSP: el texto «pan (`(1∓pan/2)`)» describe la ley retirada. En una iteración
  posterior habrá que sustituirlo por «balance lineal con corte total (`min(1, 1∓pan)`)».
- El hallazgo C9 (§tabla de riesgos) sobre el escalado del CV de pan: **cerrado** por
  `cv_scaling_fix_spec.md` (divisor unificado en `/10`).

Siguen válidos y **no se tocan**: los baselines medidos de **§C-7**, el mapeo de jacks de **§C-3**,
C1, C2, geometría §C-4 y el resto de la spec. Este fichero (`specs/pan_law_fix_spec.md`) no modifica
`specs/multiband_saturator_spec.md` en esta iteración.

---

## 7. Riesgo y tiempo de implementación

**Fichero a tocar:** `src/Saturator.cpp` (único). Edit de 8 líneas (6 de código + 1 comentario + 2
blancos) más compilación. Riesgo de conflicto con otras iteraciones: nulo sobre este repo, siempre
que ninguna otra unidad toque `Saturator::process`.

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| Regresión en p=0 por error de transcripción (p. ej. `1.f + panLow` en el lado L) | baja | alto | criterio (b) + (c); diff limitado a 6 líneas |
| `<algorithm>` no incluido y `std::min` no resuelve | muy baja | build | `rack.hpp` lo arrastra y el fichero ya usa `std::min`/`std::abs` |
| Que el revisor espere la forma `1 − max(0,p)` | baja | bajo | forma y motivo fijados en §2 |
| Confundir el corte con un fade de −3.01 dB a p=±1 | baja | bajo | tabla §4: es −3.01 dB solo en el extremo, 0 exacto en el canal mudo |

**Estimación: 5–10 min** de edición + verificación, más 2–5 min de `make clean && make`
(≈40 s de compilación en esta máquina, según §1.4). Verificación en Rack (criterios c y d):
≈2–3 min por el operador. Total ≈15 min. Cómo se mide: tiempo de reloj hasta `plugin.dll`
regenerado sin warnings y las 4 lecturas de la tabla del §4 confirmadas por el script.
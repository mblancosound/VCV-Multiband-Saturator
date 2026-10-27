# MICRO-SPEC — Curvas de saturación por banda (Low/Mid/High) (`Multiband_Saturator`)

Iteración: **W-CURVES** · Estado: **ESPEC (v2.2.1; Clean normalizado por W-CLEAN-TRIM)** · Mitiga el hallazgo **C5** de
`specs/multiband_saturator_spec.md` y añade el selector de curva de saturación por banda (Clean/Tanh/Cubic/
Arctan/Hard/Foldback/Sine Fold/Tube/Diode) en el menú contextual.

Ficheros de producción: `src/Curves.hpp` (nuevo, header-only), `src/Saturator.hpp`, `src/Saturator.cpp`,
`src/SaturatorWidget.cpp`. `Makefile` sin cambios (usa `src/*.cpp`). No se tocan panel/SVG/params/enums.

---

## 1. Objetivo y alcance

La no linealidad `tanh(x * drive)` está hardcodeada en `processBands` (6 `std::tanh`). La v2.2.1 introduce
un **selector de curva de saturación por banda** (Low/Mid/High) en el menú contextual: 9 curvas —
`Clean(0)`, `Tanh(1)`, `Cubic(2)`, `Arctan(3)`, `Hard(4)`, `Foldback(5)`, `Sine Fold(6)`, `Tube(7)`,
`Diode(8)` — con default **TANH** (conserva el comportamiento actual).

**Alcance:**

- `src/Curves.hpp`: enum `CurveId`, `applyCurve(u, curve)` con switch inline, `curveNames()`, `TRIM[]`.
- `src/Saturator.hpp`: `OSParams` lleva `curveLow/Mid/High`; estado `std::atomic<int> curves[3]`;
  `setCurve(band, id)`.
- `src/Saturator.cpp`: `processBands` sustituye los 6 `std::tanh` por `applyCurve(curveDrive(banda, drive, curve), curve)`;
  DC-blocker 15 Hz.
- `src/SaturatorWidget.cpp`: tres submenús contextuales de curva.

**Fuera de alcance (esta iteración):** ADAA; crossfade (click aceptado, control de setup); params nuevos,
panel/SVG, renumerar enums; bypass (sigue sin, C5); automatización del control de curva.

---

## 2. Contrato DSP de `src/Curves.hpp`

Header-only, namespace `coaldynamics`, sin dependencia de Rack: el arnés offline de la fase 1 compila este
fichero tal cual.

| Elemento | Contrato |
|---|---|
| `enum CurveId` | `CLEAN = 0, TANH, CUBIC, ARCTAN, HARD, FOLDBACK, SINEFOLD, TUBE, DIODE, CURVE_COUNT`. Valores fijos (no se renumeran; persistencia por ID). |
| `applyCurve(u, curve)` | Switch inline sobre `int curve`. Id inválido → cae a `TANH`. Devuelve `y * TRIM[curve]`. |
| `curveNames()` | Nombres en orden del enum: `"Clean", "Tanh", "Cubic", "Arctan", "Hard", "Foldback", "Sine Fold", "Tube", "Diode"`. |
| `TRIM[]` | `static const float TRIM[CURVE_COUNT]`; `TRIM[CLEAN] = 0.279824` (v2.2.1, normalizado a TANH; CLEAN no recibe drive vía `curveDrive()`; ver §9.7). |

Propiedades del contrato de curva:

1. `f(0) = 0` para las 9 curvas.
2. `f'(0) ~ 1` salvo `CUBIC` (1.5 por el factor 1.5 de la fórmula), `TUBE` (1.46) y `DIODE` (1.0 en 0 pero asimétrica).
3. `|f(u)| <= 1` antes de `TRIM` en las 8 curvas no lineales; `CLEAN` es lineal (`|f| = |u|`, exenta de no-linealidad por D2; recibe la banda SIN drive y sale escalada por `TRIM[CLEAN] = 0.279824`).

Salida final: `y_out = y * TRIM[curve]` con `u = curveDrive(banda_filtrada, drive, curve)`.

---

## 3. Fórmulas exactas de las 9 curvas

| Curva | Fórmula `f(u)` | Observaciones |
|---|---|---|
| `CLEAN` | `y = u` con `u = banda` (SIN drive) | Lineal; `TRIM = 0.279824` (v2.2.1, nivel seguro). `curveDrive()` devuelve `banda` para CLEAN y `banda * drive` para el resto. |
| `TANH` | `y = tanh(u)` | -1..1. |
| `CUBIC` | `y = clamp(u - u^3/3, -2/3, 2/3) * 1.5` | Clamp ±2/3 → ×1.5 = ±1. |
| `ARCTAN` | `y = (2/pi) * atan(u * pi/2)` | -1..1. |
| `HARD` | `y = clamp(u, -1, 1)` | ±1. |
| `FOLDBACK` | `t = fmod(u+1, 4); if (t < 0) t += 4; if (t > 2) t = 4 - t; y = t - 1` | t ∈ [0,2] → y ∈ [-1,1]. |
| `SINEFOLD` | `y = sin(u)` | -1..1. |
| `TUBE` | `b = 0.5; y = clamp((tanh(u+b) - tanh(b)) / (1 - tanh(b)), -1, 1)` | Clamp (D1). |
| `DIODE` | `c = 0.5; y = (u >= 0) ? (1 - exp(-u)) : -(1 - exp(u/c)) * c` | Asimétrica (1.0 en 0). |

Donde: `u = curveDrive(banda_filtrada, drive, curve)` (CLEAN ignora el drive); salida final `y_out = y * TRIM[curve]`.

---

## 4. Desviaciones cerradas por el orquestador (decisiones, no re-abiertas)

| ID | Desviación | Decisión |
|---|---|---|
| D1 | `TUBE` se recorta a [-1,1] porque la fórmula sin clamp tiende a -2.72. | Se conserva la fórmula en el resto del rango; el clamp entra en la curva. |
| D2 | `TRIM[CLEAN] = 0.279824` y CLEAN **no recibe drive** (v2.2.1; sustituye el contrato v2.2.0 de `TRIM[CLEAN] = 1.0`, que el reporte de usuario invalidó: Clean subía +26.6…+31.9 dB a 5 V). | Normalización de seguridad impuesta: igualar RMS con TANH en la referencia y nunca superar +1.0 dB. Medido (§9.7): 5 V → +0.510/−0.000/−0.043 dB a 0/50/100 %; 1 V → −9.269/−13.579/−13.813 dB. CLEAN lineal (THD < −200 dB, sin clamp). Null test nuevo: salida = TRIM_CLEAN × entrada bit-exacto. |
| D3 | El punto de calibración 5 V / drive 50% (g=6, u=30) satura todas las curvas; el criterio de ±1 dB a drive 50% se cumple por construcción. | El dato útil es a drive 0% y 100%. `CLEAN` queda exento por ser lineal. |
| D4 | El null test exacto se define a nivel de curva; a nivel de módulo la referencia es la suma lineal de bandas (el ripple del crossover C7 es previo). | — |
| D5 | El DC-blocker de 15 Hz implica ~1.9 dB a 20 Hz en el camino wet; la baseline C-7 "~0 dB a 20 Hz" solo vale en dry o con el blocker descontado. | — |
| D6 | El DC-blocker va a frecuencia base, tras `processBands()` (suma+decimación) y antes del mix; R = exp(-2*pi*15/fs) con fs = sampleRate. | — |
| D7 | `CUBIC` tiene `f'(0) = 1.5`, no ~1: el factor 1.5 que lleva el límite a ±1 también escala la pendiente. | Se mantiene la fórmula del contrato; no se renormaliza. Consecuencia medida: THD a drive 0% = 58.74 % (vs 33.36 % de TANH), así que Cubic distorsiona más que Tanh a bajo nivel. El TRIM solo iguala nivel a saturación, no la pendiente. |

---

## 5. Ubicación exacta en el código

1. `Saturator::processBands<Stage>` sustituye los 6 `std::tanh` por `applyCurve(curveDrive(banda, drive, curve), curve)`.
2. `OSParams` (en `Saturator.hpp`) lleva `curveLow`, `curveMid`, `curveHigh` resueltos una vez por bloque.
3. Estado: `std::atomic<int> curves[3]` (índices: 0 = Low, 1 = Mid, 2 = High).
4. `Saturator::setCurve(band, id)` valida `0..CURVE_COUNT-1`.
5. `satMeter = min(|y_out - u|, 1)` con `y_out = applyCurve(u)` (post-TRIM) y `u = curveDrive(banda, drive, curve)` (v2.2.1). Con CLEAN el medidor muestra ~|1 − TRIM[CLEAN]|·|banda| (offset del escalado, esperado). Las luces siguen sin cablear (C4), así que hoy no tiene efecto visible.

Cadena v2.2.1 (tras sumar bandas, antes del mix):

```
processBands (crossovers + applyCurve + pan/vol + suma + decimación)
   -> DC-blocker 15 Hz (R = exp(-2*pi*15/fs), fs = sampleRate; SOLO wet)
   -> mix dry/wet
   -> OUTPUT_VOL
```

---

## 6. Esquema JSON y compatibilidad

- `dataToJson()`: raíz con `"curves": [low, mid, high]` (array de 3 enteros).
- `dataFromJson()`: sin la clave -> TANH (default); valores fuera de rango -> clamp; array corto -> TANH en las
  bandas que falten.

---

## 7. Menú contextual

- Tres submenús de primer nivel: `"Low Curve"`, `"Mid Curve"`, `"High Curve"`.
- `createIndexSubmenuItem(curveNames(), getter, setter)`, radio check, mismo patrón que `"Oversampling"`.
- Menú no undoable.

---

## 8. Protocolo de medición offline y criterios de aceptación verificables

Arnés offline (sin Rack): 1 kHz, 5 V, drives 0/50/100%, THD/H2/H3/RMS/DC; alias 5 kHz a 1x/4x/8x, cadena
completa.

Criterios de aceptación (copia del contrato):

1. Build limpio (`export PATH=/mingw64/bin:$PATH; RACK_DIR=/c/Rack-SDK2.6.6 make clean && make`) -> 0 errores,
   0 warnings; `make dist` genera el `.vcvplugin`.
2. Default: módulo nuevo sin patch -> `TANH`; los tres submenús marcan la curva por defecto.
3. UI: clic derecho -> submenús de curva con radio check; seleccionar cambia check y audio en el siguiente
   bloque.
4. Contrato de curva: `f(0)=0`, `f'(0)~1` (Tube 1.46, Diode 1.0 en 0 pero asimétrica), `|f|<=1` antes de
   TRIM; CLEAN: `applyCurve(curveDrive(x, g, CLEAN), CLEAN) == x * 0.279824f` bit-exacto (100k floats;
   g = 1, 6, 11; tests T1–T4 de §9.7) y lineal (THD < −200 dB, sin clamp).
5. THD: criterio de ±1 dB a drive 50% se cumple por construcción (D3); dato útil a drive 0% y 100%; CLEAN
   normalizada (§9.7): a 5 V, |ΔRMS| ≤ 1.0 dB vs TANH en 0/50/100 % y nunca > +1.0 dB; a 1 V nunca > +1.0 dB.
6. JSON: guardar con curvas -> recargar -> igual; sin la clave -> TANH; fuera de rango -> clamp; array corto
   -> TANH en las bandas que falten.
7. Compatibilidad: `git diff` no muestra cambios en enums de params/inputs/outputs/lights.

---

## 9. Mediciones

### 9.1 Metodología

Arnés offline (sin Rack): `src/Curves.hpp` (header-only, sin dependencia de Rack). Condiciones: fs base
**48 kHz**, calidad del FIR **Q = 16**, corte **OS_CUTOFF = 0.95**. El **TRIM ya está insertado en
`src/Curves.hpp`** (ver §2 y Tabla B).

Mediciones **intrínsecas** (sin alias): fs = **384 kHz** (8x del fs base), 1 kHz, 5 V pico, THD% =
`sqrt(sum H2..H12) / H1`. Gains: drive 0% ~ g=1; drive 50% ~ g=6; drive 100% ~ g=11 (D3).

### 9.2 Tabla A — THD / H2 / H3 / RMS / DC intrínsecos (sin alias)

Cada celda: **THD % / H2 (dB) / H3 (dB) / RMS / DC**.

| Curva | g=1 (drive 0%) | g=6 (drive 50%) | g=11 (drive 100%) |
|---|---:|---:|---:|
| Clean | 0.00 / -325.0 / -172.2 / 3.5355 / 0 | 0.00 / -325.2 / -177.3 / 21.2132 / 0 | 0.00 / -332.4 / -169.1 / 38.8909 / 0 |
| Tanh | 33.36 / -339.4 / -10.69 / 0.9329 / 0 | 43.34 / -326.6 / -9.57 / 0.9893 / 0 | 43.68 / -321.3 / -9.55 / 0.9942 / 0 |
| Cubic | 58.74 / -323.0 / -34.94 / 0.9301 / 0 | 40.73 / -321.0 / -9.75 / 0.9897 / 0 | 42.79 / -315.7 / -9.61 / 0.9922 / 0 |
| Arctan | 29.68 / -326.3 / -11.75 / 0.8273 / 0 | 40.72 / -321.3 / -9.91 / 0.9574 / 0 | 42.09 / -320.0 / -9.74 / 0.9740 / 0 |
| Hard | 37.84 / -338.4 / -10.02 / 0.9564 / 0 | 43.65 / -327.4 / -9.55 / 0.9932 / 0 | 43.78 / -320.6 / -9.55 / 0.9964 / 0 |
| Foldback | 217.51 / -168.4 / +2.55 / 0.6403 / 0 | 145.78 / -150.4 / -0.44 / 0.5611 / 0 | 282.02 / -140.6 / +1.07 / 0.5955 / 0 |
| Sine Fold | 137.94 / -330.1 / +0.94 / 0.7893 / 0 | 224.99 / -316.3 / +0.73 / 0.7387 / 0 | 280.83 / -307.0 / +0.56 / 0.7267 / 0 |
| Tube | 38.52 / -31.50 / -10.05 / 0.9589 / -0.018532 | 43.62 / -46.36 / -9.56 / 0.9934 / -0.003067 | 43.77 / -53.53 / -9.55 / 0.9962 / -0.001342 |
| Diode | 32.80 / -21.43 / -11.30 / 0.7149 / +0.199117 | 43.14 / -35.59 / -9.60 / 0.7791 / +0.242029 | 43.62 / -40.81 / -9.56 / 0.7843 / +0.245643 |

Salvedades: los THD y H2/H3 son invariantes con TRIM; los RMS/DC de esta tabla son sin trim, medidos con el header a 1.0. La tabla de RMS con trim va aparte (Tabla B).

### 9.3 Tabla B — TRIM insertados en `src/Curves.hpp`

| Curva | TRIM |
|---|---:|
| Clean | 0.279824 (v2.2.1: RMS_TANH/(5/√2); ruta seca sin drive) |
| Tanh | 1.0000 |
| Cubic | 0.9996 |
| Arctan | 1.0334 |
| Hard | 0.9961 |
| Foldback | 1.7631 |
| Sine Fold | 1.3392 |
| Tube | 0.9959 |
| Diode | 1.2698 |

Verificación post-trim: RMS a g=6 = 0.9893 en las 8 curvas no-CLEAN (TRIM sugerido 1.0000). CLEAN (v2.2.1, §9.7): a 5 V, Δ vs TANH = +0.510/−0.000/−0.043 dB a 0/50/100 % (nunca > +1.0); a 1 V, −9.269/−13.579/−13.813 dB (nunca > +1.0). Bit tests: applyCurve(curveDrive(x, g, CLEAN), CLEAN) == x * 0.279824f, 0 diferencias en 100k floats para g = 1, 6, 11; THD < −200 dB; ganancia constante (Δ 5 V vs 1 V = 0.0000 dB).

### 9.4 Tabla C — Desviación RMS respecto a TANH con TRIM aplicado

Cada celda: desviación de RMS en dB respecto a TANH (TRIM aplicado); positivo = más fuerte.

| Curva | g=1 | g=6 | g=11 |
|---|---:|---:|---:|
| Cubic | -0.03 | 0.00 | -0.02 |
| Arctan | -0.76 | 0.00 | +0.11 |
| Hard | +0.18 | 0.00 | -0.01 |
| Foldback | +1.66 | 0.00 | +0.47 |
| Sine Fold | +1.09 | 0.00 | -0.19 |
| Tube | +0.20 | 0.00 | -0.02 |
| Diode | -0.24 | 0.00 | +0.01 |
| CLEAN | +0.51 / −0.00 / −0.04 a 5 V; −9.27 / −13.58 / −13.81 a 1 V (v2.2.1, §9.7). Antes del fix: +11.57 / +26.63 / +31.85 dB a 5 V |

Conclusión: ≤1 dB en todas las no-CLEAN a drive 50% y 100%; a drive 0% Foldback +1.66 dB y Sine Fold +1.09 dB (leve exceso, documentado).

### 9.5 Tabla D — Alias de la cadena completa (5 kHz, 5 V, drive 11, peor componente no armónica <20 kHz relativa al fundamental de salida), N=1x/4x/8x

| Curva | N=1x | N=4x | N=8x |
|---|---:|---:|---:|
| Clean | -95.3 | -95.3 | -95.3 (fuga de ventana, no distorsión) |
| Tanh | -4.2 | -25.9 | -37.1 |
| Cubic | -8.8 | -22.8 | -24.0 |
| Arctan | -5.2 | -28.4 | -38.0 |
| Hard | -3.9 | -24.2 | -33.2 |
| Foldback | +5.3 | -8.2 | -8.5 |
| Sine Fold | +1.2 | -6.4 | -89.5 |
| Tube | -2.6 | -23.5 | -30.6 |
| Diode | -4.0 | -26.9 | -37.3 |

### 9.6 Nota metodológica (arnés v2.2.0)

El arnés nuevo mide la **cadena completa de 3 bandas sumadas**. El valor **-32.8 dB** de v2.1.0 **no es
comparable** (parece medido sobre banda alta / curva aislada). Prueba: **Tanh**, bit-idéntico a v2.1.0, da
**-25.9 dB** en este arnés. La diferencia es de **método, no regresión**.

Cancelación de fase a 5 kHz entre las bandas BP (1000 Hz) y HP (4000 Hz): la fundamental suma ~0.5 en
vez de ~1.27 por banda; por eso el ratio alias/fundamental empeora.

### 9.7 CLEAN v2.2.1 — normalización de seguridad (decisión B)

Reporte de usuario: con Clean el volumen sube mucho. Causa medida: CLEAN recibía
`u = banda * drive` (boost lineal: +26.63 dB sobre TANH a 5 V/g=6, +31.85 dB a g=11).

Decisión: **CLEAN no recibe drive** (`curveDrive()` devuelve la banda) y
`TRIM[CLEAN] = 0.279824f = RMS_TANH(5 V, g=6) / (5/√2)`, con RMS_TANH sin redondear
= 0.9893278586. La salida es `0.279824 × banda`, lineal, sin clamp (THD < −200 dB;
ganancia constante 0.279823987 a 5 V y 0.279823988 a 1 V — Δ 0.0000 dB).

Niveles medidos (Δ RMS vs TANH; arnés fs 384 kHz, 1 kHz, N=15360, descarte 4096):

| A | 0 % | 50 % | 100 % |
|---|---:|---:|---:|
| 5 V | +0.510 | −0.000 | −0.043 |
| 1 V | −9.269 | −13.579 | −13.813 |

Criterio de aceptación (obligatorio): a 5 V, |Δ| ≤ 1.0 dB y nunca Δ > +1.0 dB; a 1 V,
nunca Δ > +1.0 dB. PASS en las 6 celdas. Pico: CLEAN 1.3991 a 5 V < máximo de las
demás curvas 1.7631 (Foldback).

Tests T1–T4: (T1) null nuevo: applyCurve(x, CLEAN) == x * 0.279824f bit-exacto (100k
floats, 0 diferencias); (T2) invariancia al drive: lo mismo con curveDrive(x, g, CLEAN)
para g = 1, 6, 11 (0 diferencias); (T3) tabla de arriba all-PASS; (T4) THD < −200 dB a
g = 1, 6, 11 y Δ de ganancia 5 V vs 1 V ≤ 0.0005 dB.

Compromiso declarado: una recta escalada no puede igualar la compresión de nivel de
una saturante a todos los niveles (ganancia efectiva de TANH: 1.3366 × a 1 V, 0.2798 ×
a 5 V a g=6). CLEAN clava el punto de seguridad (5 V, nominal VCV, todo drive) y **queda
−11.06 dB respecto a la banda seca**: ya no es ruta de bypass/null; la transparencia
opt-in de C5 pierde esa acepción (documentado en el C5 del spec principal).

---

## 10. Riesgos

| Riesgo | Detalle / mitigación |
|---|---|
| Alias (cadena completa) | **Medido** (arnés offline, 5 kHz / 5 V / drive 11, N=1x/4x/8x): a 4x, Cubic -22.8 dB, Hard -24.2 dB, Foldback -8.2 dB, Sine Fold -6.4 dB y Tube -23.5 dB; a 8x, Foldback -8.5 dB. ADAA fuera de alcance (fase 2). |
| DC-blocker | A 15 Hz, primera orden, solo wet: ≈ -1.9 dB a 20 Hz (D5); Diode deja DC +0.242 (24% del pico) a g=6 sin blocker y el blocker lo elimina. |
| Click al cambiar curva | Crossfade fuera de alcance; click aceptado (control de setup). |
| Coste del switch | Despreciable frente a `tanh`. |
| Smoke test GUI | Imposible de automatizar. |
| Cubic a bajo nivel | `f'(0)=1.5` (D7): a drive 0% su THD es 58.74 % frente a 33.36 % de Tanh. Es el precio de mantener la fórmula del contrato; documentarlo al usuario. |

---

## 11. Trazabilidad y alcance

- **No cierra C5** (sigue sin bypass): mitiga añadiendo transparencia opt-in con la curva Clean. Además
  actualiza C5/C6/C7/C8 en `specs/multiband_saturator_spec.md`.
- Versión: 2.2.1.

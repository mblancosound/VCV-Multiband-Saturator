#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace coaldynamics {

/** Curvas de saturacion por banda. Header-only y sin dependencia de Rack:
    el arnes offline de la fase 1 compila este fichero tal cual. */
enum CurveId {
  CLEAN = 0,
  TANH,
  CUBIC,
  ARCTAN,
  HARD,
  FOLDBACK,
  SINEFOLD,
  TUBE,
  DIODE,
  CURVE_COUNT
};

/** Trim estatico por curva (make-up medido, ver specs/curves_spec.md):
    1 kHz, 5 V de pico, drive 50 % (g = 6), TRIM = RMS_TANH / RMS_curva.
    CLEAN (v2.2.1): normalizado por seguridad al RMS de TANH en la
    referencia. RMS_TANH(5 V, g = 6) = 0.9893278586; TRIM[CLEAN] =
    0.9893278586 / (5/sqrt(2)) = 0.279824. CLEAN no recibe drive (ver
    curveDrive()); ver specs/curves_spec.md 9.7. */
static const float TRIM[CURVE_COUNT] = {
    0.279824f, 1.0000f, 0.9996f, 1.0334f, 0.9961f, 1.7631f, 1.3392f,
    0.9959f, 1.2698f};

inline float clampBipolar(float x) {
  if (x > 1.f)
    return 1.f;
  if (x < -1.f)
    return -1.f;
  return x;
}

/** Aplica la curva `curve` a u (banda filtrada * drive) y devuelve la salida
    ya multiplicada por TRIM. Un id invalido cae a TANH (valor por defecto). */
inline float applyCurve(float u, int curve) {
  float y;
  switch (curve) {
    case CLEAN:
      y = u;
      break;
    case TANH:
      y = std::tanh(u);
      break;
    case CUBIC: {
      float c = u - u * u * u / 3.f;
      if (c > 2.f / 3.f)
        c = 2.f / 3.f;
      if (c < -2.f / 3.f)
        c = -2.f / 3.f;
      y = c * 1.5f;
      break;
    }
    case ARCTAN: {
      const float pi = 3.14159265358979f;
      y = (2.f / pi) * std::atan(u * pi / 2.f);
      break;
    }
    case HARD:
      y = clampBipolar(u);
      break;
    case FOLDBACK: {
      float t = std::fmod(u + 1.f, 4.f);
      if (t < 0.f)
        t += 4.f;
      if (t > 2.f)
        t = 4.f - t;
      y = t - 1.f;
      break;
    }
    case SINEFOLD:
      y = std::sin(u);
      break;
    case TUBE: {
      // La formula sin clamp llega a -2.72 por abajo; el contrato exige
      // |f| <= 1, asi que se recorta (desviacion documentada en la spec).
      const float b = 0.5f;
      const float tb = std::tanh(b);
      y = clampBipolar((std::tanh(u + b) - tb) / (1.f - tb));
      break;
    }
    case DIODE: {
      const float c = 0.5f;
      y = (u >= 0.f) ? (1.f - std::exp(-u))
                    : -(1.f - std::exp(u / c)) * c;
      break;
    }
    default:
      y = std::tanh(u);
      curve = TANH;
      break;
  }
  return y * TRIM[curve];
}

/** Entrada de curva por banda: CLEAN es la ruta seca escalada y NO recibe
    drive (TRIM[CLEAN] ya fija su nivel; ver specs/curves_spec.md 9.7); el
    resto de curvas recibe banda * drive. Usar SIEMPRE este par, tambien en
    el medidor:
      applyCurve(curveDrive(banda, drive, curve), curve) */
inline float curveDrive(float banda, float drive, int curve) {
  return (curve == CLEAN) ? banda : banda * drive;
}

/** Umbral del fallback ADAA1, en unidades de u (voltios post-curveDrive). */
static const float ADAA_EPS = 1e-3f;

inline float logCosh(float u) {
  const float a = std::fabs(u);
  if (a < 0.5f) {
    const float s = std::sinh(0.5f * a);
    return std::log1p(2.f * s * s);              // ln(1 + 2 sinh^2(a/2))
  }
  return a + std::log1p(std::exp(-2.f * a)) - 0.6931471805599453f;
}

inline bool curveSupportsAdaa(int curve) {
  return curve == TANH || curve == CUBIC || curve == ARCTAN ||
         curve == HARD || curve == FOLDBACK || curve == SINEFOLD;
}

inline float curveAntiderivative(float u, int curve) {
  float y;
  switch (curve) {
    case TANH:
      y = logCosh(u);
      break;
    case CUBIC: {
      const float a = std::fabs(u);
      y = (a <= 2.f) ? a * a * (0.75f - 0.125f * a * a) : (3.f - a);
      break;
    }
    case ARCTAN: {
      const float pi = 3.14159265358979f;
      const float w = 0.5f * pi * u;
      y = (4.f / (pi * pi)) * (w * std::atan(w) - 0.5f * std::log1p(w * w));
      break;
    }
    case HARD: {
      const float a = std::fabs(u);
      y = (a <= 1.f) ? 0.5f * a * a : (a - 0.5f);
      break;
    }
    case FOLDBACK: {
      float p = std::fmod(u, 4.f);
      if (p < 0.f)
        p += 4.f;
      if (p <= 1.f)
        y = 0.5f * p * p;
      else if (p <= 3.f)
        y = 2.f * p - 0.5f * p * p - 1.f;
      else
        y = 0.5f * p * p - 4.f * p + 8.f;
      break;
    }
    case SINEFOLD:
      y = -std::cos(u);
      break;
    default:
      y = logCosh(u);
      curve = TANH;
      break;
  }
  return y * TRIM[curve];
}

inline float applyCurveAdaa(float u, float &uPrev, int curve) {
  if (!curveSupportsAdaa(curve))
    return applyCurve(u, curve);                 // CLEAN/TUBE/DIODE: directa, SIN tocar uPrev
  const float du = u - uPrev;
  float y;
  if (std::fabs(du) >= ADAA_EPS)
    y = (curveAntiderivative(u, curve) -
         curveAntiderivative(uPrev, curve)) / du;
  else
    y = applyCurve(u, curve);                    // fallback epsilon
  uPrev = u;
  return y;
}

/** Nombres para el menu contextual, en el orden del enum. */
inline const std::vector<std::string> &curveNames() {
  static const std::vector<std::string> names = {
      "Clean", "Tanh",  "Cubic",     "Arctan", "Hard",
      "Foldback", "Sine Fold", "Tube", "Diode"};
  return names;
}

}  // namespace coaldynamics
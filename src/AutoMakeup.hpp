#pragma once

#include <cmath>

namespace coaldynamics {

/** Umbral de "hay senal" del detector, en voltios (media |x| de banda).
    1e-3 V = -60 dBV. Por debajo el makeup es inaudible y gainS se congela. */
static const float AUTO_MAKEUP_GATE = 1e-3f;

/** Auto-makeup por banda (G2). Header-only, C++11, sin Rack, sin allocation,
    sin I/O: lo usan el modulo y el arnes offline. */
struct AutoMakeup {
  static const int BANDS = 3;

  float coefAttack;
  float coefRelease;
  float coefSmooth;
  float envIn[BANDS];
  float envOut[BANDS];
  float gainS[BANDS];

  AutoMakeup() {
    setSampleRate(48000.f);
    reset();
  }

  void setSampleRate(float sampleRate) {
    const float fs = (sampleRate > 0.f) ? sampleRate : 48000.f;
    coefAttack = 1.f - std::exp(-1.f / (fs * 0.010f));
    coefRelease = 1.f - std::exp(-1.f / (fs * 0.100f));
    coefSmooth = 1.f - std::exp(-1.f / (fs * 0.010f));
  }

  void reset() {
    for (int b = 0; b < BANDS; b++) {
      envIn[b] = 0.f;
      envOut[b] = 0.f;
      gainS[b] = 1.f;
    }
  }

  /** dIn/dOut: media de |senal| por banda, ya sin signo y >= 0. Se llama UNA
      vez por muestra base, nunca por submuestra de oversampling. */
  void update(const float dIn[BANDS], const float dOut[BANDS]) {
    for (int b = 0; b < BANDS; b++) {
      // Guarda de finitud: NaN o negativo -> 0. Es la identidad en el unico
      // dominio que produce el modulo (|L|,|R| >= 0 finitos) y evita que un
      // NaN de entrada envenene las envolventes para siempre.
      const float xi = (dIn[b] > 0.f) ? dIn[b] : 0.f;
      const float xo = (dOut[b] > 0.f) ? dOut[b] : 0.f;
      const float aI = (xi > envIn[b]) ? coefAttack : coefRelease;
      const float aO = (xo > envOut[b]) ? coefAttack : coefRelease;
      envIn[b] += aI * (xi - envIn[b]);
      envOut[b] += aO * (xo - envOut[b]);
      if (envIn[b] < AUTO_MAKEUP_GATE)
        continue;                       // silencio: gainS se congela
      const float g = envIn[b] / (envOut[b] + 1e-6f);
      if (!(g >= 0.f))
        continue;
      gainS[b] += coefSmooth * ((g > 4.f ? 4.f : g) - gainS[b]);
    }
  }

  /** Precondicion: 0 <= band < BANDS. Sin comprobacion en audio. */
  float gain(int band) const { return gainS[band]; }
};

} // namespace coaldynamics

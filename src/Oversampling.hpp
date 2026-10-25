#pragma once

#include <rack.hpp>

namespace coaldynamics {

// Calidad del FIR de sobremuestreo.
static const int OS_QUALITY = 16;
// Corte del FIR relativo al Nyquist de la frecuencia base (0.95 = 22.8 kHz a 48 kHz).
static const float OS_CUTOFF = 0.95f;
// Latencia en muestras base del par up+down con Q = OS_QUALITY (medida: 15).
static const int OS_LATENCY = OS_QUALITY - 1;

// Convierte a N*fs, procesa a N*fs y vuelve a fs.
// La interfaz es identica para todos los factores para escribir el bucle DSP una sola vez.
template <int N, int Q>
struct OversampledStage {
  static const int FACTOR = N;
  rack::dsp::Upsampler<N, Q> upL;
  rack::dsp::Upsampler<N, Q> upR;
  rack::dsp::Decimator<N, Q> decL;
  rack::dsp::Decimator<N, Q> decR;

  OversampledStage()
      : upL(OS_CUTOFF),
        upR(OS_CUTOFF),
        decL(OS_CUTOFF),
        decR(OS_CUTOFF) {}

  void reset() {
    upL.reset();
    upR.reset();
    decL.reset();
    decR.reset();
  }

  void upsample(float l, float r, float *outL, float *outR) {
    upL.process(l, outL);
    upR.process(r, outR);
  }

  float downsampleL(float *in) { return decL.process(in); }
  float downsampleR(float *in) { return decR.process(in); }
};

// Etapa de 1x: sin conversion ni latencia.
template <int Q>
struct BypassStage {
  static const int FACTOR = 1;

  void reset() {}

  void upsample(float l, float r, float *outL, float *outR) {
    outL[0] = l;
    outR[0] = r;
  }

  float downsampleL(float *in) { return in[0]; }
  float downsampleR(float *in) { return in[0]; }
};

}  // namespace coaldynamics
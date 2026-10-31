#pragma once

#include <dsp/resampler.hpp>

namespace cdt {

// Replica offline de src/Oversampling.hpp (no se puede incluir porque arrastra
// <rack.hpp> y no enlaza sin libRack). Si cambia OS_QUALITY, OS_CUTOFF o la
// topologia en src/Oversampling.hpp, actualizar aqui: el caso T-LAT del arnes
// mide la latencia real por impulso y falla si deja de ser 15.
static const int LOCAL_OS_QUALITY = 16;
static const float LOCAL_OS_CUTOFF = 0.95f;
static const int LOCAL_OS_LATENCY = 15;

template <int N>
struct LocalStage {
  static const int FACTOR = N;

  rack::dsp::Upsampler<N, LOCAL_OS_QUALITY> upL;
  rack::dsp::Upsampler<N, LOCAL_OS_QUALITY> upR;
  rack::dsp::Decimator<N, LOCAL_OS_QUALITY> decL;
  rack::dsp::Decimator<N, LOCAL_OS_QUALITY> decR;

  LocalStage()
      : upL(LOCAL_OS_CUTOFF), upR(LOCAL_OS_CUTOFF),
        decL(LOCAL_OS_CUTOFF), decR(LOCAL_OS_CUTOFF) {}

  void reset() { upL.reset(); upR.reset(); decL.reset(); decR.reset(); }

  void upsample(float l, float r, float *outL, float *outR) {
    upL.process(l, outL);
    upR.process(r, outR);
  }

  float downsampleL(float *in) { return decL.process(in); }
  float downsampleR(float *in) { return decR.process(in); }
};

struct LocalBypassStage {
  static const int FACTOR = 1;

  void reset() {}
  void upsample(float l, float r, float *outL, float *outR) {
    outL[0] = l;
    outR[0] = r;
  }
  float downsampleL(float *in) { return in[0]; }
  float downsampleR(float *in) { return in[0]; }
};

}  // namespace cdt
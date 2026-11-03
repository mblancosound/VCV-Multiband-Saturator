#pragma once

#include <algorithm>
#include <cmath>

#include <dsp/filter.hpp>

#include "../../src/AutoMakeup.hpp"
#include "../../src/Curves.hpp"
#include "oversample_local.hpp"

namespace cdt {

// Parametros de una llamada a process(), en las MISMAS unidades que OSParams
// en src/Saturator.cpp: drive ya es 1 + knob*10, pan y vol ya llevan su CV
// sumado y su clamp aplicado.
struct ChainParams {
  float driveLow, driveMid, driveHigh;
  int curveLow, curveMid, curveHigh;
  float panLow, panMid, panHigh;
  float volLow, volMid, volHigh;
  int soloBand;
  float mix;
  float outVol;
  /** 0 = crossover LEGACY (v2.5.0), 1 = LR4 (v2.6.0). */
  int lr4;
  /** 0 = ADAA off (v2.5.0), 1 = ADAA1 on (v2.6.0). */
  int adaa;
  /** Makeup manual por banda (G2). 1.0 = 0 dB. */
  float makeupLow, makeupMid, makeupHigh;
  /** 0 = auto-makeup off (v2.6.0), 1 = on (G2). */
  int autoMakeup;

  ChainParams()
      : driveLow(1.f), driveMid(1.f), driveHigh(1.f),
        curveLow(coaldynamics::TANH), curveMid(coaldynamics::TANH),
        curveHigh(coaldynamics::TANH),
        panLow(0.f), panMid(0.f), panHigh(0.f),
        volLow(1.f), volMid(1.f), volHigh(1.f),
        soloBand(-1), mix(1.f), outVol(1.f), lr4(0), adaa(0),
        makeupLow(1.f), makeupMid(1.f), makeupHigh(1.f), autoMakeup(0) {}
};

// Replica offline de Saturator::process(). Copia la topologia y los
// coeficientes de src/Saturator.cpp: filtros a fs*factor (250 LP Q 0.707,
// 1000 BP Q 1.0, 4000 HP Q 0.707), curva por banda, pan/vol/solo, DC blocker
// de 15 Hz solo en el camino wet, buffer de dry de OS_LATENCY+1 con dryDelay
// y mezcla final con outVol.
// ATENCION: si cambia la topologia o los cortes en src/Saturator.cpp hay que
// actualizar este fichero; el caso T-LAT mide la latencia real por impulso y
// falla si deja de ser 15 en N>1 y 0 en 1x.
struct OfflineChain {
  LocalBypassStage stage1;
  LocalStage<2> stage2;
  LocalStage<4> stage4;
  LocalStage<8> stage8;

  rack::dsp::BiquadFilter lowL, lowR, midL, midR, highL, highR;
  // Segunda etapa de la cascada LR4 (v2.6.0); en LEGACY no se usan.
  rack::dsp::BiquadFilter lowL2, lowR2, highL2, highR2;

  float sampleRate;
  int factor;
  float dcR;
  float dcX1L, dcX1R, dcY1L, dcY1R;
  float dryBufferL[LOCAL_OS_LATENCY + 1];
  float dryBufferR[LOCAL_OS_LATENCY + 1];
  int dryIndex;
  int dryDelay;
  float soloGain[3];
  float dryGain;

  /** Replica del historial ADAA1 de src/Saturator.hpp (v2.6.0): mismos
      nombres que el modulo. u_prev por banda (Low/Mid/High) y canal. */
  float uPrevLowL, uPrevLowR;
  float uPrevMidL, uPrevMidR;
  float uPrevHighL, uPrevHighR;
  bool adaaResync[3];
  int adaaCurveApplied[3];

  ChainParams p;

  /** Mismo objeto que usa el modulo (src/AutoMakeup.hpp), no una copia. */
  coaldynamics::AutoMakeup autoMakeupDsp;

  OfflineChain() { configure(48000.f, 1); }

  // Reajusta sample rate y factor, recalcula todos los coeficientes y
  // resetea el estado (equivale a onSampleRateChange + applyOversampling).
  void configure(float fs, int f) {
    sampleRate = fs;
    factor = f;
    const float fso = fs * static_cast<float>(f);

    lowL.setParameters(rack::dsp::BiquadFilter::LOWPASS, 250.f / fso, 0.707f, 1.f);
    lowR.setParameters(rack::dsp::BiquadFilter::LOWPASS, 250.f / fso, 0.707f, 1.f);
    midL.setParameters(rack::dsp::BiquadFilter::BANDPASS, 1000.f / fso, 1.0f, 1.f);
    midR.setParameters(rack::dsp::BiquadFilter::BANDPASS, 1000.f / fso, 1.0f, 1.f);
    highL.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 4000.f / fso, 0.707f, 1.f);
    highR.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 4000.f / fso, 0.707f, 1.f);

    // LR4 (v2.6.0): segunda etapa de la cascada a fc*sqrt(2) y Q = 0.707,
    // identico a src/Saturator.cpp updateFilterFrequencies().
    lowL2.setParameters(rack::dsp::BiquadFilter::LOWPASS, 353.55339f / fso,
                        0.707f, 1.f);
    lowR2.setParameters(rack::dsp::BiquadFilter::LOWPASS, 353.55339f / fso,
                        0.707f, 1.f);
    highL2.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 5656.85425f / fso,
                         0.707f, 1.f);
    highR2.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 5656.85425f / fso,
                         0.707f, 1.f);

    stage1.reset();
    stage2.reset();
    stage4.reset();
    stage8.reset();

    dcR = std::exp(-6.283185307179586f * 15.f / fs);
    dcX1L = 0.f;
    dcX1R = 0.f;
    dcY1L = 0.f;
    dcY1R = 0.f;

    for (int i = 0; i <= LOCAL_OS_LATENCY; ++i) {
      dryBufferL[i] = 0.f;
      dryBufferR[i] = 0.f;
    }
    dryIndex = 0;
    dryDelay = (f > 1) ? LOCAL_OS_LATENCY : 0;

    soloGain[0] = 1.f;
    soloGain[1] = 1.f;
    soloGain[2] = 1.f;
    dryGain = 1.f;

    uPrevLowL = uPrevLowR = 0.f;
    uPrevMidL = uPrevMidR = 0.f;
    uPrevHighL = uPrevHighR = 0.f;
    adaaResync[0] = adaaResync[1] = adaaResync[2] = true;
    adaaCurveApplied[0] = adaaCurveApplied[1] = adaaCurveApplied[2] = -1;

    autoMakeupDsp.setSampleRate(fs);
    autoMakeupDsp.reset();
  }

  template <class Stage>
  void processBands(Stage &stage, float inL, float inR, float &outL,
                    float &outR) {
    const int N = Stage::FACTOR;
    float upL[8];
    float upR[8];
    float wetL[8];
    float wetR[8];

    stage.upsample(inL, inR, upL, upR);
    const int adaaOn = p.adaa;

    // Makeup resuelto una vez por muestra base, igual que OSParams en el
    // modulo: auto manda; con auto off, knob manual (default 1.0).
    float mk[3];
    mk[0] = p.autoMakeup ? autoMakeupDsp.gain(0) : p.makeupLow;
    mk[1] = p.autoMakeup ? autoMakeupDsp.gain(1) : p.makeupMid;
    mk[2] = p.autoMakeup ? autoMakeupDsp.gain(2) : p.makeupHigh;

    for (int i = 0; i < N; ++i) {
      const float xL = upL[i];
      const float xR = upR[i];

      float lowLf, lowRf, midLf, midRf, highLf, highRf;
      if (p.lr4) {
        // Low = LP4 y High = HP4 en cascada; Mid = x - low - high, de modo
        // que low + mid + high reconstruye la entrada.
        lowLf = lowL2.process(lowL.process(xL));
        lowRf = lowR2.process(lowR.process(xR));
        highLf = highL2.process(highL.process(xL));
        highRf = highR2.process(highR.process(xR));
        midLf = xL - lowLf - highLf;
        midRf = xR - lowRf - highRf;
      } else {
        lowLf = lowL.process(xL);
        lowRf = lowR.process(xR);
        midLf = midL.process(xL);
        midRf = midR.process(xR);
        highLf = highL.process(xL);
        highRf = highR.process(xR);
      }

      const float uLowL = coaldynamics::curveDrive(lowLf, p.driveLow, p.curveLow);
      const float uLowR = coaldynamics::curveDrive(lowRf, p.driveLow, p.curveLow);
      const float uMidL = coaldynamics::curveDrive(midLf, p.driveMid, p.curveMid);
      const float uMidR = coaldynamics::curveDrive(midRf, p.driveMid, p.curveMid);
      const float uHighL =
          coaldynamics::curveDrive(highLf, p.driveHigh, p.curveHigh);
      const float uHighR =
          coaldynamics::curveDrive(highRf, p.driveHigh, p.curveHigh);

      // Resync ADAA: uPrev = u en la primera submuestra del bloque.
      if (adaaOn && i == 0) {
        if (adaaResync[0]) {
          uPrevLowL = uLowL;
          uPrevLowR = uLowR;
          adaaResync[0] = false;
        }
        if (adaaResync[1]) {
          uPrevMidL = uMidL;
          uPrevMidR = uMidR;
          adaaResync[1] = false;
        }
        if (adaaResync[2]) {
          uPrevHighL = uHighL;
          uPrevHighR = uHighR;
          adaaResync[2] = false;
        }
      }

      float wetLowL, wetLowR, wetMidL, wetMidR, wetHighL, wetHighR;
      if (adaaOn) {
        wetLowL = coaldynamics::applyCurveAdaa(uLowL, uPrevLowL, p.curveLow);
        wetLowR = coaldynamics::applyCurveAdaa(uLowR, uPrevLowR, p.curveLow);
        wetMidL = coaldynamics::applyCurveAdaa(uMidL, uPrevMidL, p.curveMid);
        wetMidR = coaldynamics::applyCurveAdaa(uMidR, uPrevMidR, p.curveMid);
        wetHighL = coaldynamics::applyCurveAdaa(uHighL, uPrevHighL, p.curveHigh);
        wetHighR = coaldynamics::applyCurveAdaa(uHighR, uPrevHighR, p.curveHigh);
      } else {
        wetLowL = coaldynamics::applyCurve(uLowL, p.curveLow);
        wetLowR = coaldynamics::applyCurve(uLowR, p.curveLow);
        wetMidL = coaldynamics::applyCurve(uMidL, p.curveMid);
        wetMidR = coaldynamics::applyCurve(uMidR, p.curveMid);
        wetHighL = coaldynamics::applyCurve(uHighL, p.curveHigh);
        wetHighR = coaldynamics::applyCurve(uHighR, p.curveHigh);
      }

      const float lowOutL = wetLowL * mk[0] * p.volLow *
                            std::min(1.f, 1.f - p.panLow) * soloGain[0];
      const float lowOutR = wetLowR * mk[0] * p.volLow *
                            std::min(1.f, 1.f + p.panLow) * soloGain[0];
      const float midOutL = wetMidL * mk[1] * p.volMid *
                            std::min(1.f, 1.f - p.panMid) * soloGain[1];
      const float midOutR = wetMidR * mk[1] * p.volMid *
                            std::min(1.f, 1.f + p.panMid) * soloGain[1];
      const float highOutL = wetHighL * mk[2] * p.volHigh *
                             std::min(1.f, 1.f - p.panHigh) * soloGain[2];
      const float highOutR = wetHighR * mk[2] * p.volHigh *
                             std::min(1.f, 1.f + p.panHigh) * soloGain[2];

      wetL[i] = lowOutL + midOutL + highOutL;
      wetR[i] = lowOutR + midOutR + highOutR;

      // Detector G2: una vez por muestra base, en la ultima submuestra.
      if (i == N - 1) {
        float dIn[3];
        float dOut[3];
        dIn[0] = 0.5f * (std::fabs(lowLf) + std::fabs(lowRf));
        dIn[1] = 0.5f * (std::fabs(midLf) + std::fabs(midRf));
        dIn[2] = 0.5f * (std::fabs(highLf) + std::fabs(highRf));
        dOut[0] = 0.5f * (std::fabs(wetLowL) + std::fabs(wetLowR));
        dOut[1] = 0.5f * (std::fabs(wetMidL) + std::fabs(wetMidR));
        dOut[2] = 0.5f * (std::fabs(wetHighL) + std::fabs(wetHighR));
        autoMakeupDsp.update(dIn, dOut);
      }
    }

    outL = stage.downsampleL(wetL);
    outR = stage.downsampleR(wetR);
  }

  void process(float inL, float inR, float &outL, float &outR) {
    const float soloStep = 200.f / sampleRate;
    for (int b = 0; b < 3; ++b) {
      const float target = (p.soloBand < 0 || p.soloBand == b) ? 1.f : 0.f;
      if (soloGain[b] < target)
        soloGain[b] = std::min(target, soloGain[b] + soloStep);
      else
        soloGain[b] = std::max(target, soloGain[b] - soloStep);
    }
    const float dryTarget = (p.soloBand < 0) ? 1.f : 0.f;
    if (dryGain < dryTarget)
      dryGain = std::min(dryTarget, dryGain + soloStep);
    else
      dryGain = std::max(dryTarget, dryGain - soloStep);

    float wL = 0.f;
    float wR = 0.f;
    switch (factor) {
      case 1:
        processBands(stage1, inL, inR, wL, wR);
        break;
      case 2:
        processBands(stage2, inL, inR, wL, wR);
        break;
      case 8:
        processBands(stage8, inL, inR, wL, wR);
        break;
      default:
        processBands(stage4, inL, inR, wL, wR);
        break;
    }

    const float blockedL = wL - dcX1L + dcR * dcY1L;
    dcX1L = wL;
    dcY1L = blockedL;
    wL = blockedL;

    const float blockedR = wR - dcX1R + dcR * dcY1R;
    dcX1R = wR;
    dcY1R = blockedR;
    wR = blockedR;

    dryBufferL[dryIndex] = inL;
    dryBufferR[dryIndex] = inR;
    int readIndex = dryIndex - dryDelay;
    if (readIndex < 0) readIndex += LOCAL_OS_LATENCY + 1;
    const float dryL = dryBufferL[readIndex];
    const float dryR = dryBufferR[readIndex];
    dryIndex++;
    if (dryIndex > LOCAL_OS_LATENCY) dryIndex = 0;

    outL = (dryL * (1.f - p.mix) * dryGain + wL * p.mix) * p.outVol;
    outR = (dryR * (1.f - p.mix) * dryGain + wR * p.mix) * p.outVol;
  }
};

}  // namespace cdt
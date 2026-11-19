#pragma once

#include <dsp/filter.hpp>
#include <algorithm>
#include <cmath>

#include "AutoMakeup.hpp"
#include "Curves.hpp"
#include "Oversampling.hpp"

namespace coaldynamics {

struct OSParams {
  float driveLow, driveMid, driveHigh;
  float panLow, panMid, panHigh;
  float volLow, volMid, volHigh;
  int curveLow, curveMid, curveHigh;
  float makeupLow, makeupMid, makeupHigh;
  /** Ganancias combinadas por muestra base (J2): makeup * vol * pan * solo. */
  float gainLowL, gainLowR, gainMidL, gainMidR, gainHighL, gainHighR;
  float mix;
  float outVol;
};

struct SaturatorDsp {
  int oversampling = 1;
  int crossover = 0;
  int adaa = 0;
  float sampleRate = 44100.f;

  rack::dsp::BiquadFilter lowFilterL, lowFilterR;
  rack::dsp::BiquadFilter midFilterL, midFilterR;
  rack::dsp::BiquadFilter highFilterL, highFilterR;
  rack::dsp::BiquadFilter lowFilter2L, lowFilter2R;
  rack::dsp::BiquadFilter highFilter2L, highFilter2R;

  coaldynamics::OversampledStage<2, coaldynamics::OS_QUALITY> stage2;
  coaldynamics::OversampledStage<4, coaldynamics::OS_QUALITY> stage4;
  coaldynamics::OversampledStage<8, coaldynamics::OS_QUALITY> stage8;
  coaldynamics::BypassStage<coaldynamics::OS_QUALITY> stage1;

  float uPrev[3][2] = {};
  bool adaaResync[3] = {true, true, true};
  int adaaCurveApplied[3] = {-1, -1, -1};

  float dcR = 0.f;
  float dcX1L = 0.f, dcX1R = 0.f;
  float dcY1L = 0.f, dcY1R = 0.f;

  float dryBufferL[coaldynamics::OS_LATENCY + 1] = {};
  float dryBufferR[coaldynamics::OS_LATENCY + 1] = {};
  int dryIndex = 0;
  int dryDelay = 0;

  int soloBand = -1;
  float soloGain[3] = {1.f, 1.f, 1.f};
  float dryGain = 1.f;

  coaldynamics::AutoMakeup autoMakeupDsp;

  float satLowMeter = 0.f;
  float satMidMeter = 0.f;
  float satHighMeter = 0.f;

  void clearDryBuffer() {
    for (int i = 0; i <= coaldynamics::OS_LATENCY; i++) {
      dryBufferL[i] = 0.f;
      dryBufferR[i] = 0.f;
    }
    dryIndex = 0;
  }

  void setSampleRate(float fs) {
    sampleRate = fs;
    const float twoPi = 6.283185307179586f;
    dcR = std::exp(-twoPi * 15.f / fs);
    dcX1L = 0.f;
    dcX1R = 0.f;
    dcY1L = 0.f;
    dcY1R = 0.f;
    clearDryBuffer();
    autoMakeupDsp.setSampleRate(fs);
    updateFilterFrequencies();
  }

  void setOversampling(int factor) {
    if (factor != 1 && factor != 2 && factor != 4 && factor != 8)
      return;
    oversampling = factor;
    if (factor == 2)
      stage2.reset();
    else if (factor == 4)
      stage4.reset();
    else if (factor == 8)
      stage8.reset();
    dryDelay = (factor > 1) ? coaldynamics::OS_LATENCY : 0;
    clearDryBuffer();
    lowFilterL.reset();
    lowFilterR.reset();
    midFilterL.reset();
    midFilterR.reset();
    highFilterL.reset();
    highFilterR.reset();
    updateFilterFrequencies();
    resetAdaaState();
  }

  void setCrossover(int mode) {
    if (mode != 0 && mode != 1)
      return;
    crossover = mode;
    lowFilterL.reset();
    lowFilterR.reset();
    lowFilter2L.reset();
    lowFilter2R.reset();
    midFilterL.reset();
    midFilterR.reset();
    highFilterL.reset();
    highFilterR.reset();
    highFilter2L.reset();
    highFilter2R.reset();
    updateFilterFrequencies();
    resetAdaaState();
  }

  void setAdaa(int on) {
    adaa = (on != 0) ? 1 : 0;
    resetAdaaState();
  }

  void resetAdaaState() {
    for (int b = 0; b < 3; b++)
      uPrev[b][0] = uPrev[b][1] = 0.f;
    adaaResync[0] = adaaResync[1] = adaaResync[2] = true;
    adaaCurveApplied[0] = adaaCurveApplied[1] = adaaCurveApplied[2] = -1;
  }

  void updateFilterFrequencies() {
    const float fs = sampleRate * (float)oversampling;
    if (crossover == 1) {
      lowFilterL.setParameters(rack::dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f, 1.0f);
      lowFilterR.setParameters(rack::dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f, 1.0f);
      lowFilter2L.setParameters(rack::dsp::BiquadFilter::LOWPASS, 353.55339f / fs, 0.707f, 1.0f);
      lowFilter2R.setParameters(rack::dsp::BiquadFilter::LOWPASS, 353.55339f / fs, 0.707f, 1.0f);
      highFilterL.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f, 1.0f);
      highFilterR.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f, 1.0f);
      highFilter2L.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 5656.85425f / fs, 0.707f, 1.0f);
      highFilter2R.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 5656.85425f / fs, 0.707f, 1.0f);
      return;
    }
    lowFilterL.setParameters(rack::dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f, 1.0f);
    lowFilterR.setParameters(rack::dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f, 1.0f);
    midFilterL.setParameters(rack::dsp::BiquadFilter::BANDPASS, 1000.f / fs, 1.0f, 1.0f);
    midFilterR.setParameters(rack::dsp::BiquadFilter::BANDPASS, 1000.f / fs, 1.0f, 1.0f);
    highFilterL.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f, 1.0f);
    highFilterR.setParameters(rack::dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f, 1.0f);
  }

  void resetState() {
    setOversampling(oversampling);
    lowFilter2L.reset();
    lowFilter2R.reset();
    highFilter2L.reset();
    highFilter2R.reset();
    dcX1L = 0.f;
    dcX1R = 0.f;
    dcY1L = 0.f;
    dcY1R = 0.f;
    settleRamps();
    satLowMeter = 0.f;
    satMidMeter = 0.f;
    satHighMeter = 0.f;
    autoMakeupDsp.reset();
  }

  void settleRamps() {
    for (int b = 0; b < 3; b++)
      soloGain[b] = (soloBand < 0 || soloBand == b) ? 1.f : 0.f;
    dryGain = (soloBand < 0) ? 1.f : 0.f;
  }

  template <class Stage>
  void processBlock(Stage &stage, const OSParams &p, float inL, float inR, float &outL,
                    float &outR) {
    if (crossover == 1) {
      if (adaa == 1)
        processBands<Stage, true, true>(stage, p, inL, inR, outL, outR);
      else
        processBands<Stage, true, false>(stage, p, inL, inR, outL, outR);
    } else {
      if (adaa == 1)
        processBands<Stage, false, true>(stage, p, inL, inR, outL, outR);
      else
        processBands<Stage, false, false>(stage, p, inL, inR, outL, outR);
    }
  }

  template <class Stage, bool LR4, bool ADAA>
  void processBands(Stage &stage, const OSParams &p, float inL, float inR, float &outL,
                    float &outR) {
    const int N = Stage::FACTOR;
    float highL[N];
    float highR[N];
    float wetL[N];
    float wetR[N];

    stage.upsample(inL, inR, highL, highR);
    const int curve[3] = {p.curveLow, p.curveMid, p.curveHigh};

    for (int i = 0; i < N; i++) {
      float xL = highL[i];
      float xR = highR[i];

      float lowL, lowR, midL, midR, upperL, upperR;
      if (LR4) {
        lowL = lowFilter2L.process(lowFilterL.process(xL));
        lowR = lowFilter2R.process(lowFilterR.process(xR));
        upperL = highFilter2L.process(highFilterL.process(xL));
        upperR = highFilter2R.process(highFilterR.process(xR));
        midL = xL - lowL - upperL;
        midR = xR - lowR - upperR;
      } else {
        lowL = lowFilterL.process(xL);
        lowR = lowFilterR.process(xR);
        midL = midFilterL.process(xL);
        midR = midFilterR.process(xR);
        upperL = highFilterL.process(xL);
        upperR = highFilterR.process(xR);
      }

      float u[3][2];
      u[0][0] = coaldynamics::curveDrive(lowL, p.driveLow, curve[0]);
      u[0][1] = coaldynamics::curveDrive(lowR, p.driveLow, curve[0]);
      u[1][0] = coaldynamics::curveDrive(midL, p.driveMid, curve[1]);
      u[1][1] = coaldynamics::curveDrive(midR, p.driveMid, curve[1]);
      u[2][0] = coaldynamics::curveDrive(upperL, p.driveHigh, curve[2]);
      u[2][1] = coaldynamics::curveDrive(upperR, p.driveHigh, curve[2]);

      float wet[3][2];
      if (ADAA) {
        if (i == 0) {
          for (int b = 0; b < 3; b++) {
            if (adaaResync[b]) {
              uPrev[b][0] = u[b][0];
              uPrev[b][1] = u[b][1];
              adaaResync[b] = false;
            }
          }
        }
        for (int b = 0; b < 3; b++) {
          wet[b][0] = coaldynamics::applyCurveAdaa(u[b][0], uPrev[b][0], curve[b]);
          wet[b][1] = coaldynamics::applyCurveAdaa(u[b][1], uPrev[b][1], curve[b]);
        }
      } else {
        for (int b = 0; b < 3; b++) {
          wet[b][0] = coaldynamics::applyCurve(u[b][0], curve[b]);
          wet[b][1] = coaldynamics::applyCurve(u[b][1], curve[b]);
        }
      }

      float lowOutL = wet[0][0] * p.gainLowL;
      float lowOutR = wet[0][1] * p.gainLowR;
      float midOutL = wet[1][0] * p.gainMidL;
      float midOutR = wet[1][1] * p.gainMidR;
      float upperOutL = wet[2][0] * p.gainHighL;
      float upperOutR = wet[2][1] * p.gainHighR;

      wetL[i] = lowOutL + midOutL + upperOutL;
      wetR[i] = lowOutR + midOutR + upperOutR;

      if (i == N - 1) {
        satLowMeter = std::min(std::abs(wet[0][0] - u[0][0]), 1.f);
        satMidMeter = std::min(std::abs(wet[1][0] - u[1][0]), 1.f);
        satHighMeter = std::min(std::abs(wet[2][0] - u[2][0]), 1.f);

        float dIn[3];
        float dOut[3];
        dIn[0] = 0.5f * (std::abs(u[0][0]) + std::abs(u[0][1]));
        dIn[1] = 0.5f * (std::abs(u[1][0]) + std::abs(u[1][1]));
        dIn[2] = 0.5f * (std::abs(u[2][0]) + std::abs(u[2][1]));
        for (int b = 0; b < 3; b++)
          dOut[b] = 0.5f * (std::abs(wet[b][0]) + std::abs(wet[b][1])) / coaldynamics::TRIM[curve[b]];
        autoMakeupDsp.update(dIn, dOut);
      }
    }

    outL = stage.downsampleL(wetL);
    outR = stage.downsampleR(wetR);
  }

  void process(OSParams &p, float inL, float inR, float &outL, float &outR, float sampleTime) {
    if (p.curveLow != adaaCurveApplied[0]) {
      adaaCurveApplied[0] = p.curveLow;
      adaaResync[0] = true;
    }
    if (p.curveMid != adaaCurveApplied[1]) {
      adaaCurveApplied[1] = p.curveMid;
      adaaResync[1] = true;
    }
    if (p.curveHigh != adaaCurveApplied[2]) {
      adaaCurveApplied[2] = p.curveHigh;
      adaaResync[2] = true;
    }

    const float soloStep = sampleTime * 200.f;
    for (int b = 0; b < 3; b++) {
      const float target = (soloBand < 0 || soloBand == b) ? 1.f : 0.f;
      if (soloGain[b] < target)
        soloGain[b] = std::min(target, soloGain[b] + soloStep);
      else
        soloGain[b] = std::max(target, soloGain[b] - soloStep);
    }
    const float dryTarget = (soloBand < 0) ? 1.f : 0.f;
    if (dryGain < dryTarget)
      dryGain = std::min(dryTarget, dryGain + soloStep);
    else
      dryGain = std::max(dryTarget, dryGain - soloStep);

    // Ganancias combinadas (J2): una multiplicacion por muestra base en vez de
    // cinco por submuestra en processBands.
    p.gainLowL = p.makeupLow * p.volLow * std::min(1.f, 1.f - p.panLow) * soloGain[0];
    p.gainLowR = p.makeupLow * p.volLow * std::min(1.f, 1.f + p.panLow) * soloGain[0];
    p.gainMidL = p.makeupMid * p.volMid * std::min(1.f, 1.f - p.panMid) * soloGain[1];
    p.gainMidR = p.makeupMid * p.volMid * std::min(1.f, 1.f + p.panMid) * soloGain[1];
    p.gainHighL = p.makeupHigh * p.volHigh * std::min(1.f, 1.f - p.panHigh) * soloGain[2];
    p.gainHighR = p.makeupHigh * p.volHigh * std::min(1.f, 1.f + p.panHigh) * soloGain[2];

    float wetL = 0.f;
    float wetR = 0.f;
    switch (oversampling) {
      case 1:
        processBlock(stage1, p, inL, inR, wetL, wetR);
        break;
      case 2:
        processBlock(stage2, p, inL, inR, wetL, wetR);
        break;
      case 8:
        processBlock(stage8, p, inL, inR, wetL, wetR);
        break;
      default:
        processBlock(stage4, p, inL, inR, wetL, wetR);
        break;
    }

    float blockedL = wetL - dcX1L + dcR * dcY1L;
    dcX1L = wetL;
    dcY1L = blockedL;
    wetL = blockedL;
    float blockedR = wetR - dcX1R + dcR * dcY1R;
    dcX1R = wetR;
    dcY1R = blockedR;
    wetR = blockedR;

    dryBufferL[dryIndex] = inL;
    dryBufferR[dryIndex] = inR;
    int readIndex = dryIndex - dryDelay;
    if (readIndex < 0)
      readIndex += coaldynamics::OS_LATENCY + 1;
    float dryL = dryBufferL[readIndex];
    float dryR = dryBufferR[readIndex];
    dryIndex++;
    if (dryIndex > coaldynamics::OS_LATENCY)
      dryIndex = 0;

    outL = (dryL * (1.f - p.mix) * dryGain + wetL * p.mix) * p.outVol;
    outR = (dryR * (1.f - p.mix) * dryGain + wetR * p.mix) * p.outVol;
  }
};

}  // namespace coaldynamics

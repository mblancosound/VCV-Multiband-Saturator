#pragma once

#include <algorithm>
#include <cmath>

#include "../../src/SaturatorDsp.hpp"

namespace cdt {

struct ChainParams {
  float driveLow, driveMid, driveHigh;
  int curveLow, curveMid, curveHigh;
  float panLow, panMid, panHigh;
  float volLow, volMid, volHigh;
  int soloBand;
  float mix;
  float outVol;
  int lr4;
  int adaa;
  float makeupLow, makeupMid, makeupHigh;
  int autoMakeup;

  ChainParams()
      : driveLow(1.f), driveMid(1.f), driveHigh(1.f),
        curveLow(coaldynamics::TANH), curveMid(coaldynamics::TANH), curveHigh(coaldynamics::TANH),
        panLow(0.f), panMid(0.f), panHigh(0.f),
        volLow(1.f), volMid(1.f), volHigh(1.f),
        soloBand(-1), mix(1.f), outVol(1.f), lr4(0), adaa(0),
        makeupLow(1.f), makeupMid(1.f), makeupHigh(1.f), autoMakeup(0) {}
};

struct OfflineChain {
  coaldynamics::SaturatorDsp dsp;
  int adaaApplied = -1;

  ChainParams p;

  OfflineChain() { configure(48000.f, 1); }

  void configure(float fs, int factor) {
    adaaApplied = p.adaa;
    dsp.setAdaa(p.adaa);
    dsp.setCrossover(p.lr4 ? 1 : 0);
    dsp.setSampleRate(fs);
    dsp.setOversampling(factor);
    dsp.autoMakeupDsp.reset();
    settleRamps();
  }

  void settleRamps() {
    dsp.soloBand = p.soloBand;
    dsp.settleRamps();
  }

  void process(float inL, float inR, float &outL, float &outR) {
    dsp.soloBand = p.soloBand;
    if (p.adaa != adaaApplied) {
      dsp.setAdaa(p.adaa);
      adaaApplied = p.adaa;
    }
    coaldynamics::OSParams q;
    q.driveLow = p.driveLow;
    q.driveMid = p.driveMid;
    q.driveHigh = p.driveHigh;
    q.panLow = p.panLow;
    q.panMid = p.panMid;
    q.panHigh = p.panHigh;
    q.volLow = p.volLow;
    q.volMid = p.volMid;
    q.volHigh = p.volHigh;
    q.curveLow = p.curveLow;
    q.curveMid = p.curveMid;
    q.curveHigh = p.curveHigh;
    q.mix = p.mix;
    q.outVol = p.outVol;
    float mkLow = p.autoMakeup ? dsp.autoMakeupDsp.gain(0) : p.makeupLow;
    float mkMid = p.autoMakeup ? dsp.autoMakeupDsp.gain(1) : p.makeupMid;
    float mkHigh = p.autoMakeup ? dsp.autoMakeupDsp.gain(2) : p.makeupHigh;
    q.makeupLow = mkLow;
    q.makeupMid = mkMid;
    q.makeupHigh = mkHigh;
    dsp.process(q, inL, inR, outL, outR, 1.f / dsp.sampleRate);
  }
};

}  // namespace cdt

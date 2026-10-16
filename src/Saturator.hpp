#pragma once

#include <rack.hpp>

using namespace rack;

struct Saturator;

struct SaturatorWidget : ModuleWidget {
  SaturatorWidget(Saturator *module);
};

struct Saturator : Module {
  enum ParamIds {
    LOW_DRIVE,
    MID_DRIVE,
    HIGH_DRIVE,
    LOW_PAN,
    LOW_VOL,
    MID_PAN,
    MID_VOL,
    HIGH_PAN,
    HIGH_VOL,
    OUTPUT_VOL,
    MIX_PARAM,
    LOW_PAN_CV,
    LOW_VOL_CV,
    MID_PAN_CV,
    MID_VOL_CV,
    HIGH_PAN_CV,
    HIGH_VOL_CV,
    NUM_PARAMS
  };
  enum InputIds {
    IN_L,
    IN_R,
    LOW_PAN_INPUT,
    LOW_VOL_INPUT,
    MID_PAN_INPUT,
    MID_VOL_INPUT,
    HIGH_PAN_INPUT,
    HIGH_VOL_INPUT,
    NUM_INPUTS
  };
  enum OutputIds { OUT_L, OUT_R, NUM_OUTPUTS };
  enum LightIds { ENUMS(SATURE_LIGHT, 6), NUM_LIGHTS };

  dsp::ClockDivider lightDivider;

  dsp::BiquadFilter lowFilterL, lowFilterR;
  dsp::BiquadFilter midFilterL, midFilterR;
  dsp::BiquadFilter highFilterL, highFilterR;

  Saturator();

  void process(const ProcessArgs &args) override;
};
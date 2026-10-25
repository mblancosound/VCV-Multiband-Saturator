#pragma once

#include <rack.hpp>

#include "Oversampling.hpp"

using namespace rack;

/** Format for the PAN knobs: bipolar balance on a 0-50 scale, "C" at center.
Overrides only the display string; the raw value in [-1, 1] and its range are unchanged.
*/
struct PanParamQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    float v = getValue();
    if (v < -1.f)
      v = -1.f;
    if (v > 1.f)
      v = 1.f;
    int n = (int) std::lround((v < 0.f ? -v : v) * 50.f);
    if (n == 0)
      return "C";
    return string::f("%d%s", n, v < 0.f ? "L" : "R");
  }
};

struct Saturator;

struct SaturatorWidget : ModuleWidget {
  SaturatorWidget(Saturator *module);
  void appendContextMenu(ui::Menu *menu) override;
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

  /** Parameters already resolved (knob + CV) for one base-rate sample. */
  struct OSParams {
    float driveLow, driveMid, driveHigh;
    float panLow, panMid, panHigh;
    float volLow, volMid, volHigh;
  };

  dsp::ClockDivider lightDivider;

  dsp::BiquadFilter lowFilterL, lowFilterR;
  dsp::BiquadFilter midFilterL, midFilterR;
  dsp::BiquadFilter highFilterL, highFilterR;

  coaldynamics::OversampledStage<2, coaldynamics::OS_QUALITY> stage2;
  coaldynamics::OversampledStage<4, coaldynamics::OS_QUALITY> stage4;
  coaldynamics::OversampledStage<8, coaldynamics::OS_QUALITY> stage8;
  coaldynamics::BypassStage<coaldynamics::OS_QUALITY> stage1;

  /** 1, 2, 4 u 8. Sólo lo escribe applyOversampling() en el hilo de audio. */
  int oversampling = 4;
  /** Pedido pendiente de aplicar (0 = ninguno). Lo escribe la UI o el JSON. */
  int pendingOversampling = 0;
  float sampleRate = 44100.f;

  /** Delay de la señal dry para compensar la latencia del sobremuestreo. */
  float dryBufferL[coaldynamics::OS_LATENCY + 1] = {};
  float dryBufferR[coaldynamics::OS_LATENCY + 1] = {};
  int dryIndex = 0;
  int dryDelay = coaldynamics::OS_LATENCY;

  /** Saturation meters for the lights, taken from the last oversampled sub-sample. */
  float satLowMeter = 0.f;
  float satMidMeter = 0.f;
  float satHighMeter = 0.f;

  Saturator();

  void process(const ProcessArgs &args) override;
  void onSampleRateChange(const SampleRateChangeEvent &e) override;
  json_t *dataToJson() override;
  void dataFromJson(json_t *rootJ) override;

  /** Called from UI/JSON threads: only records the request. */
  void setOversampling(int factor);
  /** Called from the audio thread: resets state and rebuilds coefficients. */
  void applyOversampling(int factor);
  void updateFilterFrequencies();

  template <class Stage>
  void processBands(Stage &stage, const OSParams &p, float inL, float inR,
                    float &outL, float &outR);
};
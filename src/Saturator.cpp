#include "Saturator.hpp"
#include "plugin.hpp"

Plugin *pluginInstance;
Model *modelSaturator;

Saturator::Saturator() {
  config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

  configParam(LOW_DRIVE, 0.f, 1.f, 0.f, "Low Drive", "%", 0.f, 100.f);
  configParam(LOW_PAN, -1.f, 1.f, 0.f, "Low Pan", "", -1.f, 1.f);
  configParam(LOW_VOL, 0.f, 1.f, 0.8f, "Low Vol", "%", 0.f, 100.f);

  configParam(MID_DRIVE, 0.f, 1.f, 0.f, "Mid Drive", "%", 0.f, 100.f);
  configParam(MID_PAN, -1.f, 1.f, 0.f, "Mid Pan", "", -1.f, 1.f);
  configParam(MID_VOL, 0.f, 1.f, 0.8f, "Mid Vol", "%", 0.f, 100.f);

  configParam(HIGH_DRIVE, 0.f, 1.f, 0.f, "High Drive", "%", 0.f, 100.f);
  configParam(HIGH_PAN, -1.f, 1.f, 0.f, "High Pan", "", -1.f, 1.f);
  configParam(HIGH_VOL, 0.f, 1.f, 0.8f, "High Vol", "%", 0.f, 100.f);

  configParam(OUTPUT_VOL, 0.f, 1.f, 1.f, "Output", "%", 0.f, 100.f);
  configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix", "%", 0.f, 100.f);

  configParam(LOW_PAN_CV, -1.f, 1.f, 0.f, "Low Pan CV", "%", 0.f, 100.f);
  configParam(LOW_VOL_CV, -1.f, 1.f, 0.f, "Low Vol CV", "%", 0.f, 100.f);
  configParam(MID_PAN_CV, -1.f, 1.f, 0.f, "Mid Pan CV", "%", 0.f, 100.f);
  configParam(MID_VOL_CV, -1.f, 1.f, 0.f, "Mid Vol CV", "%", 0.f, 100.f);
  configParam(HIGH_PAN_CV, -1.f, 1.f, 0.f, "High Pan CV", "%", 0.f, 100.f);
  configParam(HIGH_VOL_CV, -1.f, 1.f, 0.f, "High Vol CV", "%", 0.f, 100.f);

  getParamQuantity(LOW_DRIVE)->randomizeEnabled = false;
  getParamQuantity(LOW_PAN)->randomizeEnabled = false;
  getParamQuantity(LOW_VOL)->randomizeEnabled = false;
  getParamQuantity(MID_DRIVE)->randomizeEnabled = false;
  getParamQuantity(MID_PAN)->randomizeEnabled = false;
  getParamQuantity(MID_VOL)->randomizeEnabled = false;
  getParamQuantity(HIGH_DRIVE)->randomizeEnabled = false;
  getParamQuantity(HIGH_PAN)->randomizeEnabled = false;
  getParamQuantity(HIGH_VOL)->randomizeEnabled = false;
  getParamQuantity(OUTPUT_VOL)->randomizeEnabled = false;
  getParamQuantity(MIX_PARAM)->randomizeEnabled = false;
  getParamQuantity(LOW_PAN_CV)->randomizeEnabled = false;
  getParamQuantity(LOW_VOL_CV)->randomizeEnabled = false;
  getParamQuantity(MID_PAN_CV)->randomizeEnabled = false;
  getParamQuantity(MID_VOL_CV)->randomizeEnabled = false;
  getParamQuantity(HIGH_PAN_CV)->randomizeEnabled = false;
  getParamQuantity(HIGH_VOL_CV)->randomizeEnabled = false;

  configInput(IN_L, "Left input");
  configInput(IN_R, "Right input");
  configInput(LOW_PAN_INPUT, "Low Pan CV");
  configInput(LOW_VOL_INPUT, "Low Vol CV");
  configInput(MID_PAN_INPUT, "Mid Pan CV");
  configInput(MID_VOL_INPUT, "Mid Vol CV");
  configInput(HIGH_PAN_INPUT, "High Pan CV");
  configInput(HIGH_VOL_INPUT, "High Vol CV");

  configOutput(OUT_L, "Left output");
  configOutput(OUT_R, "Right output");

  lightDivider.setDivision(32);

  float sr = APP->engine->getSampleRate();

  // Lowpass
  lowFilterL.setParameters(dsp::BiquadFilter::LOWPASS, 250.f / sr, 0.707f,
                           1.0f);
  lowFilterR.setParameters(dsp::BiquadFilter::LOWPASS, 250.f / sr, 0.707f,
                           1.0f);

  // Bandpass
  midFilterL.setParameters(dsp::BiquadFilter::BANDPASS, 1000.f / sr, 1.0f,
                           1.0f);
  midFilterR.setParameters(dsp::BiquadFilter::BANDPASS, 1000.f / sr, 1.0f,
                           1.0f);

  // Highpass
  highFilterL.setParameters(dsp::BiquadFilter::HIGHPASS, 4000.f / sr, 0.707f,
                            1.0f);
  highFilterR.setParameters(dsp::BiquadFilter::HIGHPASS, 4000.f / sr, 0.707f,
                            1.0f);
}

void Saturator::process(const ProcessArgs &args) {
  float inL = inputs[IN_L].getVoltage();
  float inR = inputs[IN_R].isConnected() ? inputs[IN_R].getVoltage() : inL;

  float lowL = lowFilterL.process(inL);
  float lowR = lowFilterR.process(inR);
  float midL = midFilterL.process(inL);
  float midR = midFilterR.process(inR);
  float highL = highFilterL.process(inL);
  float highR = highFilterR.process(inR);

  float driveLow = 1.f + params[LOW_DRIVE].getValue() * 10.f;
  float driveMid = 1.f + params[MID_DRIVE].getValue() * 10.f;
  float driveHigh = 1.f + params[HIGH_DRIVE].getValue() * 10.f;

  float wetLowL = std::tanh(lowL * driveLow);
  float wetLowR = std::tanh(lowR * driveLow);
  float wetMidL = std::tanh(midL * driveMid);
  float wetMidR = std::tanh(midR * driveMid);
  float wetHighL = std::tanh(highL * driveHigh);
  float wetHighR = std::tanh(highR * driveHigh);

  float panLow = clamp(params[LOW_PAN].getValue() +
                           (inputs[LOW_PAN_INPUT].getVoltage() / 5.f) *
                               params[LOW_PAN_CV].getValue(),
                       -1.f, 1.f);
  float panMid = clamp(params[MID_PAN].getValue() +
                           (inputs[MID_PAN_INPUT].getVoltage() / 5.f) *
                               params[MID_PAN_CV].getValue(),
                       -1.f, 1.f);
  float panHigh = clamp(params[HIGH_PAN].getValue() +
                            (inputs[HIGH_PAN_INPUT].getVoltage() / 5.f) *
                                params[HIGH_PAN_CV].getValue(),
                        -1.f, 1.f);

  float volLow = clamp(params[LOW_VOL].getValue() +
                           (inputs[LOW_VOL_INPUT].getVoltage() / 10.f) *
                               params[LOW_VOL_CV].getValue(),
                       0.f, 1.f);
  float volMid = clamp(params[MID_VOL].getValue() +
                           (inputs[MID_VOL_INPUT].getVoltage() / 10.f) *
                               params[MID_VOL_CV].getValue(),
                       0.f, 1.f);
  float volHigh = clamp(params[HIGH_VOL].getValue() +
                            (inputs[HIGH_VOL_INPUT].getVoltage() / 10.f) *
                                params[HIGH_VOL_CV].getValue(),
                        0.f, 1.f);

  float lowOutL = wetLowL * volLow * (1.f - panLow * 0.5f);
  float lowOutR = wetLowR * volLow * (1.f + panLow * 0.5f);

  float midOutL = wetMidL * volMid * (1.f - panMid * 0.5f);
  float midOutR = wetMidR * volMid * (1.f + panMid * 0.5f);

  float highOutL = wetHighL * volHigh * (1.f - panHigh * 0.5f);
  float highOutR = wetHighR * volHigh * (1.f + panHigh * 0.5f);

  float wetL = lowOutL + midOutL + highOutL;
  float wetR = lowOutR + midOutR + highOutR;

  float mix = params[MIX_PARAM].getValue();
  float outL = inL * (1.f - mix) + wetL * mix;
  float outR = inR * (1.f - mix) + wetR * mix;

  float outVol = params[OUTPUT_VOL].getValue();
  outputs[OUT_L].setVoltage(outL * outVol);
  outputs[OUT_R].setVoltage(outR * outVol);

  if (lightDivider.process()) {
    float satLow = std::abs(wetLowL - lowL * driveLow);
    float satMid = std::abs(wetMidL - midL * driveMid);
    float satHigh = std::abs(wetHighL - highL * driveHigh);
    lights[SATURE_LIGHT + 0].setSmoothBrightness(
        std::min(satLow, 1.f), args.sampleTime * lightDivider.getDivision());
    lights[SATURE_LIGHT + 1].setSmoothBrightness(
        std::min(satLow, 1.f), args.sampleTime * lightDivider.getDivision());
    lights[SATURE_LIGHT + 2].setSmoothBrightness(
        std::min(satMid, 1.f), args.sampleTime * lightDivider.getDivision());
    lights[SATURE_LIGHT + 3].setSmoothBrightness(
        std::min(satMid, 1.f), args.sampleTime * lightDivider.getDivision());
    lights[SATURE_LIGHT + 4].setSmoothBrightness(
        std::min(satHigh, 1.f), args.sampleTime * lightDivider.getDivision());
    lights[SATURE_LIGHT + 5].setSmoothBrightness(
        std::min(satHigh, 1.f), args.sampleTime * lightDivider.getDivision());
  }
}

SaturatorWidget::SaturatorWidget(Saturator *module) {
  setModule(module);

  box.size = Vec(270, 380);

  setPanel(createPanel(asset::plugin(pluginInstance, "res/Saturator.svg")));

  // Tornillos
  addChild(createWidget<TornilloNegro>(Vec(RACK_GRID_WIDTH, 0)));
  addChild(
      createWidget<TornilloNegro>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
  addChild(createWidget<TornilloNegro>(
      Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
  addChild(createWidget<TornilloNegro>(Vec(
      box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

  // Drives (Knobs de 48px)
  addParam(
      createParamCentered<Knob48>(Vec(67.5, 84), module, Saturator::LOW_DRIVE));
  addParam(
      createParamCentered<Knob48>(Vec(135, 84), module, Saturator::MID_DRIVE));
  addParam(createParamCentered<Knob48>(Vec(202.5, 84), module,
                                       Saturator::HIGH_DRIVE));

  // Pans (Knobs de 20px)
  addParam(createParamCentered<Knob20>(Vec(52.5, 127.4), module,
                                       Saturator::LOW_PAN));
  addParam(
      createParamCentered<Knob20>(Vec(120, 127.4), module, Saturator::MID_PAN));
  addParam(createParamCentered<Knob20>(Vec(187.5, 127.4), module,
                                       Saturator::HIGH_PAN));

  // Vols (Knobs de 20px)
  addParam(createParamCentered<Knob20>(Vec(82.5, 147.4), module,
                                       Saturator::LOW_VOL));
  addParam(
      createParamCentered<Knob20>(Vec(150, 147.4), module, Saturator::MID_VOL));
  addParam(createParamCentered<Knob20>(Vec(217.5, 147.4), module,
                                       Saturator::HIGH_VOL));

  // Output & Mix (Knobs de 48px)
  addParam(createParamCentered<Knob48>(Vec(101.3, 201.3), module,
                                       Saturator::OUTPUT_VOL));
  addParam(createParamCentered<Knob48>(Vec(168.8, 201.3), module,
                                       Saturator::MIX_PARAM));

  // CV Pans (Atenuadores CV - Knobs de 20px)
  addParam(createParamCentered<Knob20>(Vec(52.6, 299.6), module,
                                       Saturator::LOW_PAN_CV));
  addParam(createParamCentered<Knob20>(Vec(120.1, 299.6), module,
                                       Saturator::MID_PAN_CV));
  addParam(createParamCentered<Knob20>(Vec(187.6, 299.6), module,
                                       Saturator::HIGH_PAN_CV));

  // CV Vols (Atenuadores CV - Knobs de 20px)
  addParam(createParamCentered<Knob20>(Vec(82.4, 299.6), module,
                                       Saturator::LOW_VOL_CV));
  addParam(createParamCentered<Knob20>(Vec(149.9, 299.6), module,
                                       Saturator::MID_VOL_CV));
  addParam(createParamCentered<Knob20>(Vec(217.4, 299.6), module,
                                       Saturator::HIGH_VOL_CV));

  // Entradas de Audio
  addInput(createInputCentered<CustomInputPort>(Vec(47.3, 352.6), module,
                                                Saturator::IN_L));
  addInput(createInputCentered<CustomInputPort>(Vec(87.7, 352.6), module,
                                                Saturator::IN_R));

  // Salidas de Audio
  addOutput(createOutputCentered<CustomOutputPort>(Vec(182.3, 352.6), module,
                                                   Saturator::OUT_L));
  addOutput(createOutputCentered<CustomOutputPort>(Vec(222.8, 352.6), module,
                                                   Saturator::OUT_R));

  // Entradas CV
  addInput(createInputCentered<CustomInputPort>(Vec(52.6, 273), module,
                                                Saturator::LOW_VOL_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(82.4, 273), module,
                                                Saturator::MID_VOL_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(119.7, 273), module,
                                                Saturator::HIGH_VOL_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(149.5, 273), module,
                                                Saturator::LOW_PAN_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(187.6, 273), module,
                                                Saturator::MID_PAN_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(217.4, 273), module,
                                                Saturator::HIGH_PAN_INPUT));
}
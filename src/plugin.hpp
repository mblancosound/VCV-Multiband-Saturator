#pragma once

#include <rack.hpp>

using namespace rack;

#include "Saturator.hpp"

extern Plugin *pluginInstance;
extern Model *modelSaturator;

struct Knob48 : RoundKnob {
  Knob48() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Knob_48px.svg")));
    minAngle = -0.7 * M_PI;
    maxAngle = 0.7 * M_PI;
  }
};

struct Knob20 : RoundKnob {
  Knob20() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Knob_20px.svg")));
    minAngle = -0.5 * M_PI;
    maxAngle = 0.5 * M_PI;
  }
};

struct CustomInputPort : SvgPort {
  CustomInputPort() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/PJ301M_input.svg")));
  }
};

struct CustomOutputPort : SvgPort {
  CustomOutputPort() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/PJ301M_output.svg")));
  }
};

struct TornilloNegro : SvgPort {
  TornilloNegro() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/tornillo_negro.svg")));
  }
};
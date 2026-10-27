#include "Saturator.hpp"

#include "plugin.hpp"

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
                                                Saturator::LOW_PAN_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(82.4, 273), module,
                                                Saturator::LOW_VOL_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(119.7, 273), module,
                                                Saturator::MID_PAN_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(149.5, 273), module,
                                                Saturator::MID_VOL_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(187.6, 273), module,
                                                Saturator::HIGH_PAN_INPUT));
  addInput(createInputCentered<CustomInputPort>(Vec(217.4, 273), module,
                                                Saturator::HIGH_VOL_INPUT));
}

void SaturatorWidget::appendContextMenu(ui::Menu *menu) {
  ModuleWidget::appendContextMenu(menu);

  Saturator *saturator = dynamic_cast<Saturator *>(module);
  if (!saturator)
    return;

  menu->addChild(new ui::MenuSeparator);
  menu->addChild(createIndexSubmenuItem(
      "Oversampling", {"Off (1x)", "2x", "4x", "8x"},
      [=]() {
        int amount = saturator->oversampling;
        if (amount == 1)
          return (size_t) 0;
        if (amount == 2)
          return (size_t) 1;
        if (amount == 8)
          return (size_t) 3;
        return (size_t) 2;
      },
      [=](size_t index) {
        if (index == 0)
          saturator->setOversampling(1);
        else if (index == 1)
          saturator->setOversampling(2);
        else if (index == 2)
          saturator->setOversampling(4);
        else
          saturator->setOversampling(8);
      }));

  menu->addChild(new ui::MenuSeparator);
  menu->addChild(createIndexSubmenuItem(
      "Low Curve", coaldynamics::curveNames(),
      [=]() { return (size_t) saturator->curves[0].load(); },
      [=](size_t index) { saturator->setCurve(0, (int) index); }));
  menu->addChild(createIndexSubmenuItem(
      "Mid Curve", coaldynamics::curveNames(),
      [=]() { return (size_t) saturator->curves[1].load(); },
      [=](size_t index) { saturator->setCurve(1, (int) index); }));
  menu->addChild(createIndexSubmenuItem(
      "High Curve", coaldynamics::curveNames(),
      [=]() { return (size_t) saturator->curves[2].load(); },
      [=](size_t index) { saturator->setCurve(2, (int) index); }));

  // Solo: -1 = Off, 0 = Low, 1 = Mid, 2 = High (mapeo explicito index<->banda).
  // El solo respeta MIX: silencia el dry y las otras dos bandas con rampa de
  // ~5 ms; la banda elegida pasa por el knob MIX actual.
  menu->addChild(new ui::MenuSeparator);
  menu->addChild(createIndexSubmenuItem(
      "Solo", {"Off", "Low", "Mid", "High"},
      [=]() {
        int band = saturator->solo.load();
        switch (band) {
          case 0:
            return (size_t) 1;
          case 1:
            return (size_t) 2;
          case 2:
            return (size_t) 3;
          default:
            return (size_t) 0;
        }
      },
      [=](size_t index) { saturator->setSolo((int) index - 1); }));

  // Copia la curva actual de una banda a las tres. Se guarda con la clave
  // "curves" existente (sin JSON nuevo). Sin crossfade: el click de cambio de
  // curva esta aceptado, igual que el selector manual (v2.2.1).
  menu->addChild(createSubmenuItem(
      "Copy Curve", "", [=](ui::Menu *menu) {
        static const char *bandNames[3] = {"Low", "Mid", "High"};
        for (int src = 0; src < 3; src++) {
          menu->addChild(createMenuItem(
              string::f("%s to all", bandNames[src]), "", [=]() {
                int id = saturator->curves[src].load();
                for (int b = 0; b < 3; b++)
                  saturator->setCurve(b, id);
              }));
        }
      }));

  // Info de solo lectura: sample rate, factor de oversampling, latencia en
  // muestras base y version del plugin (plugin.json). 0 samples a 1x,
  // OS_LATENCY (15) a 2x/4x/8x.
  menu->addChild(new ui::MenuSeparator);
  menu->addChild(createMenuLabel(string::f(
      "%.1f kHz | %dx | %d samples | v%s",
      saturator->sampleRate * 0.001f, saturator->oversampling,
      saturator->oversampling > 1 ? coaldynamics::OS_LATENCY : 0,
      pluginInstance->version.c_str())));
}

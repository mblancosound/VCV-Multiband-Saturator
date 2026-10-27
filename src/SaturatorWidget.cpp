#include "Saturator.hpp"

#include "plugin.hpp"

// ============================================================================
// Undo (Ctrl+Z) de las acciones del menu contextual (v2.4.0).
//
// Solo SaturatorWidget.cpp: cada menu guarda el valor ANTES de aplicar el
// cambio y lo registra en el historial de Rack (APP->history->push). Las
// undo/redo resuelven el modulo por moduleId en cada llamada, porque el
// puntero capturado en el menu puede quedar colgando si el modulo se borra
// (regla de history.hpp: "Pointers to Modules... are not allowed in Actions").
//
// Confirmado en Rack 2.6.6 (src/history.cpp): State::push() NO ejecuta la
// accion, solo la registra; el patron es aplicar el cambio y despues push().
// Ctrl+Z / Ctrl+Shift+Z se resuelven en Scene::onKey -> State::undo().
// El historial no se serializa con el patch (State no tiene dataToJson), por
// lo que dataFromJson() nunca registra entradas: cargar un patch no es una
// accion de usuario.
//
// Regla: si el valor elegido ya esta activo, no hay escritura ni entrada de
// historial (no-op silencioso).
// ============================================================================

namespace {

/** Resuelve el modulo por su id estable. Si ya no existe (borrado / undo de
    "add module"), devuelve NULL y los undo/redo son no-op silenciosos, igual
    que el patron de ParamChange::undo() en history.cpp. */
Saturator *saturatorFromModuleId(int64_t moduleId) {
  return dynamic_cast<Saturator *>(APP->engine->getModule(moduleId));
}

/** Solicita un factor de oversampling desde una undo/redo. Reutiliza
    setOversampling() (que solo deja pendingOversampling pendiente); si el
    factor pedido ya es el aplicado, cancela una peticion aun no aplicada (el
    campo es publico y atomico: Saturator.hpp lo documenta como escrito por la
    UI y consumido por el hilo de audio). */
void requestOversampling(Saturator *saturator, int factor) {
  if (factor == saturator->oversampling) {
    saturator->pendingOversampling.store(0, std::memory_order_relaxed);
    return;
  }
  saturator->setOversampling(factor);
}

// --- Acciones de historial (una clase por operacion del menu) ---------------

/** Cambio de curva de una banda (Low/Mid/High). */
struct CurveAction : history::ModuleAction {
  int band = 0;
  int oldCurve = 0;
  int newCurve = 0;

  CurveAction() { name = "change curve"; }

  void undo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    s->setCurve(band, oldCurve);
  }

  void redo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    s->setCurve(band, newCurve);
  }
};

/** Copy Curve: las tres bandas en UNA sola entrada de historial. Guarda los
    tres identificadores originales; con una clase propia se evita depender de
    ComplexAction y de su orden de undo. */
struct CopyCurveAction : history::ModuleAction {
  int oldCurves[3] = {};
  int newCurve = 0;

  CopyCurveAction() { name = "copy curve"; }

  void undo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    for (int b = 0; b < 3; b++)
      s->setCurve(b, oldCurves[b]);
  }

  void redo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    for (int b = 0; b < 3; b++)
      s->setCurve(b, newCurve);
  }
};

/** Cambio de solo (Off: -1, o 0..2). */
struct SoloAction : history::ModuleAction {
  int oldSolo = -1;
  int newSolo = -1;

  SoloAction() { name = "solo band"; }

  void undo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    s->setSolo(oldSolo);
  }

  void redo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    s->setSolo(newSolo);
  }
};

/** Cambio de oversampling (1/2/4/8). */
struct OversamplingAction : history::ModuleAction {
  int oldFactor = 1;
  int newFactor = 1;

  OversamplingAction() { name = "change oversampling"; }

  void undo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    requestOversampling(s, oldFactor);
  }

  void redo() override {
    Saturator *s = saturatorFromModuleId(moduleId);
    if (!s)
      return;
    requestOversampling(s, newFactor);
  }
};

// --- Helpers de los menus: aplicar + registrar (o no-op si no cambia) -------

void pushCurveHistory(Saturator *saturator, int band, int newCurve) {
  int oldCurve = saturator->curves[band].load();
  if (oldCurve == newCurve)
    return;
  saturator->setCurve(band, newCurve);

  CurveAction *action = new CurveAction;
  action->moduleId = saturator->id;
  action->band = band;
  action->oldCurve = oldCurve;
  action->newCurve = newCurve;
  APP->history->push(action);
}

void pushCopyCurveHistory(Saturator *saturator, int src) {
  int newCurve = saturator->curves[src].load();
  int oldCurves[3] = {saturator->curves[0].load(), saturator->curves[1].load(),
                      saturator->curves[2].load()};
  bool changed = false;
  for (int b = 0; b < 3; b++) {
    if (oldCurves[b] != newCurve)
      changed = true;
  }
  if (!changed)
    return;
  for (int b = 0; b < 3; b++)
    saturator->setCurve(b, newCurve);

  CopyCurveAction *action = new CopyCurveAction;
  action->moduleId = saturator->id;
  for (int b = 0; b < 3; b++)
    action->oldCurves[b] = oldCurves[b];
  action->newCurve = newCurve;
  APP->history->push(action);
}

void pushSoloHistory(Saturator *saturator, int band) {
  int oldSolo = saturator->solo.load();
  if (oldSolo == band)
    return;
  saturator->setSolo(band);

  SoloAction *action = new SoloAction;
  action->moduleId = saturator->id;
  action->oldSolo = oldSolo;
  action->newSolo = band;
  APP->history->push(action);
}

void pushOversamplingHistory(Saturator *saturator, int factor) {
  // Valor efectivo: la peticion pendiente cuenta como actual hasta que el
  // hilo de audio la aplica al principio del siguiente bloque. Se lee UNA vez
  // (atomico) para no comparar dos lecturas distintas si el audio la consume
  // entre medias.
  int pending = saturator->pendingOversampling.load(std::memory_order_relaxed);
  int oldFactor = (pending != 0) ? pending : saturator->oversampling;
  if (oldFactor == factor)
    return;
  requestOversampling(saturator, factor);

  OversamplingAction *action = new OversamplingAction;
  action->moduleId = saturator->id;
  action->oldFactor = oldFactor;
  action->newFactor = factor;
  APP->history->push(action);
}

} // namespace

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
        static const int factors[4] = {1, 2, 4, 8};
        pushOversamplingHistory(saturator, factors[index]);
      }));

  menu->addChild(new ui::MenuSeparator);
  menu->addChild(createIndexSubmenuItem(
      "Low Curve", coaldynamics::curveNames(),
      [=]() { return (size_t) saturator->curves[0].load(); },
      [=](size_t index) { pushCurveHistory(saturator, 0, (int) index); }));
  menu->addChild(createIndexSubmenuItem(
      "Mid Curve", coaldynamics::curveNames(),
      [=]() { return (size_t) saturator->curves[1].load(); },
      [=](size_t index) { pushCurveHistory(saturator, 1, (int) index); }));
  menu->addChild(createIndexSubmenuItem(
      "High Curve", coaldynamics::curveNames(),
      [=]() { return (size_t) saturator->curves[2].load(); },
      [=](size_t index) { pushCurveHistory(saturator, 2, (int) index); }));

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
      [=](size_t index) { pushSoloHistory(saturator, (int) index - 1); }));

  // Copia la curva actual de una banda a las tres. Se guarda con la clave
  // "curves" existente (sin JSON nuevo). Sin crossfade: el click de cambio de
  // curva esta aceptado, igual que el selector manual (v2.2.1). Undo/redo de
  // las tres bandas como UNA sola entrada de historial (v2.4.0).
  menu->addChild(createSubmenuItem(
      "Copy Curve", "", [=](ui::Menu *menu) {
        static const char *bandNames[3] = {"Low", "Mid", "High"};
        for (int src = 0; src < 3; src++) {
          menu->addChild(createMenuItem(
              string::f("%s to all", bandNames[src]), "",
              [=]() { pushCopyCurveHistory(saturator, src); }));
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

#include "plugin.hpp"
#include "Saturator.hpp"

extern Plugin *pluginInstance;
extern Model *modelSaturator;

void init(Plugin *p) {
  pluginInstance = p;

  modelSaturator =
      createModel<Saturator, SaturatorWidget>("Multiband_Saturator");
  p->addModel(modelSaturator);
}
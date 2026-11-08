#include "plugin.hpp"
#include "Saturator.hpp"

extern Plugin *pluginInstance;
extern Model *modelSaturator;

void init(Plugin *p) {
  pluginInstance = p;

  modelSaturator =
      createModel<Saturator, SaturatorWidget>("Anthracite");
  p->addModel(modelSaturator);
}
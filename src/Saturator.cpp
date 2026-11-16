#include "Saturator.hpp"

#include "plugin.hpp"

Plugin *pluginInstance;
Model *modelSaturator;

Saturator::Saturator() {
  config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

  configParam(LOW_DRIVE, 0.f, 1.f, 0.f, "Low Drive", "%", 0.f, 100.f);
  configParam<PanParamQuantity>(LOW_PAN, -1.f, 1.f, 0.f, "Low Pan", "");
  configParam(LOW_VOL, 0.f, 1.f, 1.f, "Low Vol", "%", 0.f, 100.f);

  configParam(MID_DRIVE, 0.f, 1.f, 0.f, "Mid Drive", "%", 0.f, 100.f);
  configParam<PanParamQuantity>(MID_PAN, -1.f, 1.f, 0.f, "Mid Pan", "");
  configParam(MID_VOL, 0.f, 1.f, 1.f, "Mid Vol", "%", 0.f, 100.f);

  configParam(HIGH_DRIVE, 0.f, 1.f, 0.f, "High Drive", "%", 0.f, 100.f);
  configParam<PanParamQuantity>(HIGH_PAN, -1.f, 1.f, 0.f, "High Pan", "");
  configParam(HIGH_VOL, 0.f, 1.f, 1.f, "High Vol", "%", 0.f, 100.f);

  configParam(OUTPUT_VOL, 0.f, 1.f, 1.f, "Output", "%", 0.f, 100.f);
  configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix", "%", 0.f, 100.f);

  configParam(LOW_PAN_CV, -1.f, 1.f, 0.f, "Low Pan CV", "%", 0.f, 100.f);
  configParam(LOW_VOL_CV, -1.f, 1.f, 0.f, "Low Vol CV", "%", 0.f, 100.f);
  configParam(MID_PAN_CV, -1.f, 1.f, 0.f, "Mid Pan CV", "%", 0.f, 100.f);
  configParam(MID_VOL_CV, -1.f, 1.f, 0.f, "Mid Vol CV", "%", 0.f, 100.f);
  configParam(HIGH_PAN_CV, -1.f, 1.f, 0.f, "High Pan CV", "%", 0.f, 100.f);
  configParam(HIGH_VOL_CV, -1.f, 1.f, 0.f, "High Vol CV", "%", 0.f, 100.f);
  configParam(LOW_DRIVE_CV, -1.f, 1.f, 0.f, "Low Drive CV", "%", 0.f, 100.f);
  configParam(MID_DRIVE_CV, -1.f, 1.f, 0.f, "Mid Drive CV", "%", 0.f, 100.f);
  configParam(HIGH_DRIVE_CV, -1.f, 1.f, 0.f, "High Drive CV", "%", 0.f, 100.f);

  // Makeup manual por banda (G2): rango 1.0..3.981 = 0..+12.00 dB. La ley
  // interna sigue siendo lineal; solo el display del parametro se muestra en dB.
  configParam<MakeupParamQuantity>(LOW_MAKEUP, 1.f, 3.9810717f, 1.f, "Low Makeup", " dB");
  configParam<MakeupParamQuantity>(MID_MAKEUP, 1.f, 3.9810717f, 1.f, "Mid Makeup", " dB");
  configParam<MakeupParamQuantity>(HIGH_MAKEUP, 1.f, 3.9810717f, 1.f, "High Makeup", " dB");

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
  getParamQuantity(LOW_DRIVE_CV)->randomizeEnabled = false;
  getParamQuantity(MID_DRIVE_CV)->randomizeEnabled = false;
  getParamQuantity(HIGH_DRIVE_CV)->randomizeEnabled = false;
  getParamQuantity(LOW_MAKEUP)->randomizeEnabled = false;
  getParamQuantity(MID_MAKEUP)->randomizeEnabled = false;
  getParamQuantity(HIGH_MAKEUP)->randomizeEnabled = false;

  configInput(IN_L, "Left input");
  configInput(IN_R, "Right input");
  configInput(LOW_PAN_INPUT, "Low Pan CV");
  configInput(LOW_VOL_INPUT, "Low Vol CV");
  configInput(MID_PAN_INPUT, "Mid Pan CV");
  configInput(MID_VOL_INPUT, "Mid Vol CV");
  configInput(HIGH_PAN_INPUT, "High Pan CV");
  configInput(HIGH_VOL_INPUT, "High Vol CV");
  configInput(LOW_DRIVE_INPUT, "Low Drive CV");
  configInput(MID_DRIVE_INPUT, "Mid Drive CV");
  configInput(HIGH_DRIVE_INPUT, "High Drive CV");

  configOutput(OUT_L, "Left output");
  configOutput(OUT_R, "Right output");

  // Ctrl+E de Rack: ruta directa entrada -> salida mientras el modulo esta
  // bypasseado (engine::Module::configBypass). Sin esto Rack no sabe adonde
  // llevar la senal y el modulo se queda mudo al bypassearlo.
  configBypass(IN_L, OUT_L);
  configBypass(IN_R, OUT_R);

  lightDivider.setDivision(32);

  // Filtros disenados para la frecuencia de muestreo actual x factor de
  // sobremuestreo. onSampleRateChange() los reajusta al entrar en el engine.
  core.setSampleRate(44100.f);

  // Curva por defecto: TANH en las tres bandas (comportamiento v2.1.0).
  for (int i = 0; i < 3; i++)
    curves[i].store(coaldynamics::TANH, std::memory_order_relaxed);
}


/** Llamado desde UI o desde el JSON: solo registra la peticion para que el
hilo de audio la aplique al principio de un bloque. */
void Saturator::setOversampling(int factor) {
  if (factor != 1 && factor != 2 && factor != 4 && factor != 8)
    return;
  if (factor == oversampling)
    return;
  pendingOversampling.store(factor, std::memory_order_relaxed);
}

/** Llamado desde el hilo de audio: limpia estados y recalcula coeficientes. */
void Saturator::applyOversampling(int factor) {
  // Defensa: un factor fuera de {1, 2, 4, 8} dejaria fs = 0 en
  // updateFilterFrequencies() (250.f / 0 -> inf, y NaN en los biquads).
  if (factor != 1 && factor != 2 && factor != 4 && factor != 8)
    return;

  oversampling = factor;
  core.setOversampling(factor);
}

/** Llamado desde UI o JSON: valida 0 (LEGACY) / 1 (LR4). Igual que
setOversampling, solo publica la peticion; el audio la aplica al principio del
siguiente bloque. */
void Saturator::setCrossover(int mode) {
  if (mode != 0 && mode != 1)
    return;
  if (mode == crossover)
    return;
  pendingCrossover.store(mode, std::memory_order_relaxed);
}

/** Llamado desde el hilo de audio: resetea los estados de filtro y rehace los
 * coeficientes para la topologia pedida. El cambio de topologia produce un click
 * corto, aceptado en specs/v2.6_quality_spec.md 3.4. */
void Saturator::applyCrossover(int mode) {
  if (mode != 0 && mode != 1)
    return;
  crossover = mode;
  core.setCrossover(mode);
}

/** Llamado desde UI o desde el JSON: solo registra la peticion para que el
hilo de audio la aplique al principio de un bloque. */
void Saturator::setAdaa(int on) {
  const int v = (on != 0) ? 1 : 0;
  if (v == adaa) {
    // Cancela una peticion pendiente obsoleta: pedir el valor aplicado deja
    // el estado determinista aunque dataFromJson haya dejado pending=1.
    pendingAdaa.store(-1, std::memory_order_relaxed);
    return;
  }
  pendingAdaa.store(v, std::memory_order_relaxed);
}

/** Llamado desde el hilo de audio: fija el modo y limpia el historial. */
void Saturator::applyAdaa(int on) {
  adaa = (on != 0) ? 1 : 0;
  core.setAdaa(adaa);
}


void Saturator::onReset() {
  core.soloBand = solo.load(std::memory_order_relaxed);
  core.resetState();
}

/** Llamado desde UI o JSON: valida banda (0..2) e id (0..CURVE_COUNT-1).
Un valor invalido se ignora. Solo publica el atomico; el audio lo lee en el
siguiente bloque. */
void Saturator::setCurve(int band, int id) {
  if (band < 0 || band > 2)
    return;
  if (id < 0 || id >= coaldynamics::CURVE_COUNT)
    return;
  curves[band].store(id, std::memory_order_relaxed);
}

/** Llamado desde UI o JSON: valida -1 (Off) hasta 2 (High). Igual que
setCurve, solo publica el atomico; el audio lo lee cada muestra base. */
void Saturator::setSolo(int band) {
  if (band < -1 || band > 2)
    return;
  solo.store(band, std::memory_order_relaxed);
}

/** Llamado desde UI o JSON. Selector puro: no hay peticion pendiente porque
alternar el modo no reconstruye coeficientes ni resetea estado. */
void Saturator::setAutoMakeup(int on) {
  autoMakeup.store((on != 0) ? 1 : 0, std::memory_order_relaxed);
}

void Saturator::onSampleRateChange(const SampleRateChangeEvent &e) {
  sampleRate = e.sampleRate;
  core.setSampleRate(e.sampleRate);
}

json_t *Saturator::dataToJson() {
  json_t *rootJ = json_object();
  json_object_set_new(rootJ, "oversampling", json_integer(oversampling));

  // Modo de crossover (v2.6.0). Siempre se escribe; la lectura es estricta.
  json_object_set_new(rootJ, "crossover",
                      json_string(crossover == 1 ? "lr4" : "legacy"));
  // ADAA (v2.6.0). Siempre se escribe; la lectura es estricta (json_is_true).
  json_object_set_new(rootJ, "adaa", json_boolean(adaa));
  // Auto-makeup (v2.7). Siempre se escribe; la lectura es estricta.
  json_object_set_new(rootJ, "autoMakeup",
                      json_boolean(autoMakeup.load() != 0));

  json_t *curvesJ = json_array();
  for (int i = 0; i < 3; i++)
    json_array_append_new(curvesJ, json_integer(curves[i].load()));
  json_object_set_new(rootJ, "curves", curvesJ);

  json_object_set_new(rootJ, "solo", json_integer(solo.load()));

  // Modo link curves (v2.5.0). Siempre se escribe; la lectura (dataFromJson)
  // es estricta: solo true/false JSON cambia el modo.
  json_object_set_new(rootJ, "linkCurves", json_boolean(linkCurves));

  return rootJ;
}

void Saturator::dataFromJson(json_t *rootJ) {
  json_t *osJ = json_object_get(rootJ, "oversampling");
  if (osJ) {
    int factor = (int)json_integer_value(osJ);
    if (factor != 1 && factor != 2 && factor != 4 && factor != 8)
      factor = 1;
    setOversampling(factor);
  }

  // "crossover": solo "lr4" activa LR4; ausente o cualquier otro valor deja
  // LEGACY, que es el comportamiento exacto de v2.5.0.
  json_t *crossJ = json_object_get(rootJ, "crossover");
  int crossMode = 0;
  if (crossJ && json_is_string(crossJ) &&
      std::string(json_string_value(crossJ)) == "lr4")
    crossMode = 1;
  setCrossover(crossMode);
  // "adaa": SOLO el booleano JSON true activa ADAA (json_is_true). Ausente,
  // false, 1, "true" (string) o cualquier otro valor -> off (default v2.5.0).
  // Sin historial: cargar un patch no es una accion de usuario (regla v2.4.0).
  json_t *adaaJ = json_object_get(rootJ, "adaa");
  setAdaa((adaaJ && json_is_true(adaaJ)) ? 1 : 0);
  // "autoMakeup": SOLO el booleano JSON true lo activa (json_is_true). Ausente,
  // false, 1 o "true" (string) -> off (default v2.6.0). Sin historial: cargar
  // un patch no es una accion de usuario (regla v2.4.0).
  json_t *makeupJ = json_object_get(rootJ, "autoMakeup");
  setAutoMakeup((makeupJ && json_is_true(makeupJ)) ? 1 : 0);
  // Sin "curves" -> TANH en las tres bandas. Valor no entero o array corto
  // -> TANH en esa banda. Numero fuera de rango -> clamp 0..CURVE_COUNT-1.
  json_t *curvesJ = json_object_get(rootJ, "curves");
  for (int i = 0; i < 3; i++) {
    int id = coaldynamics::TANH;
    if (curvesJ && json_is_array(curvesJ)) {
      json_t *idJ = json_array_get(curvesJ, (size_t)i);
      if (idJ && json_is_integer(idJ))
        id = (int)json_integer_value(idJ);
    }
    if (id < 0)
      id = 0;
    if (id >= coaldynamics::CURVE_COUNT)
      id = coaldynamics::CURVE_COUNT - 1;
    setCurve(i, id);
  }

  // Sin "solo" -> Off (-1). No entero o fuera de -1..2 -> Off (-1).
  json_t *soloJ = json_object_get(rootJ, "solo");
  int soloValue = -1;
  if (soloJ && json_is_integer(soloJ))
    soloValue = (int)json_integer_value(soloJ);
  if (soloValue < -1 || soloValue > 2)
    soloValue = -1;
  setSolo(soloValue);

  // Modo link curves: sin la clave -> off (default; un patch v2.4.0 no trae
  // la clave y carga con el modo off, sin cambios de curvas). No booleano
  // ("x", 1, "true" como string) -> off. Solo exactamente true/false cambia
  // el modo, igual de estricto que "solo". Sin historial: cargar un patch no
  // es una accion de usuario (regla v2.4.0).
  json_t *linkJ = json_object_get(rootJ, "linkCurves");
  linkCurves =
      (linkJ && json_is_boolean(linkJ)) ? json_boolean_value(linkJ) : false;
}


void Saturator::process(const ProcessArgs &args) {
  // exchange() consume la peticion de forma atomica: un store de la UI cae o
  // antes o despues, nunca se pierde ni se lee a medias.
  const int requestedFactor =
      pendingOversampling.exchange(0, std::memory_order_relaxed);
  if (requestedFactor != 0)
    applyOversampling(requestedFactor);

  // Mismo patron para el crossover: la peticion se consume una vez por bloque.
  const int requestedCrossover =
      pendingCrossover.exchange(-1, std::memory_order_relaxed);
  if (requestedCrossover != -1)
    applyCrossover(requestedCrossover);

  // ADAA (v2.6.0): mismo patron pending + exchange que oversampling/crossover.
  const int requestedAdaa = pendingAdaa.exchange(-1, std::memory_order_relaxed);
  if (requestedAdaa != -1)
    applyAdaa(requestedAdaa);
  float inL = inputs[IN_L].getVoltage();
  float inR = inputs[IN_R].isConnected() ? inputs[IN_R].getVoltage() : inL;

  coaldynamics::OSParams p;
  p.driveLow = coaldynamics::driveFromCv(params[LOW_DRIVE].getValue(),
                                         inputs[LOW_DRIVE_INPUT].getVoltage(),
                                         params[LOW_DRIVE_CV].getValue());
  p.driveMid = coaldynamics::driveFromCv(params[MID_DRIVE].getValue(),
                                         inputs[MID_DRIVE_INPUT].getVoltage(),
                                         params[MID_DRIVE_CV].getValue());
  p.driveHigh = coaldynamics::driveFromCv(params[HIGH_DRIVE].getValue(),
                                          inputs[HIGH_DRIVE_INPUT].getVoltage(),
                                          params[HIGH_DRIVE_CV].getValue());
  p.curveLow = curves[0].load(std::memory_order_relaxed);
  p.curveMid = curves[1].load(std::memory_order_relaxed);
  p.curveHigh = curves[2].load(std::memory_order_relaxed);

  // Makeup por banda (G2): auto manda; con auto Off se usa el knob manual
  // (default 1.0 = 0 dB = identidad). Una lectura relaxed por muestra base.
  const bool autoOn = autoMakeup.load(std::memory_order_relaxed) != 0;
  p.makeupLow = autoOn ? core.autoMakeupDsp.gain(0) : params[LOW_MAKEUP].getValue();
  p.makeupMid = autoOn ? core.autoMakeupDsp.gain(1) : params[MID_MAKEUP].getValue();
  p.makeupHigh =
      autoOn ? core.autoMakeupDsp.gain(2) : params[HIGH_MAKEUP].getValue();

  p.panLow = clamp(params[LOW_PAN].getValue() +
                       (inputs[LOW_PAN_INPUT].getVoltage() / 10.f) *
                           params[LOW_PAN_CV].getValue(),
                   -1.f, 1.f);
  p.panMid = clamp(params[MID_PAN].getValue() +
                       (inputs[MID_PAN_INPUT].getVoltage() / 10.f) *
                           params[MID_PAN_CV].getValue(),
                   -1.f, 1.f);
  p.panHigh = clamp(params[HIGH_PAN].getValue() +
                        (inputs[HIGH_PAN_INPUT].getVoltage() / 10.f) *
                            params[HIGH_PAN_CV].getValue(),
                    -1.f, 1.f);

  p.volLow = clamp(params[LOW_VOL].getValue() +
                       (inputs[LOW_VOL_INPUT].getVoltage() / 10.f) *
                           params[LOW_VOL_CV].getValue(),
                   0.f, 1.f);
  p.volMid = clamp(params[MID_VOL].getValue() +
                       (inputs[MID_VOL_INPUT].getVoltage() / 10.f) *
                           params[MID_VOL_CV].getValue(),
                   0.f, 1.f);
  p.volHigh = clamp(params[HIGH_VOL].getValue() +
                        (inputs[HIGH_VOL_INPUT].getVoltage() / 10.f) *
                            params[HIGH_VOL_CV].getValue(),
                    0.f, 1.f);

  // Mix y salida: el core aplica ambos al final de process().
  p.mix = params[MIX_PARAM].getValue();
  p.outVol = params[OUTPUT_VOL].getValue();

  // Solo y rampas (~5 ms) viven en el core; basta con publicarle el objetivo.
  core.soloBand = solo.load(std::memory_order_relaxed);
  float outL = 0.f;
  float outR = 0.f;
  core.process(p, inL, inR, outL, outR, args.sampleTime);

  // outVol ya esta aplicado dentro del core: NO volver a multiplicar.
  outputs[OUT_L].setVoltage(outL);
  outputs[OUT_R].setVoltage(outR);

  if (lightDivider.process()) {
    float smoothTime = args.sampleTime * lightDivider.getDivision();
    lights[SATURE_LIGHT + 0].setSmoothBrightness(core.satLowMeter, smoothTime);
    lights[SATURE_LIGHT + 1].setSmoothBrightness(core.satLowMeter, smoothTime);
    lights[SATURE_LIGHT + 2].setSmoothBrightness(core.satMidMeter, smoothTime);
    lights[SATURE_LIGHT + 3].setSmoothBrightness(core.satMidMeter, smoothTime);
    lights[SATURE_LIGHT + 4].setSmoothBrightness(core.satHighMeter, smoothTime);
    lights[SATURE_LIGHT + 5].setSmoothBrightness(core.satHighMeter, smoothTime);
  }
}

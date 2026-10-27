#include "Saturator.hpp"

#include "plugin.hpp"

Plugin *pluginInstance;
Model *modelSaturator;

Saturator::Saturator() {
  config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

  configParam(LOW_DRIVE, 0.f, 1.f, 0.f, "Low Drive", "%", 0.f, 100.f);
  configParam<PanParamQuantity>(LOW_PAN, -1.f, 1.f, 0.f, "Low Pan", "");
  configParam(LOW_VOL, 0.f, 1.f, 0.8f, "Low Vol", "%", 0.f, 100.f);

  configParam(MID_DRIVE, 0.f, 1.f, 0.f, "Mid Drive", "%", 0.f, 100.f);
  configParam<PanParamQuantity>(MID_PAN, -1.f, 1.f, 0.f, "Mid Pan", "");
  configParam(MID_VOL, 0.f, 1.f, 0.8f, "Mid Vol", "%", 0.f, 100.f);

  configParam(HIGH_DRIVE, 0.f, 1.f, 0.f, "High Drive", "%", 0.f, 100.f);
  configParam<PanParamQuantity>(HIGH_PAN, -1.f, 1.f, 0.f, "High Pan", "");
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

  // Filtros disenados para la frecuencia de muestreo actual x factor de
  // sobremuestreo. onSampleRateChange() los reajusta al entrar en el engine.
  updateFilterFrequencies();

  // Curva por defecto: TANH en las tres bandas (comportamiento v2.1.0).
  for (int i = 0; i < 3; i++)
    curves[i].store(coaldynamics::TANH, std::memory_order_relaxed);

  const float twoPi = 6.283185307179586f;
  dcR = std::exp(-twoPi * 15.f / sampleRate);
}

/** Disena los crossovers a sampleRate * oversampling. Unico punto que toca
los coeficientes de los biquads. */
void Saturator::updateFilterFrequencies() {
  float fs = sampleRate * (float) oversampling;

  // Lowpass
  lowFilterL.setParameters(dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f,
                           1.0f);
  lowFilterR.setParameters(dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f,
                           1.0f);

  // Bandpass
  midFilterL.setParameters(dsp::BiquadFilter::BANDPASS, 1000.f / fs, 1.0f,
                           1.0f);
  midFilterR.setParameters(dsp::BiquadFilter::BANDPASS, 1000.f / fs, 1.0f,
                           1.0f);

  // Highpass
  highFilterL.setParameters(dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f,
                            1.0f);
  highFilterR.setParameters(dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f,
                            1.0f);
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

  if (factor == 2)
    stage2.reset();
  else if (factor == 4)
    stage4.reset();
  else if (factor == 8)
    stage8.reset();

  dryDelay = (factor > 1) ? coaldynamics::OS_LATENCY : 0;
  for (int i = 0; i <= coaldynamics::OS_LATENCY; i++) {
    dryBufferL[i] = 0.f;
    dryBufferR[i] = 0.f;
  }
  dryIndex = 0;

  lowFilterL.reset();
  lowFilterR.reset();
  midFilterL.reset();
  midFilterR.reset();
  highFilterL.reset();
  highFilterR.reset();

  updateFilterFrequencies();
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

void Saturator::onSampleRateChange(const SampleRateChangeEvent &e) {
  sampleRate = e.sampleRate;

  const float twoPi = 6.283185307179586f;
  dcR = std::exp(-twoPi * 15.f / sampleRate);
  dcX1L = 0.f;
  dcX1R = 0.f;
  dcY1L = 0.f;
  dcY1R = 0.f;

  for (int i = 0; i <= coaldynamics::OS_LATENCY; i++) {
    dryBufferL[i] = 0.f;
    dryBufferR[i] = 0.f;
  }
  dryIndex = 0;

  updateFilterFrequencies();
}

json_t *Saturator::dataToJson() {
  json_t *rootJ = json_object();
  json_object_set_new(rootJ, "oversampling", json_integer(oversampling));

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
    int factor = (int) json_integer_value(osJ);
    if (factor != 1 && factor != 2 && factor != 4 && factor != 8)
      factor = 1;
    setOversampling(factor);
  }

  // Sin "curves" -> TANH en las tres bandas. Valor no entero o array corto
  // -> TANH en esa banda. Numero fuera de rango -> clamp 0..CURVE_COUNT-1.
  json_t *curvesJ = json_object_get(rootJ, "curves");
  for (int i = 0; i < 3; i++) {
    int id = coaldynamics::TANH;
    if (curvesJ && json_is_array(curvesJ)) {
      json_t *idJ = json_array_get(curvesJ, (size_t) i);
      if (idJ && json_is_integer(idJ))
        id = (int) json_integer_value(idJ);
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
    soloValue = (int) json_integer_value(soloJ);
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

/** Procesa un bloque de Stage::FACTOR submuestras: filtros + curva + pan/vol a
frecuencia sobremuestreada, y decimacion de la suma de bandas. */
template <class Stage>
void Saturator::processBands(Stage &stage, const OSParams &p, float inL,
                             float inR, float &outL, float &outR) {
  const int N = Stage::FACTOR;
  float highL[N];
  float highR[N];
  float wetL[N];
  float wetR[N];

  stage.upsample(inL, inR, highL, highR);

  for (int i = 0; i < N; i++) {
    float xL = highL[i];
    float xR = highR[i];

    float lowL = lowFilterL.process(xL);
    float lowR = lowFilterR.process(xR);
    float midL = midFilterL.process(xL);
    float midR = midFilterR.process(xR);
    float upperL = highFilterL.process(xL);
    float upperR = highFilterR.process(xR);

    float wetLowL = coaldynamics::applyCurve(
        coaldynamics::curveDrive(lowL, p.driveLow, p.curveLow), p.curveLow);
    float wetLowR = coaldynamics::applyCurve(
        coaldynamics::curveDrive(lowR, p.driveLow, p.curveLow), p.curveLow);
    float wetMidL = coaldynamics::applyCurve(
        coaldynamics::curveDrive(midL, p.driveMid, p.curveMid), p.curveMid);
    float wetMidR = coaldynamics::applyCurve(
        coaldynamics::curveDrive(midR, p.driveMid, p.curveMid), p.curveMid);
    float wetUpperL = coaldynamics::applyCurve(
        coaldynamics::curveDrive(upperL, p.driveHigh, p.curveHigh),
        p.curveHigh);
    float wetUpperR = coaldynamics::applyCurve(
        coaldynamics::curveDrive(upperR, p.driveHigh, p.curveHigh),
        p.curveHigh);

    // Ley de pan: balance lineal con corte total (G_L=min(1,1-p),
    // G_R=min(1,1+p)); nunca gana >1
    float lowOutL =
        wetLowL * p.volLow * std::min(1.f, 1.f - p.panLow) * p.soloGainLow;
    float lowOutR =
        wetLowR * p.volLow * std::min(1.f, 1.f + p.panLow) * p.soloGainLow;

    float midOutL =
        wetMidL * p.volMid * std::min(1.f, 1.f - p.panMid) * p.soloGainMid;
    float midOutR =
        wetMidR * p.volMid * std::min(1.f, 1.f + p.panMid) * p.soloGainMid;

    float upperOutL = wetUpperL * p.volHigh *
                      std::min(1.f, 1.f - p.panHigh) * p.soloGainHigh;
    float upperOutR = wetUpperR * p.volHigh *
                      std::min(1.f, 1.f + p.panHigh) * p.soloGainHigh;

    wetL[i] = lowOutL + midOutL + upperOutL;
    wetR[i] = lowOutR + midOutR + upperOutR;

    if (i == N - 1) {
      satLowMeter = std::min(
          std::abs(wetLowL -
                   coaldynamics::curveDrive(lowL, p.driveLow, p.curveLow)),
          1.f);
      satMidMeter = std::min(
          std::abs(wetMidL -
                   coaldynamics::curveDrive(midL, p.driveMid, p.curveMid)),
          1.f);
      satHighMeter = std::min(
          std::abs(wetUpperL -
                   coaldynamics::curveDrive(upperL, p.driveHigh, p.curveHigh)),
          1.f);
    }
  }

  outL = stage.downsampleL(wetL);
  outR = stage.downsampleR(wetR);
}

void Saturator::process(const ProcessArgs &args) {
  // exchange() consume la peticion de forma atomica: un store de la UI cae o
  // antes o despues, nunca se pierde ni se lee a medias.
  const int requestedFactor =
      pendingOversampling.exchange(0, std::memory_order_relaxed);
  if (requestedFactor != 0)
    applyOversampling(requestedFactor);

  float inL = inputs[IN_L].getVoltage();
  float inR = inputs[IN_R].isConnected() ? inputs[IN_R].getVoltage() : inL;

  OSParams p;
  p.driveLow = 1.f + params[LOW_DRIVE].getValue() * 10.f;
  p.driveMid = 1.f + params[MID_DRIVE].getValue() * 10.f;
  p.driveHigh = 1.f + params[HIGH_DRIVE].getValue() * 10.f;
  p.curveLow = curves[0].load(std::memory_order_relaxed);
  p.curveMid = curves[1].load(std::memory_order_relaxed);
  p.curveHigh = curves[2].load(std::memory_order_relaxed);

  // Rampa de solo/dry ~5 ms: objetivo segun `solo`, un paso lineal por muestra
  // base (240 pasos a 48 kHz: step = sampleTime * 200). Se calcula UNA vez por
  // process(), a frecuencia base; el valor queda constante en las N
  // submuestras del bloque de oversampling, asi que no hay artefacto al
  // cambiar de stage (1x/2x/4x/8x).
  const int soloBand = solo.load(std::memory_order_relaxed);
  const float soloStep = args.sampleTime * 200.f;
  for (int b = 0; b < 3; b++) {
    float target = (soloBand < 0 || soloBand == b) ? 1.f : 0.f;
    if (soloGain[b] < target)
      soloGain[b] = std::min(target, soloGain[b] + soloStep);
    else
      soloGain[b] = std::max(target, soloGain[b] - soloStep);
  }
  float dryTarget = (soloBand < 0) ? 1.f : 0.f;
  if (dryGain < dryTarget)
    dryGain = std::min(dryTarget, dryGain + soloStep);
  else
    dryGain = std::max(dryTarget, dryGain - soloStep);
  p.soloGainLow = soloGain[0];
  p.soloGainMid = soloGain[1];
  p.soloGainHigh = soloGain[2];

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

  float wetL = 0.f;
  float wetR = 0.f;
  switch (oversampling) {
    case 1:
      processBands(stage1, p, inL, inR, wetL, wetR);
      break;
    case 2:
      processBands(stage2, p, inL, inR, wetL, wetR);
      break;
    case 8:
      processBands(stage8, p, inL, inR, wetL, wetR);
      break;
    default:
      processBands(stage4, p, inL, inR, wetL, wetR);
      break;
  }

  // DC-blocker de 15 Hz, solo en el camino wet: y = x - x1 + R*y1.
  float blockedL = wetL - dcX1L + dcR * dcY1L;
  dcX1L = wetL;
  dcY1L = blockedL;
  wetL = blockedL;

  float blockedR = wetR - dcX1R + dcR * dcY1R;
  dcX1R = wetR;
  dcY1R = blockedR;
  wetR = blockedR;

  // El dry se retrasa lo mismo que el wet para no cancelar en peine al mezclar
  dryBufferL[dryIndex] = inL;
  dryBufferR[dryIndex] = inR;
  int readIndex = dryIndex - dryDelay;
  if (readIndex < 0)
    readIndex += coaldynamics::OS_LATENCY + 1;
  float dryL = dryBufferL[readIndex];
  float dryR = dryBufferR[readIndex];
  dryIndex++;
  if (dryIndex > coaldynamics::OS_LATENCY)
    dryIndex = 0;

  float mix = params[MIX_PARAM].getValue();
  float outL = dryL * (1.f - mix) * dryGain + wetL * mix;
  float outR = dryR * (1.f - mix) * dryGain + wetR * mix;

  float outVol = params[OUTPUT_VOL].getValue();
  outputs[OUT_L].setVoltage(outL * outVol);
  outputs[OUT_R].setVoltage(outR * outVol);

  if (lightDivider.process()) {
    float smoothTime = args.sampleTime * lightDivider.getDivision();
    lights[SATURE_LIGHT + 0].setSmoothBrightness(satLowMeter, smoothTime);
    lights[SATURE_LIGHT + 1].setSmoothBrightness(satLowMeter, smoothTime);
    lights[SATURE_LIGHT + 2].setSmoothBrightness(satMidMeter, smoothTime);
    lights[SATURE_LIGHT + 3].setSmoothBrightness(satMidMeter, smoothTime);
    lights[SATURE_LIGHT + 4].setSmoothBrightness(satHighMeter, smoothTime);
    lights[SATURE_LIGHT + 5].setSmoothBrightness(satHighMeter, smoothTime);
  }
}

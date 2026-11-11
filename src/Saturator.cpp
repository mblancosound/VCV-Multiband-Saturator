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

  // Makeup manual por banda (G2): 1.0 = 0 dB; 4.0 = +12.04 dB. La ley interna
  // sigue siendo lineal 0..4; solo el display del parametro se muestra en dB.
  configParam<MakeupParamQuantity>(LOW_MAKEUP, 0.f, 4.f, 1.f, "Low Makeup", " dB");
  configParam<MakeupParamQuantity>(MID_MAKEUP, 0.f, 4.f, 1.f, "Mid Makeup", " dB");
  configParam<MakeupParamQuantity>(HIGH_MAKEUP, 0.f, 4.f, 1.f, "High Makeup", " dB");

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
  updateFilterFrequencies();

  // Curva por defecto: TANH en las tres bandas (comportamiento v2.1.0).
  for (int i = 0; i < 3; i++)
    curves[i].store(coaldynamics::TANH, std::memory_order_relaxed);

  const float twoPi = 6.283185307179586f;
  dcR = std::exp(-twoPi * 15.f / sampleRate);

  // G2: coeficientes del detector a la fs base. onSampleRateChange() los
  // reajusta al entrar en el engine, igual que dcR y los biquads.
  autoMakeupDsp.setSampleRate(sampleRate);
}

/** Disena los crossovers a sampleRate * oversampling. Unico punto que toca
los coeficientes de los biquads; la topologia depende de `crossover`. */
void Saturator::updateFilterFrequencies() {
  const float fs = sampleRate * (float) oversampling;

  if (crossover == 1) {
    // LR4 (specs/v2.6_quality_spec.md 3.1): Low = LP4 y High = HP4 como
    // cascada de dos Butterworth de 2o orden (Q = 0.707), a fc y fc*sqrt(2).
    // La banda Mid no usa filtro: se deriva como x - low - high en
    // processBands(), de modo que low + mid + high reconstruye la entrada.
    lowFilterL.setParameters(dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f,
                             1.0f);
    lowFilterR.setParameters(dsp::BiquadFilter::LOWPASS, 250.f / fs, 0.707f,
                             1.0f);
    lowFilter2L.setParameters(dsp::BiquadFilter::LOWPASS, 353.55339f / fs,
                              0.707f, 1.0f);
    lowFilter2R.setParameters(dsp::BiquadFilter::LOWPASS, 353.55339f / fs,
                              0.707f, 1.0f);

    highFilterL.setParameters(dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f,
                              1.0f);
    highFilterR.setParameters(dsp::BiquadFilter::HIGHPASS, 4000.f / fs, 0.707f,
                              1.0f);
    highFilter2L.setParameters(dsp::BiquadFilter::HIGHPASS, 5656.85425f / fs,
                               0.707f, 1.0f);
    highFilter2R.setParameters(dsp::BiquadFilter::HIGHPASS, 5656.85425f / fs,
                               0.707f, 1.0f);
    return;
  }

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
  resetAdaaState();   // historial en voltios no vale tras cambiar fsN
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
coeficientes para la topologia pedida. El cambio de topologia produce un click
corto, aceptado en specs/v2.6_quality_spec.md 3.4. */
void Saturator::applyCrossover(int mode) {
  if (mode != 0 && mode != 1)
    return;
  crossover = mode;

  lowFilterL.reset();
  lowFilterR.reset();
  lowFilter2L.reset();
  lowFilter2R.reset();
  midFilterL.reset();
  midFilterR.reset();
  highFilterL.reset();
  highFilterR.reset();
  highFilter2L.reset();
  highFilter2R.reset();

  updateFilterFrequencies();
  resetAdaaState();
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
  resetAdaaState();
}

void Saturator::resetAdaaState() {
  for (int b = 0; b < 3; b++)
    uPrev[b][0] = uPrev[b][1] = 0.f;
  adaaResync[0] = adaaResync[1] = adaaResync[2] = true;
  adaaCurveApplied[0] = adaaCurveApplied[1] = adaaCurveApplied[2] = -1;
}

void Saturator::onReset() {
  // 1) Delegado: stage activo, dryDelay, dryBufferL/R (OS_LATENCY+1), dryIndex,
  //    los 6 biquads de la primera etapa, updateFilterFrequencies() y el
  //    historial ADAA (uPrev + adaaResync + adaaCurveApplied).
  applyOversampling(oversampling);

  // 2) Segunda etapa de la cascada LR4 (low/high x L/R): applyOversampling NO
  //    los resetea y setParameters() tampoco limpia el estado (ver 0/E2).
  lowFilter2L.reset();
  lowFilter2R.reset();
  highFilter2L.reset();
  highFilter2R.reset();

  // 3) DC-blocker: solo el ESTADO. dcR es un coeficiente dependiente de
  //    sampleRate; ponerlo a 0 lo convertiria en un diferenciador (ver 0/E1).
  dcX1L = 0.f;
  dcX1R = 0.f;
  dcY1L = 0.f;
  dcY1R = 0.f;

  // 4) Rampas: se asientan en su OBJETIVO, no en cero (ver 0/E3). Escribirlas
  //    a 0 haria que inicializar el modulo arrancase con un fade de 5 ms.
  const int soloBand = solo.load(std::memory_order_relaxed);
  for (int b = 0; b < 3; b++)
    soloGain[b] = (soloBand < 0 || soloBand == b) ? 1.f : 0.f;
  dryGain = (soloBand < 0) ? 1.f : 0.f;

  // 5) Medidores de saturacion: no intervienen en el audio (solo alimentan las
  //    6 luces), pero un reset debe dejar el panel sin historia.
  satLowMeter = 0.f;
  satMidMeter = 0.f;
  satHighMeter = 0.f;

  // 6) Detectores de auto-gain: envolventes a 0 y gainS a 1 (G2).
  autoMakeupDsp.reset();
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

  // G2: solo coeficientes. NO reset(): envIn/envOut/gainS son amplitudes y
  // siguen siendo validas tras cambiar de sample rate.
  autoMakeupDsp.setSampleRate(e.sampleRate);

  updateFilterFrequencies();
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
    int factor = (int) json_integer_value(osJ);
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

/** Elige topologia y ADAA una sola vez por bloque: los dos son parametros de
plantilla, de modo que el compilador genera una version de processBands por
combinacion y no queda ninguna rama por muestra ni por submuestra. */
template <class Stage>
void Saturator::processBlock(Stage &stage, const OSParams &p, float inL,
                             float inR, float &outL, float &outR) {
  if (crossover == 1) {
    if (adaa == 1)
      processBands<Stage, true, true>(stage, p, inL, inR, outL, outR);
    else
      processBands<Stage, true, false>(stage, p, inL, inR, outL, outR);
  } else {
    if (adaa == 1)
      processBands<Stage, false, true>(stage, p, inL, inR, outL, outR);
    else
      processBands<Stage, false, false>(stage, p, inL, inR, outL, outR);
  }
}

/** Procesa un bloque de Stage::FACTOR submuestras: filtros + curva + pan/vol a
frecuencia sobremuestreada, y decimacion de la suma de bandas. */
template <class Stage, bool LR4, bool ADAA>
void Saturator::processBands(Stage &stage, const OSParams &p, float inL,
                             float inR, float &outL, float &outR) {
  const int N = Stage::FACTOR;
  float highL[N];
  float highR[N];
  float wetL[N];
  float wetR[N];

  stage.upsample(inL, inR, highL, highR);

  // Curva por banda, leida una vez por bloque.
  const int curve[3] = {p.curveLow, p.curveMid, p.curveHigh};

  for (int i = 0; i < N; i++) {
    float xL = highL[i];
    float xR = highR[i];

    float lowL, lowR, midL, midR, upperL, upperR;
    if (LR4) {
      // Low = LP4 y High = HP4 en cascada; Mid se deriva para que
      // low + mid + high reconstruya x (specs/v2.6_quality_spec.md 3.1).
      // LR4 es constante de plantilla: no hay rama por muestra.
      lowL = lowFilter2L.process(lowFilterL.process(xL));
      lowR = lowFilter2R.process(lowFilterR.process(xR));
      upperL = highFilter2L.process(highFilterL.process(xL));
      upperR = highFilter2R.process(highFilterR.process(xR));
      midL = xL - lowL - upperL;
      midR = xR - lowR - upperR;
    } else {
      lowL = lowFilterL.process(xL);
      lowR = lowFilterR.process(xR);
      midL = midFilterL.process(xL);
      midR = midFilterR.process(xR);
      upperL = highFilterL.process(xL);
      upperR = highFilterR.process(xR);
    }

    // u[banda][canal] en voltios post-curveDrive.
    float u[3][2];
    u[0][0] = coaldynamics::curveDrive(lowL, p.driveLow, curve[0]);
    u[0][1] = coaldynamics::curveDrive(lowR, p.driveLow, curve[0]);
    u[1][0] = coaldynamics::curveDrive(midL, p.driveMid, curve[1]);
    u[1][1] = coaldynamics::curveDrive(midR, p.driveMid, curve[1]);
    u[2][0] = coaldynamics::curveDrive(upperL, p.driveHigh, curve[2]);
    u[2][1] = coaldynamics::curveDrive(upperR, p.driveHigh, curve[2]);

    float wet[3][2];
    if (ADAA) {
      // Resync: uPrev = u en la PRIMERA submuestra del bloque, por banda y
      // canal. Tras el resync applyCurveAdaa ve du = 0 -> fallback -> f(u).
      // Solo ocurre cuando lo marca process() (cambio de curva) o resetAdaaState().
      if (i == 0) {
        for (int b = 0; b < 3; b++) {
          if (adaaResync[b]) {
            uPrev[b][0] = u[b][0];
            uPrev[b][1] = u[b][1];
            adaaResync[b] = false;
          }
        }
      }
      for (int b = 0; b < 3; b++) {
        wet[b][0] = coaldynamics::applyCurveAdaa(u[b][0], uPrev[b][0], curve[b]);
        wet[b][1] = coaldynamics::applyCurveAdaa(u[b][1], uPrev[b][1], curve[b]);
      }
    } else {
      // Ruta v2.5.0 exacta: sin historial y sin rama.
      for (int b = 0; b < 3; b++) {
        wet[b][0] = coaldynamics::applyCurve(u[b][0], curve[b]);
        wet[b][1] = coaldynamics::applyCurve(u[b][1], curve[b]);
      }
    }

    // Ley de pan: balance lineal con corte total (G_L=min(1,1-p),
    // G_R=min(1,1+p)); nunca gana >1
    float lowOutL = wet[0][0] * p.makeupLow * p.volLow *
                    std::min(1.f, 1.f - p.panLow) * p.soloGainLow;
    float lowOutR = wet[0][1] * p.makeupLow * p.volLow *
                    std::min(1.f, 1.f + p.panLow) * p.soloGainLow;

    float midOutL = wet[1][0] * p.makeupMid * p.volMid *
                    std::min(1.f, 1.f - p.panMid) * p.soloGainMid;
    float midOutR = wet[1][1] * p.makeupMid * p.volMid *
                    std::min(1.f, 1.f + p.panMid) * p.soloGainMid;

    float upperOutL = wet[2][0] * p.makeupHigh * p.volHigh *
                      std::min(1.f, 1.f - p.panHigh) * p.soloGainHigh;
    float upperOutR = wet[2][1] * p.makeupHigh * p.volHigh *
                      std::min(1.f, 1.f + p.panHigh) * p.soloGainHigh;

    wetL[i] = lowOutL + midOutL + upperOutL;
    wetR[i] = lowOutR + midOutR + upperOutR;

    if (i == N - 1) {
      // Medidor de la ultima submuestra, sin cambios desde v2.2: |wet - u| es
      // cuanto ha movido la curva a la entrada ya escalada por el drive.
      satLowMeter = std::min(std::abs(wet[0][0] - u[0][0]), 1.f);
      satMidMeter = std::min(std::abs(wet[1][0] - u[1][0]), 1.f);
      satHighMeter = std::min(std::abs(wet[2][0] - u[2][0]), 1.f);

      // Auto-makeup (ley corregida en 2.7.1): se compara la senal que ENTRA en
      // la no-linealidad (u, post-curveDrive) con la que SALE de ella (wet),
      // descontando el TRIM estatico de la curva. Asi la ganancia vale ~1 en
      // regimen lineal y NO cancela el mando de drive; solo compensa la
      // compresion real de la curva. La version anterior comparaba la banda
      // pre-drive con la salida post-curva, dos escalas distintas: daba 3.57
      // constante en CLEAN y ~1/drive en regimen lineal.
      float dIn[3];
      float dOut[3];
      dIn[0] = 0.5f * (std::abs(u[0][0]) + std::abs(u[0][1]));
      dIn[1] = 0.5f * (std::abs(u[1][0]) + std::abs(u[1][1]));
      dIn[2] = 0.5f * (std::abs(u[2][0]) + std::abs(u[2][1]));
      for (int b = 0; b < 3; b++)
        dOut[b] = 0.5f * (std::abs(wet[b][0]) + std::abs(wet[b][1])) /
                  coaldynamics::TRIM[curve[b]];
      autoMakeupDsp.update(dIn, dOut);
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

  // Mismo patron para el crossover: la peticion se consume una vez por bloque.
  const int requestedCrossover =
      pendingCrossover.exchange(-1, std::memory_order_relaxed);
  if (requestedCrossover != -1)
    applyCrossover(requestedCrossover);

  // ADAA (v2.6.0): mismo patron pending + exchange que oversampling/crossover.
  const int requestedAdaa =
      pendingAdaa.exchange(-1, std::memory_order_relaxed);
  if (requestedAdaa != -1)
    applyAdaa(requestedAdaa);
  float inL = inputs[IN_L].getVoltage();
  float inR = inputs[IN_R].isConnected() ? inputs[IN_R].getVoltage() : inL;

  OSParams p;
  p.driveLow = coaldynamics::driveFromCv(params[LOW_DRIVE].getValue(),
                                         inputs[LOW_DRIVE_INPUT].getVoltage(),
                                         params[LOW_DRIVE_CV].getValue());
  p.driveMid = coaldynamics::driveFromCv(params[MID_DRIVE].getValue(),
                                         inputs[MID_DRIVE_INPUT].getVoltage(),
                                         params[MID_DRIVE_CV].getValue());
  p.driveHigh = coaldynamics::driveFromCv(
      params[HIGH_DRIVE].getValue(), inputs[HIGH_DRIVE_INPUT].getVoltage(),
      params[HIGH_DRIVE_CV].getValue());
  p.curveLow = curves[0].load(std::memory_order_relaxed);
  p.curveMid = curves[1].load(std::memory_order_relaxed);
  p.curveHigh = curves[2].load(std::memory_order_relaxed);

  // Cambio de curva aplicada con ADAA: resync de uPrev en la submuestra 0.
  if (p.curveLow != adaaCurveApplied[0]) {
    adaaCurveApplied[0] = p.curveLow;
    adaaResync[0] = true;
  }
  if (p.curveMid != adaaCurveApplied[1]) {
    adaaCurveApplied[1] = p.curveMid;
    adaaResync[1] = true;
  }
  if (p.curveHigh != adaaCurveApplied[2]) {
    adaaCurveApplied[2] = p.curveHigh;
    adaaResync[2] = true;
  }

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

  // Makeup por banda (G2): auto manda; con auto Off se usa el knob manual
  // (default 1.0 = 0 dB = identidad). Una lectura relaxed por muestra base.
  const bool autoOn = autoMakeup.load(std::memory_order_relaxed) != 0;
  p.makeupLow = autoOn ? autoMakeupDsp.gain(0) : params[LOW_MAKEUP].getValue();
  p.makeupMid = autoOn ? autoMakeupDsp.gain(1) : params[MID_MAKEUP].getValue();
  p.makeupHigh =
      autoOn ? autoMakeupDsp.gain(2) : params[HIGH_MAKEUP].getValue();

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
      processBlock(stage1, p, inL, inR, wetL, wetR);
      break;
    case 2:
      processBlock(stage2, p, inL, inR, wetL, wetR);
      break;
    case 8:
      processBlock(stage8, p, inL, inR, wetL, wetR);
      break;
    default:
      processBlock(stage4, p, inL, inR, wetL, wetR);
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

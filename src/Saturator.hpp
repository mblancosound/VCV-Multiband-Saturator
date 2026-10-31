#pragma once

#include <atomic>

#include <rack.hpp>

#include "Oversampling.hpp"
#include "Curves.hpp"

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
    /** Ganancia de solo con rampa (0..1) de cada banda, resuelta por muestra base. */
    float soloGainLow, soloGainMid, soloGainHigh;
    int curveLow, curveMid, curveHigh;
  };

  dsp::ClockDivider lightDivider;

  dsp::BiquadFilter lowFilterL, lowFilterR;
  dsp::BiquadFilter midFilterL, midFilterR;
  dsp::BiquadFilter highFilterL, highFilterR;

  /** Segunda etapa de la cascada LR4: Low = LP4 (lowFilter -> lowFilter2),
      High = HP4 (highFilter -> highFilter2). En modo LEGACY no se usan. */
  dsp::BiquadFilter lowFilter2L, lowFilter2R;
  dsp::BiquadFilter highFilter2L, highFilter2R;

  /** Modo de crossover: 0 = LEGACY (LP 250 / BP 1k / HP 4k en paralelo),
      1 = LR4 (Low = LP4, High = HP4, Mid = x - low - high). Solo lo escribe
      applyCrossover() en el hilo de audio. Por defecto 0: un patch v2.5.0
      carga y suena bit-identico. */
  int crossover = 0;
  /** Peticion pendiente de cambio de modo (-1 = ninguna). La escriben la UI y
      el JSON; la consume el hilo de audio con exchange(). */
  std::atomic<int> pendingCrossover{-1};

  coaldynamics::OversampledStage<2, coaldynamics::OS_QUALITY> stage2;
  coaldynamics::OversampledStage<4, coaldynamics::OS_QUALITY> stage4;
  coaldynamics::OversampledStage<8, coaldynamics::OS_QUALITY> stage8;
  coaldynamics::BypassStage<coaldynamics::OS_QUALITY> stage1;

  /** 1, 2, 4 u 8. Sólo lo escribe applyOversampling() en el hilo de audio.
      Por defecto 1 (off): el oversampling es opt-in. */
  int oversampling = 1;
  /** Pedido pendiente de aplicar (0 = ninguno). Lo escribe la UI o el JSON y lo
      consume el hilo de audio con exchange(), por eso es atómico. */
  std::atomic<int> pendingOversampling{0};
  float sampleRate = 44100.f;

  /** Curva elegida por banda (0 = Low, 1 = Mid, 2 = High). La UI y el JSON
      la escriben; el hilo de audio la lee una vez por bloque. */
  std::atomic<int> curves[3];

  /** Modo "link curves": si esta activo, un cambio de curva desde el menu
      (Low/Mid/High) aplica la curva a las tres bandas de una vez. Lo escriben
      la UI (toggle del menu) y el JSON; el hilo de audio nunca lo lee. Por
      defecto false: un patch antiguo sin la clave carga con el modo off. */
  bool linkCurves = false;

  /** Solo por banda: -1 = Off, 0 = Low, 1 = Mid, 2 = High. La UI y el JSON
      lo escriben con setSolo(); el audio lo lee cada muestra base. */
  std::atomic<int> solo{-1};
  /** Ganancias del solo con rampa (~5 ms): 0..1 por banda y para el camino
      dry. Solo hilo de audio; arrancan en 1 y la rampa las lleva al objetivo. */
  float soloGain[3] = {1.f, 1.f, 1.f};
  float dryGain = 1.f;

  /** DC-blocker de 15 Hz, solo en el camino wet: coeficiente y estado. */
  float dcR = 0.f;
  float dcX1L = 0.f, dcX1R = 0.f;
  float dcY1L = 0.f, dcY1R = 0.f;

  /** Delay de la señal dry para compensar la latencia del sobremuestreo. */
  float dryBufferL[coaldynamics::OS_LATENCY + 1] = {};
  float dryBufferR[coaldynamics::OS_LATENCY + 1] = {};
  int dryIndex = 0;
  /** 1x por defecto: sin latencia de oversampling hasta que se active. */
  int dryDelay = 0;

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
  /** Llamado desde UI o JSON: valida 0 (LEGACY) / 1 (LR4) y deja la peticion. */
  void setCrossover(int mode);
  /** Llamado desde UI/JSON: valida y publica la curva de una banda. */
  void setCurve(int band, int id);
  /** Llamado desde UI o JSON: valida -1 (Off) hasta 2 (High) y publica el solo. */
  void setSolo(int band);
  /** Called from the audio thread: resets state and rebuilds coefficients. */
  void applyOversampling(int factor);
  /** Llamado desde el hilo de audio: resetea filtros y rehace coeficientes. */
  void applyCrossover(int mode);
  void updateFilterFrequencies();

  /** Elige topologia una vez por bloque (no por muestra). */
  template <class Stage>
  void processBlock(Stage &stage, const OSParams &p, float inL, float inR,
                    float &outL, float &outR);

  template <class Stage, bool LR4>
  void processBands(Stage &stage, const OSParams &p, float inL, float inR,
                    float &outL, float &outR);
};
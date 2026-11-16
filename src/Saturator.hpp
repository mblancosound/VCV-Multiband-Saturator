#pragma once

#include <atomic>

#include <rack.hpp>

#include "Oversampling.hpp"
#include "Curves.hpp"
#include "AutoMakeup.hpp"
#include "SaturatorDsp.hpp"

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

/** Formato de los knobs MAKEUP: ganancia lineal interna 1..3.981 mostrada en dB
(0..+12 dB). Sobreescribe solo el string de display; el DSP sigue multiplicando
por el valor lineal.
*/
struct MakeupParamQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    float v = getValue();
    if (v <= 1e-6f)
      return "-inf";
    float db = 20.f * std::log10(v);
    if (db > -0.005f && db < 0.005f)
      return "0.00";
    return string::f("%+.2f", db);
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
    /** CV de Drive por banda (G1). Atenuadores bipolares; ids 17..19. */
    LOW_DRIVE_CV,
    MID_DRIVE_CV,
    HIGH_DRIVE_CV,
    /** Makeup manual por banda (G2). Ids 20..22; 1.0 = 0 dB. */
    LOW_MAKEUP,
    MID_MAKEUP,
    HIGH_MAKEUP,
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
    /** Entradas CV de Drive por banda (G1). Ids 8..10. */
    LOW_DRIVE_INPUT,
    MID_DRIVE_INPUT,
    HIGH_DRIVE_INPUT,
    NUM_INPUTS
  };
  enum OutputIds { OUT_L, OUT_R, NUM_OUTPUTS };
  enum LightIds { ENUMS(SATURE_LIGHT, 6), NUM_LIGHTS };


  dsp::ClockDivider lightDivider;

  coaldynamics::SaturatorDsp core;

  /** Modo de crossover: 0 = LEGACY (LP 250 / BP 1k / HP 4k en paralelo),
      1 = LR4 (Low = LP4, High = HP4, Mid = x - low - high). Solo lo escribe
      applyCrossover() en el hilo de audio. Por defecto 0: un patch v2.5.0
      carga y suena bit-identico. */
  int crossover = 0;
  /** Peticion pendiente de cambio de modo (-1 = ninguna). La escriben la UI y
      el JSON; la consume el hilo de audio con exchange(). */
  std::atomic<int> pendingCrossover{-1};

  /** ADAA1 (v2.6.0): 0 = off (default, comportamiento v2.5.0), 1 = on.
      Solo lo escribe applyAdaa() en el hilo de audio. */
  int adaa = 0;
  /** Peticion pendiente de toggle (-1 = ninguna). La escriben la UI y el JSON;
      la consume el hilo de audio con exchange(). */
  std::atomic<int> pendingAdaa{-1};

  /** Auto-makeup (G2): 0 = Off (default, comportamiento v2.6.0), 1 = On.
      Selector puro: la UI/JSON lo escriben, el audio lo lee una vez por
      muestra base con memory_order_relaxed. No hay pending ni exchange porque
      alternarlo no reconstruye coeficientes ni resetea estado (los detectores
      corren siempre). */
  std::atomic<int> autoMakeup{0};

  /** 1, 2, 4 u 8. SÃ³lo lo escribe applyOversampling() en el hilo de audio.
      Por defecto 1 (off): el oversampling es opt-in. */
  int oversampling = 1;
  /** Pedido pendiente de aplicar (0 = ninguno). Lo escribe la UI o el JSON y lo
      consume el hilo de audio con exchange(), por eso es atÃ³mico. */
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

  Saturator();

  void process(const ProcessArgs &args) override;
  void onSampleRateChange(const SampleRateChangeEvent &e) override;
  json_t *dataToJson() override;
  void dataFromJson(json_t *rootJ) override;

  /** Called from UI/JSON threads: only records the request. */
  void setOversampling(int factor);
  /** Llamado desde UI o JSON: valida 0 (LEGACY) / 1 (LR4) y deja la peticion. */
  void setCrossover(int mode);
  /** Llamado desde UI o JSON: valida y publica la curva de una banda. */
  void setCurve(int band, int id);
  /** Llamado desde UI o JSON: valida -1 (Off) hasta 2 (High) y publica el solo. */
  void setSolo(int band);
  /** Called from the audio thread: resets state and rebuilds coefficients. */
  void applyOversampling(int factor);
  /** Llamado desde el hilo de audio: resetea filtros y rehace coeficientes. */
  void applyCrossover(int mode);

  /** Llamado desde UI o JSON: valida 0/1 y deja la peticion. */
  void setAdaa(int on);
  /** Llamado desde el hilo de audio: fija `adaa` y resetea el historial. */
  void applyAdaa(int on);

  /** Llamado desde UI o JSON: valida 0/1 y publica el selector. */
  void setAutoMakeup(int on);

  void onReset() override;
};

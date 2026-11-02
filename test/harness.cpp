// Arnes offline de Multiband Saturator (v2.6.0).
// Especificacion: specs/v2.6_test_harness_spec.md
// No incluye <rack.hpp> ni enlaza -lRack: solo headers del SDK
// (dsp/resampler.hpp, dsp/filter.hpp) y src/Curves.hpp (independiente de Rack).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "metrics.hpp"
#include "offline_chain.hpp"

// ---------------------------------------------------------------------------
// Shim de I/O. El SDK de Rack declara `fopen_u8` y lo inyecta en namespace std
// (`using ::fopen_u8;`), pero su definicion vive en libRack. El arnes no enlaza
// -lRack (specs/v2.6_test_harness_spec.md 5.1), asi que aportamos el simbolo.
// Los nombres de fichero del arnes son ASCII ("baseline/..."), por lo que
// fopen_s basta y evita <windows.h> (que arrastraria las macros min/max).
// ---------------------------------------------------------------------------
extern "C" FILE *fopen_u8(const char *filename, const char *mode) {
  FILE *f = NULL;
  return (fopen_s(&f, filename, mode) == 0) ? f : NULL;
}

namespace {

const float PI = 3.14159265358979f;
const float FS = 48000.f;
const float VPK = 5.f;
const int BLOCK = 96000;
const int NFFT = 16384;
const int DISCARD = 4096;

struct Outcome {
  const char *id;
  const char *magnitude;
  double value;
  double limit;
  const char *unit;
  bool pass;
};

struct Expect {
  std::string id;
  std::string status;
};

struct TrimEntry {
  int curve;
  int driveIdx;
  double rmsDb;
};

struct AliasEntry {
  int curve;
  int n;
  double aliasDb;
};

std::string trim(const std::string &s) {
  const char *ws = " \t\r\n";
  const std::size_t b = s.find_first_not_of(ws);
  if (b == std::string::npos) return std::string();
  const std::size_t e = s.find_last_not_of(ws);
  return s.substr(b, e - b + 1);
}

bool fileExists(const char *path) {
  FILE *f = std::fopen(path, "r");
  if (!f) return false;
  std::fclose(f);
  return true;
}

std::vector<Expect> loadExpectations(const char *path) {
  std::vector<Expect> v;
  FILE *f = std::fopen(path, "r");
  if (!f) return v;
  char line[512];
  while (std::fgets(line, sizeof line, f)) {
    std::string s(line);
    if (s.empty() || s[0] == '#') continue;
    const std::size_t bar = s.find('|');
    if (bar == std::string::npos) continue;
    Expect e;
    e.id = trim(s.substr(0, bar));
    const std::size_t st = s.find("status=");
    if (st == std::string::npos) continue;
    e.status = trim(s.substr(st + 7));
    if (!e.id.empty()) v.push_back(e);
  }
  std::fclose(f);
  return v;
}

std::string expectedStatus(const std::vector<Expect> &ex, const char *id) {
  for (std::size_t i = 0; i < ex.size(); ++i)
    if (ex[i].id == id) return ex[i].status;
  return std::string("PASS");
}

bool loadTrim(const char *path, std::vector<TrimEntry> &v) {
  FILE *f = std::fopen(path, "r");
  if (!f) return false;
  char line[256];
  while (std::fgets(line, sizeof line, f)) {
    int c = 0, d = 0;
    double r = 0.0;
    if (std::sscanf(line, "curve=%d drive=%d rms=%lf", &c, &d, &r) == 3) {
      TrimEntry e;
      e.curve = c;
      e.driveIdx = d;
      e.rmsDb = r;
      v.push_back(e);
    }
  }
  std::fclose(f);
  return true;
}

bool loadAlias(const char *path, std::vector<AliasEntry> &v) {
  FILE *f = std::fopen(path, "r");
  if (!f) return false;
  char line[256];
  while (std::fgets(line, sizeof line, f)) {
    int c = 0, n = 0;
    double a = 0.0;
    if (std::sscanf(line, "curve=%d n=%d alias=%lf", &c, &n, &a) == 3) {
      AliasEntry e;
      e.curve = c;
      e.n = n;
      e.aliasDb = a;
      v.push_back(e);
    }
  }
  std::fclose(f);
  return true;
}

double findTrim(const std::vector<TrimEntry> &v, int curve, int driveIdx,
                double fallback) {
  for (std::size_t i = 0; i < v.size(); ++i)
    if (v[i].curve == curve && v[i].driveIdx == driveIdx) return v[i].rmsDb;
  return fallback;
}

double findAlias(const std::vector<AliasEntry> &v, int curve, int n,
                 double fallback) {
  for (std::size_t i = 0; i < v.size(); ++i)
    if (v[i].curve == curve && v[i].n == n) return v[i].aliasDb;
  return fallback;
}

std::vector<float> tailFrom(const std::vector<float> &v, std::size_t from) {
  if (from >= v.size()) return std::vector<float>();
  return std::vector<float>(v.begin() + from, v.end());
}

void configureCurves(cdt::OfflineChain &ch, int curve, float drive) {
  ch.p.curveLow = curve;
  ch.p.curveMid = curve;
  ch.p.curveHigh = curve;
  ch.p.driveLow = drive;
  ch.p.driveMid = drive;
  ch.p.driveHigh = drive;
}

void runSine(cdt::OfflineChain &ch, double freq, int n, std::vector<float> &in,
             std::vector<float> &out) {
  in.assign(n, 0.f);
  out.assign(n, 0.f);
  for (int i = 0; i < n; ++i) {
    // Generacion en double y conversion a float al inyectar (enmienda H3):
    // el ruido de cuantizacion de fase dependia de los flags de compilacion.
    const float x = static_cast<float>(
        static_cast<double>(VPK) *
        std::sin(6.283185307179586 * freq * static_cast<double>(i) /
                 static_cast<double>(FS)));
    in[i] = x;
    float oL = 0.f;
    float oR = 0.f;
    ch.process(x, x, oL, oR);
    out[i] = oL;
  }
}

// ---------------- T-RECON ----------------
Outcome caseRecon() {
  static const double F[7] = {20.0, 100.0, 500.0, 1000.0,
                              5000.0, 10000.0, 15000.0};
  // G_exp(f) = TRIM[CLEAN] * vol * |H_dc(f)| (enmienda H1, 2026-10-31): el
  // camino wet lleva un one-pole HP a 15 Hz, |H_dc(f)| = f / sqrt(f^2 + 15^2).
  const double trimVol =
      static_cast<double>(coaldynamics::TRIM[coaldynamics::CLEAN]) * 0.8;
  double worst = 0.0;
  for (int k = 0; k < 7; ++k) {
    cdt::OfflineChain ch;
    // T-RECON valida la reconstruccion del crossover LR4 (v2.6.0): con
    // mid = x - low - high la suma de bandas es x por construccion.
    ch.p.lr4 = 1;
    ch.configure(FS, 1);
    configureCurves(ch, coaldynamics::CLEAN, 1.f);
    ch.p.volLow = 0.8f;
    ch.p.volMid = 0.8f;
    ch.p.volHigh = 0.8f;
    ch.p.mix = 1.f;
    ch.p.outVol = 1.f;

    const double hdc = F[k] / std::sqrt(F[k] * F[k] + 225.0);
    const double expected = cdt::db20(trimVol * hdc);

    std::vector<float> in, out;
    runSine(ch, F[k], BLOCK, in, out);
    const std::size_t from = BLOCK / 4;
    const double g = cdt::db20(cdt::rms(tailFrom(out, from)) /
                               cdt::rms(tailFrom(in, from)));
    const double dev = g - expected;
    if (std::fabs(dev) > std::fabs(worst)) worst = dev;
  }
  Outcome o = {"T-RECON", "ganancia", worst, 0.1, "dB",
               std::fabs(worst) <= 0.1};
  return o;
}

// ---------------- T-LAT ----------------
template <class St>
int stagePeak() {
  St st;
  st.reset();
  std::vector<float> out;
  float upL[8];
  float upR[8];
  for (int n = 0; n < 256; ++n) {
    const float x = (n == 0) ? 1.f : 0.f;
    st.upsample(x, x, upL, upR);
    out.push_back(st.downsampleL(upL));
  }
  return static_cast<int>(cdt::peakIndex(out));
}

Outcome caseLat() {
  const int l1 = stagePeak<cdt::LocalBypassStage>();
  const int l2 = stagePeak<cdt::LocalStage<2> >();
  const int l4 = stagePeak<cdt::LocalStage<4> >();
  const int l8 = stagePeak<cdt::LocalStage<8> >();

  int dev = std::abs(l1 - 0);
  dev = std::max(dev, std::abs(l2 - 15));
  dev = std::max(dev, std::abs(l4 - 15));
  dev = std::max(dev, std::abs(l8 - 15));

  if (dev == 0) {
    Outcome o = {"T-LAT", "pico", static_cast<double>(l8), 15.0, "muestras",
                 true};
    return o;
  }
  Outcome o = {"T-LAT", "desvio", static_cast<double>(dev), 0.0, "muestras",
               false};
  return o;
}

// ---------------- T-TRIM ----------------
Outcome caseTrim() {
  std::vector<TrimEntry> base;
  if (!loadTrim("baseline/curves_trim_v2.5.0.txt", base)) {
    Outcome o = {"T-TRIM", "RMS", 999.0, 1.0, "dB", false};
    return o;
  }
  static const float KN[3] = {0.f, 0.5f, 1.f};
  double worst = 0.0;
  for (int c = 0; c < coaldynamics::CURVE_COUNT; ++c) {
    for (int d = 0; d < 3; ++d) {
      cdt::OfflineChain ch;
      ch.configure(FS, 1);
      configureCurves(ch, c, 1.f + KN[d] * 10.f);
      ch.p.mix = 1.f;
      ch.p.outVol = 1.f;

      std::vector<float> in, out;
      runSine(ch, 1000.0, 48000, in, out);
      const double db = cdt::db20(cdt::rms(tailFrom(out, 12000)));
      const double b = findTrim(base, c, d, db);
      const double delta = std::fabs(db - b);
      if (delta > worst) worst = delta;
    }
  }
  Outcome o = {"T-TRIM", "RMS", worst, 1.0, "dB", worst <= 1.0};
  return o;
}

// ---------------- T-ALIAS ----------------
double measureAlias(int curve, int n) {
  cdt::OfflineChain ch;
  ch.configure(FS, n);
  configureCurves(ch, curve, 11.f);
  ch.p.mix = 1.f;
  ch.p.outVol = 1.f;

  std::vector<float> in, out;
  runSine(ch, 5000.0, DISCARD + NFFT, in, out);
  std::vector<float> seg(out.begin() + DISCARD, out.end());
  const std::vector<double> mag = cdt::spectrum(seg, NFFT);
  return cdt::aliasWorstDb(mag, FS, NFFT, 5000.0, FS / 2.f, 4);
}

/** Copia de measureAlias() con ADAA1 activo (F2). El limite se compara contra
    el valor absoluto de specs/v2.6_quality_spec.md §7, no contra el baseline.

    Banda de medida: 20 kHz (no FS/2). Los limites de §7 (criterios 6-8) se
    derivan de la baseline de §4.2, declarada explicitamente como "peor alias
    in-band no armonico <20 kHz" (v2.6_quality_spec.md:68-70): con fMax=20 kHz
    la baseline ADAA-off de TANH 4x da -25.86 dB, que es el -25.9 dB de la
    spec (con fMax=FS/2 da -13.17 dB). Por encima de 20 kHz el suelo esta
    dominado por la fuga del 5o armonico (25 kHz) en la banda de transicion
    del decimador del LocalStage, que cae en 23 kHz: es energia que ADAA no
    puede corregir y que deja los tres limites inalcanzables. */
double measureAliasAdaa(int curve, int n) {
  cdt::OfflineChain ch;
  ch.configure(FS, n);
  configureCurves(ch, curve, 11.f);
  ch.p.adaa = 1;
  ch.p.mix = 1.f;
  ch.p.outVol = 1.f;

  std::vector<float> in, out;
  runSine(ch, 5000.0, DISCARD + NFFT, in, out);
  std::vector<float> seg(out.begin() + DISCARD, out.end());
  const std::vector<double> mag = cdt::spectrum(seg, NFFT);
  return cdt::aliasWorstDb(mag, FS, NFFT, 5000.0, 20000.0, 4);
}

Outcome caseAlias() {
  std::vector<AliasEntry> base;
  if (!loadAlias("baseline/alias_v2.5.0.txt", base)) {
    Outcome o = {"T-ALIAS", "alias", 999.0, 1.0, "dB", false};
    return o;
  }
  static const int NS[3] = {1, 4, 8};
  double worst = 0.0;
  // Enmienda H3: solo las 8 curvas no lineales. CLEAN es lineal y su "alias"
  // medido es ruido de cuantizacion de fase, no distorsion.
  for (int c = coaldynamics::TANH; c < coaldynamics::CURVE_COUNT; ++c) {
    for (int k = 0; k < 3; ++k) {
      const double a = measureAlias(c, NS[k]);
      const double b = findAlias(base, c, NS[k], a);
      const double degrade = a - b;
      if (degrade > worst) worst = degrade;
    }
  }
  Outcome o = {"T-ALIAS", "alias", worst, 1.0, "dB", worst <= 1.0};
  return o;
}

// ---------------- T-ADAA (F2) ----------------
/** Alias in-band con ADAA: FOLDBACK, SINEFOLD y TANH a 4x, 5 kHz, Vpk 5,
    drive 11 (specs/v2.6_quality_spec.md §7 criterios 6-8). */
Outcome caseAdaaAlias(const char *id, int curve, double limitDB) {
  const double a = measureAliasAdaa(curve, 4);
  Outcome o = {id, "alias", a, limitDB, "dB", a <= limitDB};
  return o;
}

/** THD+N de UNA banda a 1 kHz con ADAA on/off (n = 1). Se aisla la banda Mid
    (soloBand = 1) para medir una sola instancia del operador ADAA1: la suma de
    las tres bandas saturadas añade +0.21 dB no atribuibles al algoritmo
    (medido 2026-11-02, ver specs/v2.6_adaa_contract.md §8.3). */
double thdNAdaa(int curve, float drive, int adaaOn) {
  cdt::OfflineChain ch;
  ch.configure(FS, 1);
  configureCurves(ch, curve, drive);
  ch.p.soloBand = 1;
  ch.p.adaa = adaaOn;
  ch.p.mix = 1.f;
  ch.p.outVol = 1.f;

  std::vector<float> in, out;
  runSine(ch, 1000.0, DISCARD + NFFT, in, out);
  std::vector<float> seg(out.begin() + DISCARD, out.end());
  const std::vector<double> mag = cdt::spectrum(seg, NFFT);
  return cdt::thdPlusNDb(mag, FS, NFFT, 1000.0);
}

/** Criterio 5: TANH 1 kHz, drive 6, d(THD+N) <= +0.5 dB. */
Outcome caseAdaaThd() {
  const double off = thdNAdaa(coaldynamics::TANH, 6.f, 0);
  const double on = thdNAdaa(coaldynamics::TANH, 6.f, 1);
  const double delta = on - off;
  Outcome o = {"T-ADAA-THD", "dTHD+N", delta, 0.5, "dB", delta <= 0.5};
  return o;
}

/** Criterio 10 (T-ADAA-CLEAN): con CLEAN y lr4 = 1, ADAA on y off deben dar la
    misma salida (|d| < 1e-6) y no debe haber atenuacion (|G - G_exp| < 0.01 dB
    a 1 kHz y 15 kHz). 1x, vol 0.8, pan 0. */
Outcome caseAdaaClean() {
  static const double F[2] = {1000.0, 15000.0};
  const double trimVol =
      static_cast<double>(coaldynamics::TRIM[coaldynamics::CLEAN]) * 0.8;
  double maxDiff = 0.0;
  bool gainOk = true;

  for (int k = 0; k < 2; ++k) {
    // Un UNICO punto de llamada para las dos variantes (s = 0: ADAA off,
    // s = 1: ADAA on). Con -O3 -funsafe-math-optimizations GCC reasocia de
    // forma distinta dos instancias de OfflineChain inlinadas por separado
    // (divergencia medida: 2.98e-6 con dos objetos, 0 con un solo call site),
    // lo que mediria al compilador y no la transparencia de CLEAN.
    cdt::OfflineChain ch[2];
    std::vector<float> in[2], out[2];
    for (int s = 0; s < 2; ++s) {
      ch[s].p.lr4 = 1;
      ch[s].configure(FS, 1);
      configureCurves(ch[s], coaldynamics::CLEAN, 1.f);
      ch[s].p.volLow = 0.8f;
      ch[s].p.volMid = 0.8f;
      ch[s].p.volHigh = 0.8f;
      ch[s].p.mix = 1.f;
      ch[s].p.outVol = 1.f;
      ch[s].p.adaa = s;
    }

    for (int s = 0; s < 2; ++s) runSine(ch[s], F[k], BLOCK, in[s], out[s]);

    const std::size_t n = std::min(out[0].size(), out[1].size());
    for (std::size_t i = 0; i < n; ++i) {
      const double d = std::fabs(static_cast<double>(out[0][i]) -
                                 static_cast<double>(out[1][i]));
      if (d > maxDiff) maxDiff = d;
    }

    const double hdc = F[k] / std::sqrt(F[k] * F[k] + 225.0);
    const double expected = cdt::db20(trimVol * hdc);
    const std::size_t from = BLOCK / 4;
    const double g = cdt::db20(cdt::rms(tailFrom(out[1], from)) /
                               cdt::rms(tailFrom(in[1], from)));
    if (std::fabs(g - expected) >= 0.01) gainOk = false;
  }

  Outcome o = {"T-ADAA-CLEAN", "delta", maxDiff, 1e-6, "unidades",
               maxDiff < 1e-6 && gainOk};
  return o;
}

// ---------------- T-DC ----------------
Outcome caseDc() {
  static const int CV[2] = {coaldynamics::TUBE, coaldynamics::DIODE};
  double worst = -300.0;
  for (int k = 0; k < 2; ++k) {
    cdt::OfflineChain ch;
    ch.configure(FS, 1);
    configureCurves(ch, CV[k], 11.f);
    ch.p.mix = 1.f;
    ch.p.outVol = 1.f;

    std::vector<float> out;
    for (int n = 0; n < 96000; ++n) {
      float oL = 0.f;
      float oR = 0.f;
      ch.process(1.f, 1.f, oL, oR);
      out.push_back(oL);
    }
    const std::size_t from = out.size() - 16384;
    double sum = 0.0;
    for (std::size_t i = from; i < out.size(); ++i) sum += out[i];
    const double mean = sum / 16384.0;
    const double db = cdt::db20(std::fabs(mean));
    if (db > worst) worst = db;
  }
  Outcome o = {"T-DC", "DC", worst, -60.0, "dBFS", worst <= -60.0};
  return o;
}

// ---------------- T-NAN ----------------
Outcome caseNan() {
  static const float VS[5] = {1e-6f, 1e-3f, 1.f, 100.f, 1000.f};
  static const float DR[2] = {1.f, 11.f};
  int bad = 0;
  for (int c = 0; c < coaldynamics::CURVE_COUNT; ++c) {
    for (int d = 0; d < 2; ++d) {
      for (int v = 0; v < 5; ++v) {
        cdt::OfflineChain ch;
        ch.configure(FS, 1);
        configureCurves(ch, c, DR[d]);
        ch.p.mix = 1.f;
        ch.p.outVol = 1.f;

        for (int n = 0; n < 2048; ++n) {
          float oL = 0.f;
          float oR = 0.f;
          ch.process(VS[v], VS[v], oL, oR);
          if (!std::isfinite(oL) || !std::isfinite(oR)) {
            ++bad;
            break;
          }
        }
      }
    }
  }
  Outcome o = {"T-NAN", "naninf", static_cast<double>(bad), 0.0, "unidades",
               bad == 0};
  return o;
}

// ---------------- T-MIX ----------------
Outcome caseMix() {
  cdt::OfflineChain ch;
  ch.configure(FS, 1);
  ch.p.mix = 0.f;
  ch.p.outVol = 1.f;

  double worst = 0.0;
  for (int n = 0; n < 48000; ++n) {
    const float x = 0.5f * std::sin(2.f * PI * 440.f * n / FS);
    float oL = 0.f;
    float oR = 0.f;
    ch.process(x, x, oL, oR);
    const double d = std::fabs(static_cast<double>(oL) - x);
    if (d > worst) worst = d;
  }
  Outcome o = {"T-MIX", "diff", worst, 0.0, "unidades", worst == 0.0};
  return o;
}

// ---------------- freeze ----------------
int freezeBaselines() {
  static const float KN[3] = {0.f, 0.5f, 1.f};
  FILE *ft = std::fopen("baseline/curves_trim_v2.5.0.txt", "w");
  if (!ft) {
    std::fprintf(stderr, "ERROR: no puedo escribir baseline/curves_trim_v2.5.0.txt\n");
    return 1;
  }
  std::fprintf(ft, "# Curvas TRIM: RMS de salida (dBFS rel. 1 V) | seno 1 kHz, Vpk=5 V, MIX=1, vol=1\n");
  std::fprintf(ft, "# congelado desde v2.5.0 por `harness --freeze`\n");
  for (int c = 0; c < coaldynamics::CURVE_COUNT; ++c) {
    for (int d = 0; d < 3; ++d) {
      cdt::OfflineChain ch;
      ch.configure(FS, 1);
      configureCurves(ch, c, 1.f + KN[d] * 10.f);
      ch.p.mix = 1.f;
      ch.p.outVol = 1.f;
      std::vector<float> in, out;
      runSine(ch, 1000.0, 48000, in, out);
      const double db = cdt::db20(cdt::rms(tailFrom(out, 12000)));
      std::fprintf(ft, "curve=%d drive=%d rms=%.6f\n", c, d, db);
    }
  }
  std::fclose(ft);

  FILE *fa = std::fopen("baseline/alias_v2.5.0.txt", "w");
  if (!fa) {
    std::fprintf(stderr, "ERROR: no puedo escribir baseline/alias_v2.5.0.txt\n");
    return 1;
  }
  std::fprintf(fa, "# Alias in-band no armonico (dB rel. fundamental) | seno 5 kHz, Vpk=5, drive=100%%, MIX=1\n");
  std::fprintf(fa, "# congelado desde v2.5.0 por `harness --freeze`\n");
  static const int NS[3] = {1, 4, 8};
  // Enmienda H3: el baseline de alias cubre 8 curvas (CLEAN excluida).
  for (int c = coaldynamics::TANH; c < coaldynamics::CURVE_COUNT; ++c) {
    for (int k = 0; k < 3; ++k) {
      const double a = measureAlias(c, NS[k]);
      std::fprintf(fa, "curve=%d n=%d alias=%.6f\n", c, NS[k], a);
    }
  }
  std::fclose(fa);
  std::printf("Baselines congelados en baseline/\n");
  return 0;
}

void printOutcome(const Outcome &o, const char *res) {
  std::printf("%s %s %s %.4f limit %.4f unit %s\n", res, o.id, o.magnitude,
              o.value, o.limit, o.unit);
}

}  // namespace

int main(int argc, char **argv) {
  bool freeze = false;
  std::string only;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--freeze") == 0) {
      freeze = true;
    } else if (std::strcmp(argv[i], "--case") == 0 && i + 1 < argc) {
      only = argv[++i];
    }
  }

  if (freeze) return freezeBaselines();

  const char *EXP = "baseline/expectations.txt";
  if (!fileExists(EXP)) {
    std::fprintf(stderr, "ERROR: falta %s (baseline de expectativas)\n", EXP);
    return 1;
  }
  const std::vector<Expect> ex = loadExpectations(EXP);

  const Outcome all[12] = {
      caseRecon(),       caseLat(),      caseTrim(),
      caseAlias(),       caseDc(),       caseNan(),
      caseMix(),         caseAdaaAlias("T-ADAA-FOLD", coaldynamics::FOLDBACK,
                                       -14.2),
      caseAdaaAlias("T-ADAA-SINE", coaldynamics::SINEFOLD, -12.4),
      caseAdaaAlias("T-ADAA-TANH", coaldynamics::TANH, -31.9),
      caseAdaaThd(),     caseAdaaClean()};

  int casos = 0;
  int p = 0;
  int x = 0;
  int f = 0;
  int exitCode = 0;

  for (int i = 0; i < 12; ++i) {
    const Outcome &o = all[i];
    if (!only.empty() && only != o.id) continue;
    ++casos;

    const std::string want = expectedStatus(ex, o.id);
    const bool expectedFail = (want == "XFAIL" || want == "FAIL");
    const char *res = "FAIL";
    if (o.pass && !expectedFail) {
      res = "PASS";
      ++p;
    } else if (!o.pass && expectedFail) {
      res = "XFAIL";
      ++x;
    } else {
      res = "FAIL";
      ++f;
      exitCode = 1;
    }
    printOutcome(o, res);
  }

  std::printf("RESUMEN casos=%d PASS=%d XFAIL=%d FAIL=%d\n", casos, p, x, f);
  return exitCode;
}

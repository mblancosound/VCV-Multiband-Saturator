#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

namespace cdt {

// Constante local para no depender de M_PI (no portable / _USE_MATH_DEFINES).
constexpr double pi = 3.14159265358979323846;

// 20*log10(|x|) con suelo en 1e-12 para evitar -inf.
inline double db20(double x) {
  const double a = std::fabs(x);
  return 20.0 * std::log10(std::max(a, 1e-12));
}

// Valor eficaz (RMS) de la senal.
inline double rms(const std::vector<float> &x) {
  if (x.empty()) return 0.0;
  double acc = 0.0;
  for (std::size_t n = 0; n < x.size(); ++n) {
    const double v = static_cast<double>(x[n]);
    acc += v * v;
  }
  return std::sqrt(acc / static_cast<double>(x.size()));
}

// Mayor valor absoluto de la senal.
inline double peakAbs(const std::vector<float> &x) {
  double p = 0.0;
  for (std::size_t n = 0; n < x.size(); ++n) {
    const double a = std::fabs(static_cast<double>(x[n]));
    if (a > p) p = a;
  }
  return p;
}

// Indice del primer maximo de |x| (0 si el vector esta vacio).
inline std::size_t peakIndex(const std::vector<float> &x) {
  std::size_t idx = 0;
  double p = -std::numeric_limits<double>::infinity();
  for (std::size_t n = 0; n < x.size(); ++n) {
    const double a = std::fabs(static_cast<double>(x[n]));
    if (a > p) {
      p = a;
      idx = n;
    }
  }
  return idx;
}

// Ventana Blackman-Harris de 4 terminos, normalizada para que sum(w) == 1.
// n <= 1 devuelve una unica muestra unidad.
inline std::vector<double> blackmanHarris(std::size_t n) {
  std::vector<double> w;
  if (n <= 1) {
    w.push_back(1.0);
    return w;
  }
  const double a0 = 0.35875;
  const double a1 = 0.48829;
  const double a2 = 0.14128;
  const double a3 = 0.01168;
  const double den = static_cast<double>(n - 1);
  w.resize(n);
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = 2.0 * pi * static_cast<double>(i) / den;
    const double v = a0 - a1 * std::cos(t) + a2 * std::cos(2.0 * t) -
                     a3 * std::cos(3.0 * t);
    w[i] = v;
    sum += v;
  }
  if (sum != 0.0) {
    for (std::size_t i = 0; i < n; ++i) w[i] /= sum;
  }
  return w;
}

// Espectro de magnitud (DFT real, sin normalizar) de x con ventana
// Blackman-Harris y nfft puntos. Devuelve nfft/2+1 bins.
//
// Convencion: una sinusoide de amplitud A da pico ~ A/2 en su bin; el factor 2
// no se aplica. Las tablas de cos/sin se calculan una sola vez; el doble bucle
// solo indexa (n*k) % nfft (prohibido llamar a cos/sin ahi dentro).
inline std::vector<double> spectrum(const std::vector<float> &x,
                                    std::size_t nfft) {
  std::vector<double> mag;
  if (nfft < 2 || x.empty()) return mag;

  const std::vector<double> win = blackmanHarris(nfft);

  std::vector<double> cosTab(nfft);
  std::vector<double> sinTab(nfft);
  for (std::size_t n = 0; n < nfft; ++n) {
    const double t = 2.0 * pi * static_cast<double>(n) /
                     static_cast<double>(nfft);
    cosTab[n] = std::cos(t);
    sinTab[n] = std::sin(t);
  }

  const std::size_t nbins = nfft / 2 + 1;
  mag.resize(nbins);
  for (std::size_t k = 0; k < nbins; ++k) {
    double re = 0.0;
    double im = 0.0;
    for (std::size_t n = 0; n < nfft; ++n) {
      const double xn = static_cast<double>(x[n]) * win[n];
      const std::size_t idx = (n * k) % nfft;
      re += xn * cosTab[idx];
      im -= xn * sinTab[idx];
    }
    mag[k] = std::sqrt(re * re + im * im);
  }
  return mag;
}

// Frecuencia central del bin k en Hz.
inline double binHz(double fs, std::size_t nfft, std::size_t k) {
  if (nfft == 0) return 0.0;
  return static_cast<double>(k) * fs / static_cast<double>(nfft);
}

// Peor componente no armonica (respecto al fundamental) en dB.
// Implementacion literal del pseudocodigo del arnes: el bucle exterior itera
// bins (k), no frecuencias.
inline double aliasWorstDb(const std::vector<double> &mag, double fs,
                           std::size_t nfft, double f0, double fMax,
                           int maxHarm) {
  if (mag.empty() || nfft == 0) return -300.0;

  const double bin = fs / static_cast<double>(nfft);
  if (bin <= 0.0) return -300.0;

  const long k0 = static_cast<long>(std::floor(f0 / bin + 0.5));
  const double f0b = static_cast<double>(k0) * bin;

  const long lo = std::max<long>(k0 - 4, 0L);
  const long hi = std::min<long>(k0 + 4, static_cast<long>(mag.size()) - 1);

  double fund = 0.0;
  for (long k = lo; k <= hi; ++k) {
    const double v = mag[static_cast<std::size_t>(k)];
    if (v > fund) fund = v;
  }
  if (fund <= 0.0) return -300.0;

  double worst = 0.0;
  for (std::size_t k = 1; k < mag.size(); ++k) {
    const double f = static_cast<double>(k) * bin;
    if (f < 20.0 || f >= fMax) continue;
    bool skip = false;
    for (int h = 1; h <= maxHarm; ++h) {
      if (std::fabs(f - static_cast<double>(h) * f0b) <= 4.0 * bin) {
        skip = true;
        break;
      }
    }
    if (skip) continue;
    if (mag[k] > worst) worst = mag[k];
  }
  if (worst <= 0.0) return -300.0;

  return 20.0 * std::log10(worst / fund);
}

}  // namespace cdt

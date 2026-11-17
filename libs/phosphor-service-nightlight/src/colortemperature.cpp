// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorServiceNightLight/ColorTemperature.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace PhosphorServiceNightLight {

namespace {

// Second radiation constant h·c/k, in metre-kelvin (CODATA 2018 exact values).
constexpr double kSecondRadiationConstant = 6.62607015e-34 * 2.99792458e8 / 1.380649e-23;

// The display encoding the ramp factors are expressed in.
constexpr double kEncodingGamma = 2.2;

// A piecewise Gaussian: sigma1 left of the mean, sigma2 right of it.
double lobe(double wavelength, double mean, double sigma1, double sigma2)
{
    const double t = (wavelength - mean) / (wavelength < mean ? sigma1 : sigma2);
    return std::exp(-0.5 * t * t);
}

struct Tristimulus
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// The CIE 1931 colour matching functions, Wyman, Sloan and Shirley's
// multi-lobe fit (JCGT 2(2), 2013, equation 2). @p nm is the wavelength in
// nanometres.
Tristimulus matching(double nm)
{
    return {
        1.056 * lobe(nm, 599.8, 37.9, 31.0) + 0.362 * lobe(nm, 442.0, 16.0, 26.7) - 0.065 * lobe(nm, 501.1, 20.4, 26.2),
        0.821 * lobe(nm, 568.8, 46.9, 40.5) + 0.286 * lobe(nm, 530.9, 16.3, 31.1),
        1.217 * lobe(nm, 437.0, 11.8, 36.0) + 0.681 * lobe(nm, 459.0, 26.0, 13.8),
    };
}

// The black body's XYZ at @p kelvin, up to a common scale (Planck's law with
// its constant factor dropped), summed over the visible range in 1 nm steps.
Tristimulus blackBody(double kelvin)
{
    Tristimulus sum;
    for (int nm = 380; nm <= 780; ++nm) {
        const double metres = nm * 1e-9;
        const double radiance = 1.0 / (std::pow(metres, 5) * std::expm1(kSecondRadiationConstant / (metres * kelvin)));
        const Tristimulus cmf = matching(nm);
        sum.x += radiance * cmf.x;
        sum.y += radiance * cmf.y;
        sum.z += radiance * cmf.z;
    }
    return sum;
}

// XYZ to linear sRGB, IEC 61966-2-1.
std::array<double, 3> linearSrgb(const Tristimulus& c)
{
    return {
        3.2406 * c.x - 1.5372 * c.y - 0.4986 * c.z,
        -0.9689 * c.x + 1.8758 * c.y + 0.0415 * c.z,
        0.0557 * c.x - 0.2040 * c.y + 1.0570 * c.z,
    };
}

const std::array<double, 3>& neutralSrgb()
{
    static const std::array<double, 3> neutral = linearSrgb(blackBody(ColorTemperature::Neutral));
    return neutral;
}

} // namespace

WhitePoint whitePoint(int kelvin)
{
    kelvin = ColorTemperature::bound(kelvin);
    if (kelvin == ColorTemperature::Neutral) {
        return {};
    }
    const auto rgb = linearSrgb(blackBody(kelvin));
    const auto& neutral = neutralSrgb();
    std::array<double, 3> relative;
    for (std::size_t i = 0; i < relative.size(); ++i) {
        // Below about 1900 K blue falls outside the sRGB gamut; it is dark, not negative.
        relative[i] = std::max(0.0, rgb[i] / neutral[i]);
    }
    const double peak = *std::max_element(relative.cbegin(), relative.cend());
    const auto encode = [peak](double value) {
        return std::pow(value / peak, 1.0 / kEncodingGamma);
    };
    return {encode(relative[0]), encode(relative[1]), encode(relative[2])};
}

QByteArray gammaTable(const WhitePoint& white, quint32 size)
{
    if (size < 2) {
        return {};
    }
    QByteArray table(qsizetype(size) * 3 * qsizetype(sizeof(quint16)), Qt::Uninitialized);
    auto* entries = reinterpret_cast<quint16*>(table.data());
    const double channels[] = {white.red, white.green, white.blue};
    for (int channel = 0; channel < 3; ++channel) {
        const double factor = std::clamp(channels[channel], 0.0, 1.0);
        quint16* ramp = entries + qsizetype(channel) * size;
        for (quint32 i = 0; i < size; ++i) {
            const double value = double(i) / double(size - 1) * 65535.0 * factor;
            ramp[i] = quint16(std::lround(value));
        }
    }
    return table;
}

} // namespace PhosphorServiceNightLight

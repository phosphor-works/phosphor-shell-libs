// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorServiceNightLight/phosphorservicenightlight_export.h>

#include <QByteArray>
#include <QtGlobal>

namespace PhosphorServiceNightLight {

/// The colour temperatures the library works in, in kelvin.
namespace ColorTemperature {

/// The warmest temperature the library sets.
inline constexpr int Minimum = 1000;
/// The coolest temperature the library sets.
inline constexpr int Maximum = 10000;
/// The temperature that leaves the screen unchanged (an identity ramp).
inline constexpr int Neutral = 6500;

/// Clamp @p kelvin to [Minimum, Maximum].
[[nodiscard]] constexpr int bound(int kelvin)
{
    return qBound(Minimum, kelvin, Maximum);
}

} // namespace ColorTemperature

/**
 * @brief The colour of white at a temperature, as per-channel factors.
 *
 * Each factor is in [0, 1] and the largest is 1, so the ramp only ever dims a
 * channel. The factors are gamma-encoded (sRGB-like, exponent 2.2), ready to
 * multiply an encoded ramp value: scaling an encoded value by `w^(1/2.2)` scales
 * the light it produces by `w`.
 */
struct WhitePoint
{
    double red = 1.0;
    double green = 1.0;
    double blue = 1.0;

    bool operator==(const WhitePoint&) const = default;
};

/**
 * @brief The white point of a black body at @p kelvin, relative to 6500 K.
 *
 * The black body's chromaticity comes from Planck's law integrated over the
 * CIE 1931 2° standard observer, the observer approximated by the multi-lobe
 * Gaussian fit of Wyman, Sloan and Shirley ("Simple Analytic Approximations to
 * the CIE XYZ Color Matching Functions", JCGT 2(2), 2013). XYZ goes to linear
 * sRGB through the IEC 61966-2-1 matrix. The result is divided by the same
 * computation at 6500 K, so 6500 K is exactly white and the scale is continuous
 * on both sides of it, then normalised so the largest channel is 1.
 *
 * @p kelvin is clamped to [ColorTemperature::Minimum, ColorTemperature::Maximum].
 */
[[nodiscard]] PHOSPHORSERVICENIGHTLIGHT_EXPORT WhitePoint whitePoint(int kelvin);

/**
 * @brief A wlr-gamma-control table for @p white with @p size entries per channel.
 *
 * The layout the protocol's set_gamma expects: @p size native-endian 16-bit red
 * values, then green, then blue (`6 * size` bytes). Entry `i` is
 * `i / (size - 1) * 65535 * channel`, the identity ramp scaled by the white
 * point. A @p size below 2 gives an empty array.
 */
[[nodiscard]] PHOSPHORSERVICENIGHTLIGHT_EXPORT QByteArray gammaTable(const WhitePoint& white, quint32 size);

} // namespace PhosphorServiceNightLight

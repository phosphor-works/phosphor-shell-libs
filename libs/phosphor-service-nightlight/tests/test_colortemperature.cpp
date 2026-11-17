// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The colour-temperature math: the black-body white point and the gamma table
// built from it.

#include <PhosphorServiceNightLight/ColorTemperature.h>

#include <QTest>

#include <cmath>

using namespace PhosphorServiceNightLight;

namespace {

bool near(double a, double b, double tolerance)
{
    return std::abs(a - b) <= tolerance;
}

quint16 entry(const QByteArray& table, quint32 size, int channel, quint32 index)
{
    const auto* entries = reinterpret_cast<const quint16*>(table.constData());
    return entries[qsizetype(channel) * size + index];
}

} // namespace

class ColorTemperatureTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void neutralIsWhite();
    void warmKeepsRedAndDimsBlueMost();
    void warmerIsMonotonic();
    void continuousAroundNeutral();
    void coolerThanNeutralDimsRed();
    void clampsOutOfRange();
    void referenceValues_data();
    void referenceValues();
    void tableLayout();
    void tableTooSmallIsEmpty();
};

void ColorTemperatureTest::neutralIsWhite()
{
    QCOMPARE(whitePoint(ColorTemperature::Neutral), WhitePoint{});
    const QByteArray table = gammaTable(whitePoint(ColorTemperature::Neutral), 256);
    for (int channel = 0; channel < 3; ++channel) {
        for (quint32 i = 0; i < 256; ++i) {
            QCOMPARE(entry(table, 256, channel, i), quint16(i * 257)); // 65535 / 255
        }
    }
}

void ColorTemperatureTest::warmKeepsRedAndDimsBlueMost()
{
    for (int kelvin = ColorTemperature::Minimum; kelvin < ColorTemperature::Neutral; kelvin += 100) {
        const WhitePoint white = whitePoint(kelvin);
        QCOMPARE(white.red, 1.0);
        QVERIFY2(white.green < 1.0 && white.blue < white.green, qPrintable(QString::number(kelvin)));
    }
    // Below about 1900 K blue is outside the sRGB gamut: clamped dark.
    QCOMPARE(whitePoint(ColorTemperature::Minimum).blue, 0.0);
}

void ColorTemperatureTest::warmerIsMonotonic()
{
    WhitePoint previous = whitePoint(ColorTemperature::Minimum);
    for (int kelvin = ColorTemperature::Minimum + 50; kelvin <= ColorTemperature::Neutral; kelvin += 50) {
        const WhitePoint white = whitePoint(kelvin);
        QVERIFY2(white.green > previous.green, qPrintable(QString::number(kelvin)));
        QVERIFY2(white.blue >= previous.blue, qPrintable(QString::number(kelvin)));
        previous = white;
    }
}

void ColorTemperatureTest::continuousAroundNeutral()
{
    const WhitePoint below = whitePoint(ColorTemperature::Neutral - 1);
    const WhitePoint above = whitePoint(ColorTemperature::Neutral + 1);
    for (const WhitePoint& white : {below, above}) {
        QVERIFY(near(white.red, 1.0, 0.001));
        QVERIFY(near(white.green, 1.0, 0.001));
        QVERIFY(near(white.blue, 1.0, 0.001));
    }
}

void ColorTemperatureTest::coolerThanNeutralDimsRed()
{
    const WhitePoint white = whitePoint(ColorTemperature::Maximum);
    QCOMPARE(white.blue, 1.0);
    QVERIFY(white.red < white.green);
    QVERIFY(white.green < 1.0);
}

void ColorTemperatureTest::clampsOutOfRange()
{
    QCOMPARE(whitePoint(0), whitePoint(ColorTemperature::Minimum));
    QCOMPARE(whitePoint(-5), whitePoint(ColorTemperature::Minimum));
    QCOMPARE(whitePoint(40000), whitePoint(ColorTemperature::Maximum));
    QCOMPARE(ColorTemperature::bound(999), ColorTemperature::Minimum);
    QCOMPARE(ColorTemperature::bound(4500), 4500);
}

void ColorTemperatureTest::referenceValues_data()
{
    QTest::addColumn<int>("kelvin");
    QTest::addColumn<double>("green");
    QTest::addColumn<double>("blue");
    // From a separate Python transcription of the same formulas, pinned so a
    // change to the math is noticed. Its chromaticities agree with the CIE's
    // published values (2856 K, illuminant A: x 0.446, y 0.409 against 0.448,
    // 0.407; 6500 K: x 0.3135, y 0.3238).
    QTest::newRow("3000 K") << 3000 << 0.7380 << 0.4294;
    QTest::newRow("4500 K") << 4500 << 0.8904 << 0.7345;
    QTest::newRow("5500 K") << 5500 << 0.9541 << 0.8830;
}

void ColorTemperatureTest::referenceValues()
{
    QFETCH(int, kelvin);
    QFETCH(double, green);
    QFETCH(double, blue);
    const WhitePoint white = whitePoint(kelvin);
    QCOMPARE(white.red, 1.0);
    QVERIFY2(near(white.green, green, 0.002), qPrintable(QString::number(white.green)));
    QVERIFY2(near(white.blue, blue, 0.002), qPrintable(QString::number(white.blue)));
}

void ColorTemperatureTest::tableLayout()
{
    const WhitePoint white{1.0, 0.5, 0.25};
    constexpr quint32 size = 1024;
    const QByteArray table = gammaTable(white, size);
    QCOMPARE(table.size(), qsizetype(size * 3 * 2));
    for (int channel = 0; channel < 3; ++channel) {
        QCOMPARE(entry(table, size, channel, 0), quint16(0));
        for (quint32 i = 1; i < size; ++i) {
            QVERIFY(entry(table, size, channel, i) >= entry(table, size, channel, i - 1));
        }
    }
    QCOMPARE(entry(table, size, 0, size - 1), quint16(65535));
    QCOMPARE(entry(table, size, 1, size - 1), quint16(32768)); // round(65535 * 0.5)
    QCOMPARE(entry(table, size, 2, size - 1), quint16(16384)); // round(65535 * 0.25)
}

void ColorTemperatureTest::tableTooSmallIsEmpty()
{
    QVERIFY(gammaTable(WhitePoint{}, 0).isEmpty());
    QVERIFY(gammaTable(WhitePoint{}, 1).isEmpty());
    QCOMPARE(gammaTable(WhitePoint{}, 2).size(), qsizetype(12));
}

QTEST_GUILESS_MAIN(ColorTemperatureTest)
#include "test_colortemperature.moc"

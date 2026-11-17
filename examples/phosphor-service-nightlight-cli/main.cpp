// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: GPL-3.0-or-later
//
// phosphor-service-nightlight-cli: acceptance harness + worked example for the
// phosphor-service-nightlight library.
//
//   phosphor-service-nightlight-cli [kelvin] [seconds]
//
// Enables a constant night light at <kelvin> (default 3500) on every output of
// the running Wayland compositor, holds it for <seconds> (default 5), then
// disables it, waits for the fade back to neutral and exits; the compositor
// restores the original ramps when the gamma controls are released. Prints what
// the library reports along the way. Exits 1 when the compositor offers no
// usable gamma control (KWin, or another program holding the outputs).
//
// Run it inside the compositor to test, never against a session whose screen
// you depend on: it does change the screen's colour for those seconds.

#include <PhosphorServiceNightLight/ColorTemperature.h>
#include <PhosphorServiceNightLight/NightLight.h>

#include <QGuiApplication>
#include <QTextStream>
#include <QTimer>

using PhosphorServiceNightLight::NightLight;
namespace ColorTemperature = PhosphorServiceNightLight::ColorTemperature;

namespace {

constexpr int kOk = 0;
constexpr int kUnavailable = 1;
constexpr int kUsage = 64;

// Long enough for the compositor's globals and the first gamma_size to arrive.
constexpr int kSettleMs = 500;

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments().mid(1);

    bool ok = true;
    const int kelvin = args.size() > 0 ? args.at(0).toInt(&ok) : 3500;
    bool secondsOk = true;
    const int seconds = args.size() > 1 ? args.at(1).toInt(&secondsOk) : 5;
    if (!ok || !secondsOk || args.size() > 2 || seconds < 0) {
        out() << "usage: phosphor-service-nightlight-cli [kelvin] [seconds]" << Qt::endl;
        return kUsage;
    }

    NightLight light;
    QObject::connect(&light, &NightLight::availableChanged, &light, [&light] {
        out() << "available: " << (light.isAvailable() ? "yes" : "no") << Qt::endl;
    });
    QObject::connect(&light, &NightLight::currentTemperatureChanged, &light, [&light] {
        if (light.currentTemperature() == light.targetTemperature()) {
            out() << "on screen: " << light.currentTemperature() << " K" << Qt::endl;
        }
    });

    light.setMode(NightLight::Mode::Constant);
    light.setNightTemperature(kelvin);
    out() << "night temperature: " << light.nightTemperature() << " K, holding for " << seconds << " s" << Qt::endl;
    light.setEnabled(true);

    QTimer::singleShot(kSettleMs, &app, [&] {
        if (!light.isAvailable()) {
            out() << "no usable gamma control on this compositor" << Qt::endl;
            app.exit(kUnavailable);
            return;
        }
        QTimer::singleShot(seconds * 1000, &app, [&] {
            out() << "disabling" << Qt::endl;
            QObject::connect(&light, &NightLight::currentTemperatureChanged, &app, [&] {
                if (light.currentTemperature() == ColorTemperature::Neutral) {
                    app.exit(kOk);
                }
            });
            light.setEnabled(false);
        });
    });

    return app.exec();
}

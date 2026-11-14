// cadence — track composition sessions and the ideas waiting between them.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStringList>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Everything except "open" (and no arguments at all) is a headless
    // command, so an agent can log a session or read the log without a display.
    const bool headless = !arguments.isEmpty() && arguments.first() != "open";
    if (headless)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("cadence");
    app.setApplicationVersion("0.1.0");

    if (headless) {
        Backend backend;
        Cli cli(backend);
        return cli.run(arguments);
    }

    // Matches the window to cadence.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("cadence");
    app.setWindowIcon(QIcon::fromTheme("cadence"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

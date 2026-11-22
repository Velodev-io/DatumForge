#include "MainWindow.hpp"
#include "protocol/Client.hpp"
#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setOrganizationName("local.solidworks-clone.dev");
    app.setApplicationName("Solidworks Clone");
    QString settingsFile;
    QString recoveryRoot;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--recovery-root") && i + 1 < argc) {
            recoveryRoot = QString::fromLocal8Bit(argv[++i]);
        } else if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--settings") &&
                   i + 1 < argc) {
            settingsFile = QString::fromLocal8Bit(argv[++i]);
        }
    }
    cad::MainWindow window(settingsFile, nullptr, recoveryRoot);
    window.show();
    if (app.arguments().contains(QStringLiteral("--benchmark-launch")) ||
        app.arguments().contains(QStringLiteral("--benchmark-settle"))) {
        static bool signaled = false;
        auto checkReady = [&] {
            if (signaled)
                return;
            if (window.workerClient() && window.workerClient()->ready()) {
                signaled = true;
                std::cout << "CAD_DESKTOP_READY" << std::endl;
                int settleSeconds = 0;
                int idx = app.arguments().indexOf(QStringLiteral("--benchmark-settle"));
                if (idx != -1 && idx + 1 < app.arguments().size()) {
                    settleSeconds = app.arguments()[idx + 1].toInt();
                }
                if (settleSeconds > 0) {
                    QTimer::singleShot(settleSeconds * 1000, &app, &QCoreApplication::quit);
                } else if (!app.arguments().contains(QStringLiteral("--no-exit"))) {
                    QTimer::singleShot(50, &app, &QCoreApplication::quit);
                }
            }
        };
        if (window.workerClient()) {
            QObject::connect(window.workerClient(), &cad::Client::changed, checkReady);
            QObject::connect(window.workerClient(), &cad::Client::projectChanged, checkReady);
            checkReady();
        }
    }
    return app.exec();
}

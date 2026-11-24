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
#ifdef CAD_RELEASE_PACKAGE
    app.setOrganizationName("DatumForge");
    app.setOrganizationDomain("com.datumforge.desktop");
    app.setApplicationName("DatumForge");
#else
    app.setOrganizationName("local.solidworks-clone.dev");
    app.setApplicationName("Solidworks Clone");
#endif
    QString settingsFile;
    QString recoveryRoot;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
            std::cout << "DatumForge version 0.1.0 (Darwin arm64 release)\n"
                      << "Application ID: com.datumforge.desktop\n"
                      << "Build ID: release-20261123-arm64\n"
                      << "Schema version: 4\n"
                      << "Kernel: OpenCASCADE 7.9.2, Qt 6.10.0, PlaneGCS, SQLite 3.50.4\n";
            return 0;
        }
        if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            std::cout << "Usage: DatumForge [options]\n"
                      << "Options:\n"
                      << "  --version, -v        Print version manifest and build information\n"
                      << "  --help, -h           Print this help message\n"
#ifndef CAD_RELEASE_PACKAGE
                      << "  --recovery-root <p>  Override recovery root directory\n"
                      << "  --settings <p>       Override settings file path\n"
                      << "  --benchmark-launch   Exit immediately after reaching ready state (test "
                         "probe)\n"
                      << "  --benchmark-settle <n> Settle for n seconds then exit (test probe)\n"
#endif
                ;
            return 0;
        }
#ifndef CAD_RELEASE_PACKAGE
        if (arg == QStringLiteral("--recovery-root") && i + 1 < argc) {
            recoveryRoot = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QStringLiteral("--settings") && i + 1 < argc) {
            settingsFile = QString::fromLocal8Bit(argv[++i]);
        }
#endif
    }
    cad::MainWindow window(settingsFile, nullptr, recoveryRoot);
    window.show();
#ifndef CAD_RELEASE_PACKAGE
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
#endif
    return app.exec();
}

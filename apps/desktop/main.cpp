#include "MainWindow.hpp"
#include <QApplication>
#include <QSurfaceFormat>
int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setOrganizationName("local.solidworks-clone.dev");
    app.setApplicationName("Solidworks Clone");
    cad::MainWindow window;
    window.show();
    return app.exec();
}

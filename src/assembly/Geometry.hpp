#pragma once
#include "Model.hpp"
#include "geometry/occt/Part.hpp"
namespace cad::assembly {
struct Definition {
    QString kind, name;
    qint64 revision = 0;
    Model assembly;
    kernel::Part part;
    bool resolved = true, stale = false;
};
using Loader = std::function<Definition(QString, bool)>;
Frame reference(const Reference &reference, const Model &model, const Loader &load);
QCborArray references(QString occurrence, QString definition, const Loader &load);
struct Scene {
    DisplayMesh display;
    QCborArray states, bom;
    bool stale = false;
    int exactDefinitions = 0, meshDefinitions = 0;
};
Scene scene(const Model &model, const Loader &load, QMap<QString, DisplayMesh> &coarseCache);
QCborArray interference(const Model &model, const Loader &load);
} // namespace cad::assembly

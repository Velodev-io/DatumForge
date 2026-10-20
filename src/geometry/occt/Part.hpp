#pragma once
#include "features/Model.hpp"
#include "protocol/Mesh.hpp"
#include <TopoDS_Shape.hxx>
namespace cad::kernel {
struct Body {
    QString id;
    TopoDS_Shape shape;
};
struct Part {
    std::vector<Body> bodies;
    QCborArray diagnostics, history;
    QMap<QString, TopoDS_Shape> stages, tools;
    static Part build(const QCborArray &sketches, const QCborArray &features);
    QCborMap measure(qint64 revision, QString body = {}, QString token = {}, double density = 0,
                     QString secondToken = {}) const;
    DisplayMesh mesh(double deflectionMM = .2) const;
    QCborArray topology(QString body = {}) const;
};
} // namespace cad::kernel

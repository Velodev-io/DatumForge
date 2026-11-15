#pragma once
#include "protocol/Mesh.hpp"
#include <QMap>
#include <TopoDS_Shape.hxx>
namespace cad::kernel {
// A bounded, explicitly labelled renderer reference; not a parametric document.
class ReferenceScene {
    TopoDS_Shape shape;
    QMap<QString, TopoDS_Shape> faces, edges;
    DisplayMesh display;

  public:
    void build(double x, double y, double z, int instances);
    const DisplayMesh &mesh() const {
        return display;
    }
    QCborMap measure(const Selection &selection) const;
};
} // namespace cad::kernel

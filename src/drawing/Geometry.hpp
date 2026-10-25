#pragma once
#include "Model.hpp"
#include "assembly/Geometry.hpp"
class QPainter;
namespace cad::drawing {
struct Source {
    TopoDS_Shape shape;
    kernel::Part part;
    qint64 revision = 0;
    QCborArray bom;
};
Source source(QString document, const assembly::Loader &load);
QCborMap render(const Model &model, const assembly::Loader &load);
QCborArray anchors(QString document, const assembly::Loader &load);
void paint(QPainter &painter, QCborMap sheet);
void pdf(QCborMap drawing, QIODevice &output);
} // namespace cad::drawing

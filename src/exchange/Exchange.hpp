#pragma once
#include "features/Model.hpp"
#include <TopoDS_Shape.hxx>
#include <functional>
namespace cad::exchange {
// All kernel coordinates are millimetres. File/override units are explicit.
struct Control {
    std::function<bool()> canceled;
    std::function<void(QString, int)> progress;
    void checkpoint(QString stage, int percent) const;
};
struct Import {
    TopoDS_Shape shape;
    QCborMap report;
};
QString format(QString path);
void preflight(QString path, QString format, const Control &control = {});
Import read(QString path, QString format, QString units = "File", QString healing = "None",
            const Control &control = {});
QCborMap write(const TopoDS_Shape &shape, QString path, QString format, QString units,
               double deflectionMM, const Control &control = {});
QCborMap invariants(const TopoDS_Shape &shape);
} // namespace cad::exchange

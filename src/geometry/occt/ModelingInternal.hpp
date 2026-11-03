#pragma once
#include "ModelingProfiles.hpp"
#include "Supports.hpp"
#include <TopoDS_Face.hxx>
namespace cad::geometry::modeling {
struct Context {
    SupportReferences references;
    QMap<QString, std::vector<kernel::Body>> bodyStages;
};
kernel::Body body(const Context &context, const QCborMap &selection);
TopoDS_Shape moved(const TopoDS_Shape &shape, gp_Vec shift);
TopoDS_Shape join(const TopoDS_Shape &first, const TopoDS_Shape &second, kernel::Part &part,
                  QString owner);
TopoDS_Shape extrude(const ModelingProfile &profile, const QCborMap &intent, const Context &context,
                     const TopoDS_Shape &target, kernel::Part &part,
                     const exchange::Control &control);
TopoDS_Shape draft(const TopoDS_Shape &shape, const std::vector<TopoDS_Face> &faces,
                   gp_Dir direction, gp_Pln neutral, double angle, bool outward, kernel::Part &part,
                   QString owner);
} // namespace cad::geometry::modeling

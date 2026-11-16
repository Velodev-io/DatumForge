#pragma once
#include "ModelingInternal.hpp"

namespace cad::geometry::modeling {
TopoDS_Shape fillet(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                    kernel::Part &part, QString owner);
TopoDS_Shape chamfer(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                     kernel::Part &part, QString owner);
} // namespace cad::geometry::modeling

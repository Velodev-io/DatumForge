#pragma once
#include "ModelingInternal.hpp"

namespace cad::geometry::modeling {
TopoDS_Shape pattern(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                     kernel::Part &part, QString owner);
TopoDS_Shape mirror(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                    kernel::Part &part, QString owner);
} // namespace cad::geometry::modeling

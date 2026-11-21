#pragma once
#include "ModelingInternal.hpp"

namespace cad::geometry::modeling {

TopoDS_Shape loft(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                  kernel::Part &part, QString owner);

} // namespace cad::geometry::modeling

#pragma once
#include "ModelingInternal.hpp"

namespace cad::geometry::modeling {
TopoDS_Shape rib(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                 kernel::Part &part, QString owner, const exchange::Control &control = {});
} // namespace cad::geometry::modeling

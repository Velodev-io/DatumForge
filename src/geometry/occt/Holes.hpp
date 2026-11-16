#pragma once
#include "ModelingInternal.hpp"

namespace cad::geometry::modeling {
TopoDS_Shape hole(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                  kernel::Part &part, QString owner, const exchange::Control &control = {});
QCborMap cosmeticThread(const QCborMap &intent, const Context &context);
} // namespace cad::geometry::modeling

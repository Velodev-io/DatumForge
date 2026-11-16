#pragma once
#include "ModelingInternal.hpp"

namespace cad::geometry::modeling {
TopoDS_Shape pattern(const QCborMap &intent, const Context &context, kernel::Part &part,
                     QString owner, QStringList &identities, const exchange::Control &control = {});
TopoDS_Shape mirror(const QCborMap &intent, const Context &context, kernel::Part &part,
                    QString owner, QStringList &identities);
} // namespace cad::geometry::modeling

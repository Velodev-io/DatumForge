#pragma once
#include "SupportReferences.hpp"
#include "model/Supports.hpp"
namespace cad::geometry {
struct DatumValues {
    QMap<QString, sketch::Frame> planes;
    QMap<QString, SupportAxis> axes;
};
DatumValues datum(const model::Datum &definition, const SupportReferences &references);
} // namespace cad::geometry

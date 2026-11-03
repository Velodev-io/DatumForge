#pragma once
#include "sketch/Sketch.hpp"
namespace cad::model {
// Display-only, evaluated in the sole geometry worker; all lengths are SI.
struct DatumGeometry {
    QString id, kind;
    sketch::Frame frame;
    sketch::Vec3 direction{0, 0, 1};
    static std::vector<DatumGeometry> parse(const QCborArray &values);
};
} // namespace cad::model

#pragma once
#include "sketch/Sketch.hpp"
#include <Geom_Curve.hxx>
namespace cad::geometry {
// Worker-only exact curve in kernel millimetres. Semantic parameters remain SI
// for parabolas, normalized 0..1 for lines, and native angles/knots otherwise.
struct SketchCurve {
    Handle(Geom_Curve) curve;
    double first = 0, last = 1, parameterScale = 1;
};
SketchCurve sketchCurve(const sketch::Sketch &s, const sketch::Entity &e);
} // namespace cad::geometry

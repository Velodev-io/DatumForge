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
std::vector<sketch::Vec2> sketchIntersections(const sketch::Sketch &s, const sketch::Entity &a,
                                              const sketch::Entity &b, bool infiniteFirst = false);
struct SketchFillet {
    sketch::Vec2 center;
    double first, second;
};
std::vector<SketchFillet> sketchFillets(const sketch::Sketch &s, const sketch::Entity &a,
                                        const sketch::Entity &b, double radius);
} // namespace cad::geometry

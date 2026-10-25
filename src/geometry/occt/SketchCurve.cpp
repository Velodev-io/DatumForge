#include "SketchCurve.hpp"
#include "sketch/Curves.hpp"
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Line.hxx>
#include <Geom_Parabola.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
namespace cad::geometry {
SketchCurve sketchCurve(const sketch::Sketch &s, const sketch::Entity &e) {
    auto point = [&](sketch::Vec2 p) {
        auto v = s.frame.world(p);
        return gp_Pnt(v[0] * 1000, v[1] * 1000, v[2] * 1000);
    };
    auto a = point(s.entity(e.a).point);
    auto u = s.frame.u, v = s.frame.v;
    gp_Dir normal(gp_Vec(u[0], u[1], u[2]).Crossed(gp_Vec(v[0], v[1], v[2])));
    SketchCurve out;
    auto range = sketch::curveRange(e);
    out.first = range.first;
    out.last = range.second;
    if (e.kind == "Line") {
        auto b = point(s.entity(e.b).point);
        out.curve = new Geom_Line(a, gp_Dir(gp_Vec(a, b)));
        out.parameterScale = a.Distance(b);
    } else if (e.kind == "Circle" || e.kind == "Arc") {
        out.curve = new Geom_Circle(gp_Ax2(a, normal, gp_Dir(u[0], u[1], u[2])), e.radius * 1000);
    } else if (sketch::ellipseCurve(e.kind)) {
        auto focus = point(s.entity(e.b).point);
        out.curve =
            new Geom_Ellipse(gp_Ax2(a, normal, gp_Dir(gp_Vec(a, focus))),
                             std::hypot(a.Distance(focus), e.radius * 1000), e.radius * 1000);
    } else if (e.kind == "Parabola") {
        auto focus = point(s.entity(e.b).point);
        out.curve =
            new Geom_Parabola(gp_Ax2(a, normal, gp_Dir(gp_Vec(a, focus))), a.Distance(focus));
        out.parameterScale = 1000;
    } else if (e.kind == "Spline") {
        TColgp_Array1OfPnt poles(1, e.poles.size());
        TColStd_Array1OfReal weights(1, e.poles.size()), knots(1, e.knots.size());
        TColStd_Array1OfInteger mult(1, e.multiplicities.size());
        for (int i = 0; i < e.poles.size(); ++i) {
            poles.SetValue(i + 1, point(s.entity(e.poles[i]).point));
            weights.SetValue(i + 1, e.weights[i]);
        }
        for (int i = 0; i < int(e.knots.size()); ++i) {
            knots.SetValue(i + 1, e.knots[i]);
            mult.SetValue(i + 1, e.multiplicities[i]);
        }
        out.curve = new Geom_BSplineCurve(poles, weights, knots, mult, e.degree, false);
    } else
        throw domain::Error("UnsupportedOperation", "Exact source curve required");
    out.first *= out.parameterScale;
    out.last *= out.parameterScale;
    return out;
}
} // namespace cad::geometry

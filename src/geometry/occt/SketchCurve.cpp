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

#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2dGcc_Circ2d2TanRad.hxx>
#include <Geom2dGcc_QualifiedCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pln.hxx>
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

namespace cad::geometry {
namespace {
auto planarCurve(const sketch::Sketch &s, const sketch::Entity &e) {
    auto local = s;
    local.frame = {};
    auto c = sketchCurve(local, e);
    return std::pair{c, GeomAPI::To2d(c.curve, gp_Pln(gp::XOY()))};
}
} // namespace
std::vector<sketch::Vec2> sketchIntersections(const sketch::Sketch &s, const sketch::Entity &a,
                                              const sketch::Entity &b, bool infiniteFirst) {
    try {
        auto [ca, pa] = planarCurve(s, a);
        auto [cb, pb] = planarCurve(s, b);
        if (!infiniteFirst)
            pa = new Geom2d_TrimmedCurve(pa, ca.first, ca.last);
        pb = new Geom2d_TrimmedCurve(pb, cb.first, cb.last);
        Geom2dAPI_InterCurveCurve intersection(pa, pb, 1e-7);
        domain::check(intersection.NbSegments() == 0, "ReferenceAmbiguous",
                      "Overlapping curves require explicit repair");
        std::vector<sketch::Vec2> result;
        for (int i = 1; i <= intersection.NbPoints(); ++i) {
            auto p = intersection.Point(i);
            result.push_back({p.X() / 1000, p.Y() / 1000});
        }
        return result;
    } catch (const Standard_Failure &) {
        throw domain::Error("InvalidGeometry", "Exact curve intersection failed");
    }
}
std::vector<SketchFillet> sketchFillets(const sketch::Sketch &s, const sketch::Entity &a,
                                        const sketch::Entity &b, double radius) {
    try {
        auto [ca, pa] = planarCurve(s, a);
        auto [cb, pb] = planarCurve(s, b);
        Geom2dGcc_Circ2d2TanRad solver(
            Geom2dGcc_QualifiedCurve(Geom2dAdaptor_Curve(pa, ca.first, ca.last),
                                     GccEnt_unqualified),
            Geom2dGcc_QualifiedCurve(Geom2dAdaptor_Curve(pb, cb.first, cb.last),
                                     GccEnt_unqualified),
            radius * 1000, 1e-7);
        domain::check(solver.IsDone(), "InvalidGeometry", "Exact fillet solver failed");
        std::vector<SketchFillet> result;
        for (int i = 1; i <= solver.NbSolutions(); ++i) {
            double ignored, u, v;
            gp_Pnt2d p;
            solver.Tangency1(i, ignored, u, p);
            solver.Tangency2(i, ignored, v, p);
            auto normalize = [](const SketchCurve &c, double u) {
                if (c.curve->IsPeriodic()) {
                    double period = c.curve->Period();
                    u -= std::floor((u - c.first) / period) * period;
                }
                return u;
            };
            u = normalize(ca, u);
            v = normalize(cb, v);
            if (u <= ca.first || u >= ca.last || v <= cb.first || v >= cb.last)
                continue;
            auto center = solver.ThisSolution(i).Location();
            result.push_back({{center.X() / 1000, center.Y() / 1000},
                              u / ca.parameterScale,
                              v / cb.parameterScale});
        }
        return result;
    } catch (const Standard_Failure &) {
        throw domain::Error("InvalidGeometry", "Exact fillet construction failed");
    }
}
} // namespace cad::geometry

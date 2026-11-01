#include "References.hpp"
#include "Curves.hpp"
#include "geometry/occt/SketchCurve.hpp"
#include <GeomAPI_IntCS.hxx>
#include <Geom_Plane.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
namespace cad::sketch {
void resolvePierce(Sketch &target, Constraint &c, const Sketch &source) {
    auto &ref = *c.reference;
    const auto &e = source.entity(ref.entity);
    domain::check(e.kind == ref.role, "ReferenceMissing",
                  "Pierce source changed its semantic curve role; repair the reference");
    struct Root {
        double parameter;
        Vec2 point;
    };
    std::vector<Root> roots;
    auto local = [&](gp_Pnt p) {
        return target.frame.local({p.X() / 1000, p.Y() / 1000, p.Z() / 1000});
    };
    if (e.kind == "Point") {
        auto world = source.frame.world(e.point);
        try {
            roots.push_back({0, target.frame.local(world)});
        } catch (const domain::Error &) {
            throw ReferenceFailure("ReferenceMissing",
                                   "Pierce source point no longer lies on the target plane");
        }
    } else {
        auto inPlane = [&](Vec2 p) {
            try {
                target.frame.local(source.frame.world(p));
                return true;
            } catch (const domain::Error &) {
                return false;
            }
        };
        bool overlap = false;
        if (e.kind == "Line")
            overlap = inPlane(source.entity(e.a).point) && inPlane(source.entity(e.b).point);
        else if (e.kind == "Spline")
            overlap = std::all_of(e.poles.begin(), e.poles.end(),
                                  [&](auto id) { return inPlane(source.entity(id).point); });
        else {
            auto o = source.frame.origin, u = source.frame.u, v = source.frame.v;
            auto point = [&](Vec3 world) {
                try {
                    target.frame.local(world);
                    return true;
                } catch (const domain::Error &) {
                    return false;
                }
            };
            Vec3 a{o[0] + u[0], o[1] + u[1], o[2] + u[2]}, b{o[0] + v[0], o[1] + v[1], o[2] + v[2]};
            overlap = point(o) && point(a) && point(b);
        }
        if (overlap)
            throw ReferenceFailure("ReferenceAmbiguous",
                                   "Pierce source overlaps the target plane; choose a crossing "
                                   "curve or a source point");
        try {
            auto exact = geometry::sketchCurve(source, e);
            Handle(Geom_TrimmedCurve) curve =
                new Geom_TrimmedCurve(exact.curve, exact.first, exact.last);
            auto o = target.frame.origin, u = target.frame.u, v = target.frame.v;
            gp_Dir normal(gp_Vec(u[0], u[1], u[2]).Crossed(gp_Vec(v[0], v[1], v[2])));
            Handle(Geom_Plane) plane =
                new Geom_Plane(gp_Pnt(o[0] * 1000, o[1] * 1000, o[2] * 1000), normal);
            GeomAPI_IntCS op(curve, plane);
            domain::check(op.IsDone(), "SolveFailure", "Exact curve/plane intersection failed");
            if (op.NbSegments())
                throw ReferenceFailure("ReferenceAmbiguous",
                                       "Pierce source overlaps the target plane; choose a crossing "
                                       "curve or a source point");
            for (int i = 1; i <= op.NbPoints(); ++i) {
                double u, v, parameter;
                op.Parameters(i, u, v, parameter);
                roots.push_back({parameter / exact.parameterScale, local(op.Point(i))});
            }
        } catch (const Standard_Failure &e) {
            throw ReferenceFailure("SolveFailure", QString("Exact Pierce intersection failed: ") +
                                                       e.GetMessageString());
        }
    }
    std::sort(roots.begin(), roots.end(),
              [](auto &a, auto &b) { return a.parameter < b.parameter; });
    roots.erase(
        std::unique(roots.begin(), roots.end(),
                    [](auto &a, auto &b) { return std::abs(a.parameter - b.parameter) < 1e-10; }),
        roots.end());
    if ((e.kind == "Circle" || e.kind == "Ellipse") && roots.size() > 1 &&
        std::abs(roots.back().parameter - roots.front().parameter - 6.2831853071795864769) < 1e-8 &&
        length(roots.front().point, roots.back().point) < 1e-10)
        roots.pop_back();
    auto range = e.kind == "Point" ? std::pair<double, double>{-1, 1} : curveRange(e);
    QCborArray choices;
    std::vector<Root> selected;
    for (size_t i = 0; i < roots.size(); ++i) {
        auto root = roots[i];
        double lower = i ? (roots[i - 1].parameter + root.parameter) / 2 : range.first;
        double upper =
            i + 1 < roots.size() ? (roots[i + 1].parameter + root.parameter) / 2 : range.second;
        choices.append(fields({{"constraint", c.id},
                               {"parameter", root.parameter},
                               {"lower", lower},
                               {"upper", upper},
                               {"x", root.point.x},
                               {"y", root.point.y}}));
        if (root.parameter >= ref.lower - 1e-12 && root.parameter <= ref.upper + 1e-12)
            selected.push_back(root);
    }
    if (selected.size() != 1)
        throw ReferenceFailure(selected.empty() ? "ReferenceMissing" : "ReferenceAmbiguous",
                               selected.empty()
                                   ? "Pierce branch no longer intersects this interval; explicitly "
                                     "repair the reference"
                                   : "Multiple Pierce intersections; explicitly select a branch",
                               choices);
    ref.parameter = selected[0].parameter;
    c.value = selected[0].point.x;
    c.second = selected[0].point.y;
}
} // namespace cad::sketch

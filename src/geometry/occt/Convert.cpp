#include "Convert.hpp"
#include <GeomAdaptor_Curve.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
namespace cad::geometry {
sketch::Sketch convert(const model::Conversion &c, const sketch::Frame &target,
                       const SupportReferences &references) {
    c.map();
    target.validate();
    sketch::Sketch out;
    out.id = c.sketch;
    out.frame = target;
    auto identity = [&](QString role) {
        return QUuid::createUuidV5(QUuid(c.id), role.toUtf8()).toString(QUuid::WithoutBraces);
    };
    auto project = [&](gp_Pnt p) {
        auto delta = gp_Vec(kernelPoint(target.origin), p);
        double x = delta.Dot(vector(target.u)) / 1000 - c.center.x,
               y = delta.Dot(vector(target.v)) / 1000 - c.center.y;
        return sketch::Vec2{c.center.x + c.translation.x +
                                c.scale * (x * std::cos(c.angle) - y * std::sin(c.angle)),
                            c.center.y + c.translation.y +
                                c.scale * (x * std::sin(c.angle) + y * std::cos(c.angle))};
    };
    QSet<QString> ids;
    auto point = [&](gp_Pnt p, QString role) {
        domain::check(out.entities.size() < 256 && out.constraints.size() < 128, "ResourceLimit",
                      "Converted points exceed existing entity/variable bounds");
        auto id = identity(role);
        domain::check(!ids.contains(id), "InvalidInput", "Repeated conversion source identity");
        ids.insert(id);
        auto q = project(p);
        out.point(q, c.construction);
        out.entities.back().id = id;
        sketch::Constraint fixed;
        fixed.id = QUuid::createUuidV5(QUuid(id), "fixed").toString(QUuid::WithoutBraces);
        fixed.kind = "Fixed";
        fixed.a = id;
        fixed.value = q.x;
        fixed.second = q.y;
        out.constraints.push_back(fixed);
        return id;
    };
    try {
        for (auto &ref : c.sources)
            for (auto &source : references.curves(ref)) {
                if (source.point) {
                    point(*source.point, "point/" + source.key);
                    continue;
                }
                GeomAdaptor_Curve adaptor(source.curve, std::min(source.first, source.last),
                                          std::max(source.first, source.last));
                if (adaptor.GetType() != GeomAbs_Line) {
                    Handle(Geom_TrimmedCurve) trimmed =
                        new Geom_TrimmedCurve(source.curve, std::min(source.first, source.last),
                                              std::max(source.first, source.last));
                    auto converted = GeomConvert::CurveToBSplineCurve(trimmed);
                    auto spline = Handle(Geom_BSplineCurve)::DownCast(converted->Copy());
                    if (spline->IsPeriodic())
                        spline->SetNotPeriodic();
                    spline->Segment(spline->FirstParameter(), spline->LastParameter());
                    if (source.last < source.first)
                        spline->Reverse();
                    domain::check(
                        spline->Degree() <= 5 && spline->NbPoles() <= 64 && spline->NbKnots() <= 64,
                        "ResourceLimit", "Projected exact curve exceeds existing spline admission");
                    std::vector<sketch::Vec2> projected;
                    for (int i = 1; i <= spline->NbPoles(); ++i)
                        projected.push_back(project(spline->Pole(i)));
                    bool area = false;
                    for (auto a : projected)
                        for (auto b : projected) {
                            double ax = a.x - projected[0].x, ay = a.y - projected[0].y,
                                   bx = b.x - projected[0].x, by = b.y - projected[0].y;
                            area = area || std::abs(ax * by - ay * bx) >
                                               1e-12 * std::hypot(ax, ay) * std::hypot(bx, by);
                        }
                    bool closed = source.curve->Value(source.first)
                                      .Distance(source.curve->Value(source.last)) <= 1e-6;
                    domain::check(!closed || area, "InvalidGeometry",
                                  "Closed curve projection collapses to a line or point");
                    sketch::Entity e;
                    e.id = identity("curve/" + source.key);
                    e.kind = "Spline";
                    e.construction = c.construction;
                    e.degree = spline->Degree();
                    e.start = spline->FirstParameter();
                    e.sweep = spline->LastParameter() - e.start;
                    for (int i = 1; i <= spline->NbPoles(); ++i) {
                        e.poles.append(
                            point(spline->Pole(i), source.key + "/pole/" + QString::number(i)));
                        e.weights.push_back(spline->Weight(i));
                    }
                    e.a = e.poles[0];
                    for (int i = 1; i <= spline->NbKnots(); ++i) {
                        e.knots.push_back(spline->Knot(i));
                        e.multiplicities.push_back(spline->Multiplicity(i));
                    }
                    out.entities.push_back(e);
                    continue;
                }
                auto a = source.curve->Value(source.first), b = source.curve->Value(source.last);
                domain::check(sketch::length(project(a), project(b)) > 1e-9, "InvalidGeometry",
                              "Projected edge collapses to a point");
                auto pa = point(a, source.key + "/start"), pb = point(b, source.key + "/end");
                out.line(pa, pb, c.construction);
                out.entities.back().id = identity("curve/" + source.key);
            }
        out.validate();
        return out;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Exact conversion failed: ") + e.GetMessageString());
    }
}
} // namespace cad::geometry

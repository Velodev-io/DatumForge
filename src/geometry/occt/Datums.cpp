#include "Datums.hpp"
#include <Standard_Failure.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
namespace cad::geometry {
namespace {
gp_Vec normal(const sketch::Frame &f) {
    return vector(f.u).Crossed(vector(f.v));
}
gp_Vec unit(gp_Vec v) {
    domain::check(v.Magnitude() > 1e-6, "InvalidGeometry", "Distinct support points are required");
    return v.Normalized();
}
sketch::Frame planeValue(const model::Datum &d, const SupportReferences &r) {
    const auto &refs = d.references;
    if (d.mode == "Principal") {
        sketch::Frame f;
        auto name = d.parameters.value("plane").toString();
        if (name == "XZ")
            f.v = {0, 0, 1};
        if (name == "YZ") {
            f.u = {0, 1, 0};
            f.v = {0, 0, 1};
        }
        return f;
    }
    if (d.mode == "ParallelPoint") {
        auto f = r.plane(refs[0]);
        f.origin = siPoint(r.point(refs[1]));
        return f;
    }
    if (d.mode == "Angle") {
        auto f = r.plane(refs[0]);
        auto a = r.axis(refs[1]);
        auto n = normal(f), direction = vector(a.direction);
        domain::check(std::abs(n.Dot(direction)) < 1e-10 &&
                          std::abs(n.Dot(gp_Vec(kernelPoint(f.origin), kernelPoint(a.origin)))) <
                              1e-6,
                      "InvalidGeometry", "Angle axis must lie on the reference plane");
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(kernelPoint(a.origin), gp_Dir(direction)),
                             d.parameters.value("angle").toDouble());
        f.origin = siPoint(kernelPoint(f.origin).Transformed(rotation));
        f.u = coordinates(vector(f.u).Transformed(rotation));
        f.v = coordinates(vector(f.v).Transformed(rotation));
        return f;
    }
    if (d.mode == "ThreePoints") {
        auto p = r.point(refs[0]);
        auto u = unit(gp_Vec(p, r.point(refs[1])));
        auto v = unit(gp_Vec(p, r.point(refs[2])));
        return supportFrame(p, u.Crossed(v), u);
    }
    if (d.mode == "LinePoint" || d.mode == "TwoLines") {
        auto a = r.axis(refs[0]);
        auto p = kernelPoint(a.origin);
        auto u = vector(a.direction);
        auto q = d.mode == "LinePoint" ? r.point(refs[1]) : kernelPoint(r.axis(refs[1]).origin);
        auto delta = gp_Vec(p, q);
        auto n = u.Crossed(delta);
        if (d.mode == "TwoLines") {
            auto b = r.axis(refs[1]);
            auto cross = u.Crossed(vector(b.direction));
            if (cross.SquareMagnitude() > 1e-20) {
                domain::check(std::abs(delta.Dot(cross.Normalized())) <= 1e-6, "InvalidGeometry",
                              "Skew lines cannot define one plane");
                n = cross;
            }
        }
        return supportFrame(p, n, u);
    }
    if (d.mode == "NormalCurve") {
        auto c = r.curve(refs[0]);
        gp_Pnt p;
        gp_Vec tangent;
        c.curve->D1(r.parameter(refs[0]), p, tangent);
        if (c.last < c.first)
            tangent.Reverse();
        return supportFrame(p, unit(tangent));
    }
    if (d.mode == "TangentSurface")
        return r.tangent(refs[0], d.parameters.value("u_fraction").toDouble(),
                         d.parameters.value("v_fraction").toDouble());
    if (d.mode == "Midplane") {
        auto a = r.plane(refs[0]), b = r.plane(refs[1]);
        auto n1 = normal(a), n2 = normal(b);
        if (n1.Crossed(n2).SquareMagnitude() <= 1e-20) {
            auto distance = gp_Vec(kernelPoint(a.origin), kernelPoint(b.origin)).Dot(n1);
            domain::check(std::abs(distance) > 1e-7, "ReferenceAmbiguous",
                          "Coincident planes do not select a unique middle plane");
            a.origin = siPoint(kernelPoint(a.origin).Translated(n1 * (distance / 2)));
            return a;
        }
        auto choice = d.parameters.value("bisector").toString();
        domain::check(!choice.isEmpty(), "ReferenceAmbiguous",
                      "Choose one of the two plane bisectors");
        auto sign = choice == "Internal" ? 1. : -1.;
        auto n = n1 + n2 * sign;
        auto distance = gp_Vec(kernelPoint(a.origin).XYZ()).Dot(n1) +
                        sign * gp_Vec(kernelPoint(b.origin).XYZ()).Dot(n2);
        return supportFrame(gp_Pnt((n * (distance / n.SquareMagnitude())).XYZ()), n, vector(a.u));
    }
    throw domain::Error("UnsupportedOperation", "Unsupported plane mode");
}
DatumValues buildDatum(const model::Datum &d, const SupportReferences &references) {
    d.validate();
    if (d.kind == "DatumAxis") {
        SupportAxis a;
        auto &refs = d.references;
        if (d.mode == "Principal") {
            auto name = d.parameters.value("axis").toString();
            a.direction = name == "X"   ? sketch::Vec3{1, 0, 0}
                          : name == "Y" ? sketch::Vec3{0, 1, 0}
                                        : sketch::Vec3{0, 0, 1};
        } else if (d.mode == "Line" || d.mode == "FaceAxis")
            a = references.axis(refs[0]);
        else if (d.mode == "TwoPoints") {
            auto p = references.point(refs[0]), q = references.point(refs[1]);
            a = {siPoint(p), coordinates(unit(gp_Vec(p, q)))};
        } else if (d.mode == "PointNormal") {
            auto p = references.point(refs[0]);
            a = {siPoint(p), coordinates(references.normalAt(refs[1], p))};
        } else {
            auto f = references.plane(refs[0]), g = references.plane(refs[1]);
            auto n1 = normal(f), n2 = normal(g), direction = n1.Crossed(n2);
            if (direction.SquareMagnitude() <= 1e-20)
                throw domain::Error(
                    std::abs(gp_Vec(kernelPoint(f.origin), kernelPoint(g.origin)).Dot(n1)) <= 1e-7
                        ? "ReferenceAmbiguous"
                        : "InvalidGeometry",
                    "Parallel planes do not define a unique intersection axis");
            double d1 = gp_Vec(kernelPoint(f.origin).XYZ()).Dot(n1),
                   d2 = gp_Vec(kernelPoint(g.origin).XYZ()).Dot(n2);
            auto p = (n2.Crossed(direction) * d1 + direction.Crossed(n1) * d2) /
                     direction.SquareMagnitude();
            a = {siPoint(gp_Pnt(p.XYZ())), coordinates(direction.Normalized())};
        }
        if (d.reversed)
            a.direction = coordinates(-vector(a.direction));
        supportFrame(kernelPoint(a.origin), vector(a.direction));
        DatumValues result;
        result.axes.insert(d.id, a);
        return result;
    }
    auto base = d.mode == "Offset" ? references.plane(d.references[0]) : planeValue(d, references);
    auto n = normal(base);
    DatumValues values;
    auto ids = d.outputs();
    for (int i = 0; i < ids.size(); ++i) {
        auto f = base;
        if (d.mode == "Offset")
            f.origin = siPoint(
                kernelPoint(base.origin)
                    .Translated(n * (d.parameters.value("distance").toDouble() * 1000 * (i + 1))));
        if (d.reversed)
            f.v = coordinates(-vector(f.v));
        f.validate();
        values.planes.insert(ids[i], f);
    }
    return values;
}
} // namespace
DatumValues datum(const model::Datum &d, const SupportReferences &references) {
    try {
        return buildDatum(d, references);
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Exact datum construction failed: ") + e.GetMessageString());
    }
}
} // namespace cad::geometry

#pragma once
#include "Part.hpp"
#include "SketchCurve.hpp"
#include "domain/GeometryReference.hpp"
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
namespace cad::geometry {
struct SupportAxis {
    sketch::Vec3 origin{0, 0, 0}, direction{0, 0, 1};
};
struct SupportCurve {
    QString key;
    Handle(Geom_Curve) curve;
    double first = 0, last = 1, parameterScale = 1;
    bool edge = false;
    std::optional<gp_Pnt> point;
};
gp_Pnt kernelPoint(sketch::Vec3 point);
sketch::Vec3 siPoint(gp_Pnt point);
gp_Vec vector(sketch::Vec3 value);
sketch::Vec3 coordinates(gp_Vec value);
sketch::Frame supportFrame(gp_Pnt origin, gp_Vec normal, gp_Vec hint = {1, 0, 0});
QString supportToken(const TopoDS_Shape &shape);
class SupportReferences {
    void scope(const domain::GeometryReference &reference) const;

  public:
    QString document = "Self";
    QMap<QString, sketch::Sketch> sketches;
    QMap<QString, sketch::Frame> planes;
    QMap<QString, SupportAxis> axes;
    QMap<QString, std::vector<SupportCurve>> derived;
    QMap<QString, TopoDS_Shape> stages;
    TopoDS_Shape topology(const domain::GeometryReference &reference) const;
    sketch::Frame plane(const domain::GeometryReference &reference) const;
    SupportAxis axis(const domain::GeometryReference &reference) const;
    SupportCurve curve(const domain::GeometryReference &reference) const;
    double parameter(const domain::GeometryReference &reference) const;
    gp_Pnt point(const domain::GeometryReference &reference) const;
    std::vector<SupportCurve> curves(const domain::GeometryReference &reference) const;
    sketch::Frame tangent(const domain::GeometryReference &reference, double u, double v) const;
    gp_Vec normalAt(const domain::GeometryReference &reference, gp_Pnt point) const;
};
} // namespace cad::geometry

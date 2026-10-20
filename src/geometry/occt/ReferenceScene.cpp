#include "ReferenceScene.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
namespace cad::kernel {
using wire::require;
void ReferenceScene::build(double x, double y, double z, int count) {
    for (double n : {x, y, z})
        require(std::isfinite(n) && n >= 1 && n <= 1000, "reference size must be 1–1000 mm");
    require(count == 1 || count == 100, "reference instance count");
    auto candidate = BRepPrimAPI_MakeBox(x, y, z).Shape();
    require(BRepCheck_Analyzer(candidate).IsValid(), "invalid reference geometry");
    BRepMesh_IncrementalMesh tessellate(candidate, .2, false, .4, false);
    DisplayMesh m;
    m.minimum = {0, 0, 0};
    m.maximum = {float(x), float(y), float(z)};
    QMap<QString, TopoDS_Shape> faceMap, edgeMap;
    for (TopExp_Explorer it(candidate, TopAbs_FACE); it.More(); it.Next()) {
        auto face = TopoDS::Face(it.Current());
        GProp_GProps p;
        BRepGProp::SurfaceProperties(face, p);
        auto center = p.CentreOfMass();
        QString id;
        if (std::abs(center.X()) < 1e-6)
            id = "face/x-min";
        else if (std::abs(center.X() - x) < 1e-6)
            id = "face/x-max";
        else if (std::abs(center.Y()) < 1e-6)
            id = "face/y-min";
        else if (std::abs(center.Y() - y) < 1e-6)
            id = "face/y-max";
        else
            id = std::abs(center.Z()) < 1e-6 ? "face/z-min" : "face/z-max";
        faceMap.insert(id, face);
        auto role = m.faceIds.size();
        m.faceIds.append(id);
        TopLoc_Location loc;
        auto tri = BRep_Tool::Triangulation(face, loc);
        require(!tri.IsNull(), "missing tessellation");
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            if (face.Orientation() == TopAbs_REVERSED)
                std::swap(b, c);
            for (int node : {a, b, c}) {
                auto v = tri->Node(node).Transformed(loc.Transformation());
                m.indices.append(quint32(m.vertices.size()));
                m.vertices.append({float(v.X()), float(v.Y()), float(v.Z())});
            }
            m.triangleRoles.append(quint32(role));
        }
    }
    TopTools_IndexedMapOfShape unique;
    TopExp::MapShapes(candidate, TopAbs_EDGE, unique);
    for (int i = 1; i <= unique.Extent(); ++i) {
        auto edge = TopoDS::Edge(unique(i));
        BRepAdaptor_Curve curve(edge);
        auto a = curve.Value(curve.FirstParameter()), b = curve.Value(curve.LastParameter());
        QString id = "edge";
        const double size[]{x, y, z};
        const char *axes[]{"x", "y", "z"};
        for (int k = 0; k < 3; ++k) {
            double av = a.Coord(k + 1), bv = b.Coord(k + 1);
            if (std::abs(av - bv) < 1e-6)
                id += "/" + QString(axes[k]) + (std::abs(av) < 1e-6 ? "-min" : "-max");
            else
                require(std::abs(std::abs(av - bv) - size[k]) < 1e-6, "reference edge extent");
        }
        require(!edgeMap.contains(id), "ambiguous reference edge");
        edgeMap.insert(id, edge);
        m.edgeIds.append(id);
        m.edges.append({float(a.X()), float(a.Y()), float(a.Z())});
        m.edges.append({float(b.X()), float(b.Y()), float(b.Z())});
    }
    for (int i = 0; i < count; ++i)
        m.instances.append({QString("occurrence/%1").arg(i),
                            count == 1 ? QVector3D()
                                       : QVector3D(float(i % 10) * (float(x) + 5),
                                                   float(i / 10) * (float(y) + 5), 0)});
    m.validate();
    shape = candidate;
    faces = std::move(faceMap);
    edges = std::move(edgeMap);
    display = std::move(m);
}
QCborMap ReferenceScene::measure(const Selection &selected) const {
    require(!shape.IsNull(), "no reference scene");
    bool found = false;
    for (const auto &i : display.instances)
        if (i.id == selected.occurrence)
            found = true;
    require(found, "unknown occurrence");
    GProp_GProps p;
    QString unit, label;
    double value = 0;
    if (selected.kind == "Face") {
        require(faces.contains(selected.entity), "unknown face");
        BRepGProp::SurfaceProperties(faces.value(selected.entity), p);
        value = p.Mass();
        unit = "mm²";
        label = "Exact face area";
    } else if (selected.kind == "Edge") {
        require(edges.contains(selected.entity), "unknown edge");
        BRepGProp::LinearProperties(edges.value(selected.entity), p);
        value = p.Mass();
        unit = "mm";
        label = "Exact edge length";
    } else {
        require((selected.kind == "Body" || selected.kind == "Component") &&
                    selected.entity == "body/reference-box",
                "unknown body");
        BRepGProp::VolumeProperties(shape, p);
        value = p.Mass();
        unit = "mm³";
        label = "Exact body volume";
    }
    require(std::isfinite(value) && value > 0, "invalid exact measurement");
    QCborMap result;
    result.insert(QStringLiteral("value"), value);
    result.insert(QStringLiteral("unit"), unit);
    result.insert(QStringLiteral("label"), label);
    return result;
}
} // namespace cad::kernel

#include "drawing/Geometry.hpp"
#include "support/check.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <QCoreApplication>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        auto id = newId(), body = newId();
        assembly::Definition def;
        def.kind = "Part";
        def.revision = 1;
        def.part.bodies.push_back({body, BRepPrimAPI_MakeCylinder(10, 20).Shape()});
        auto load = [&](QString doc, bool) {
            domain::check(doc == id, "ReferenceMissing", "Deleted source");
            return def;
        };
        drawing::Model m;
        drawing::Sheet s;
        drawing::View v;
        v.source = id;
        v.sourceRevision = 1;
        v.x = 50;
        v.y = 80;
        s.views.push_back(v);
        QString circle, line;
        for (TopExp_Explorer it(def.part.bodies[0].shape, TopAbs_EDGE); it.More(); it.Next()) {
            BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
            if (c.GetType() == GeomAbs_Circle)
                circle = kernel::topologyToken(it.Current());
            else if (c.GetType() == GeomAbs_Line)
                line = kernel::topologyToken(it.Current());
        }
        CHECK(!circle.isEmpty() && !line.isEmpty());
        for (auto kind : {"Radius", "Diameter", "Center", "Linear", "Angular"}) {
            drawing::Annotation a;
            a.view = v.id;
            a.kind = kind;
            a.body = body;
            a.token = (a.kind == "Linear" || a.kind == "Angular") ? line : circle;
            a.secondToken = line;
            a.x = 100;
            a.y = 100 + 5 * s.annotations.size();
            s.annotations.push_back(a);
        }
        s.annotations[0].overrideText = "User label";
        m.sheets.push_back(s);
        auto r = drawing::render(m, load);
        CHECK(r.value("current").toBool());
        auto annotations = r.value("sheets").toArray()[0].toMap().value("annotations").toArray();
        CHECK(std::abs(annotations[0].toMap().value("measured_si").toDouble() - .01) < 1e-10);
        CHECK(annotations[0].toMap().value("label").toString().contains("override"));
        CHECK(std::abs(annotations[1].toMap().value("measured_si").toDouble() - .02) < 1e-10);
        CHECK(std::abs(annotations[3].toMap().value("measured_si").toDouble() - .02) < 1e-10);
        CHECK(std::abs(annotations[4].toMap().value("measured_si").toDouble()) < 1e-10);
        def.part.bodies[0].shape = BRepPrimAPI_MakeCylinder(12, 20).Shape();
        def.revision = 2;
        r = drawing::render(m, load);
        CHECK(!r.value("current").toBool());
        CHECK(r.value("sheets")
                  .toArray()[0]
                  .toMap()
                  .value("annotations")
                  .toArray()[0]
                  .toMap()
                  .value("status") == "Dangling");
        auto missing = [&](QString, bool) -> assembly::Definition {
            throw domain::Error("ReferenceMissing", "Deleted source");
        };
        r = drawing::render(m, missing);
        CHECK(!r.value("current").toBool());
        CHECK(r.value("sheets").toArray()[0].toMap().value("views").toArray()[0].toMap().value(
                  "status") == "Unresolved");
        auto bad = m;
        bad.sheets[0].scale = std::numeric_limits<double>::infinity();
        rejects([&] { drawing::Model::parse(bad.map()); });
        std::cout << "Analytic radius/diameter/center/linear/angular, display override, real "
                     "topology edit and deleted source passed\n";
    });
}

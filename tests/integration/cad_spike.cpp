#include "support/check.hpp"
#include <APIHeaderSection_MakeHeader.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <IGESData_IGESModel.hxx>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <iomanip>
#include <numbers>
#include <sqlite3.h>
namespace {
double volume(const TopoDS_Shape &shape) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(shape, p);
    return p.Mass();
}
int count(const TopoDS_Shape &shape, TopAbs_ShapeEnum kind) {
    int result = 0;
    for (TopExp_Explorer it(shape, kind); it.More(); it.Next())
        ++result;
    return result;
}
void valid(const TopoDS_Shape &shape) {
    CHECK(!shape.IsNull());
    CHECK(BRepCheck_Analyzer(shape).IsValid());
}
TopoDS_Shape cut(const TopoDS_Shape &original, const TopoDS_Shape &tool) {
    BRepAlgoAPI_Cut operation(original, tool);
    CHECK(operation.IsDone());
    valid(operation.Shape());
    return operation.Shape();
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        const auto pi = std::numbers::pi;
        auto metersToMillimeters = [](double m) {
            CHECK(std::isfinite(m) && m > 0);
            return 1000 * m;
        };
        near(metersToMillimeters(0.08), 80);
        BRepBuilderAPI_MakePolygon wire;
        wire.Add(gp_Pnt(0, 0, 0));
        wire.Add(gp_Pnt(80, 0, 0));
        wire.Add(gp_Pnt(80, 50, 0));
        wire.Add(gp_Pnt(0, 50, 0));
        wire.Close();
        auto profile = BRepBuilderAPI_MakeFace(wire.Wire()).Face();
        auto base = BRepPrimAPI_MakePrism(profile, gp_Vec(0, 0, 10)).Shape();
        valid(base);
        near(volume(base), 40000);
        CHECK(count(base, TopAbs_SOLID) == 1);
        auto bracket = cut(base, BRepPrimAPI_MakeBox(gp_Pnt(20, 20, -1), 20, 20, 12).Shape());
        near(volume(bracket), 36000);
        for (auto [x, y] : {std::pair{10., 10.}, {60., 10.}, {10., 40.}, {60., 40.}})
            bracket = cut(
                bracket,
                BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, -1), gp_Dir(0, 0, 1)), 2, 12).Shape());
        near(volume(bracket), 36000 - 160 * pi);
        TopoDS_Edge selected;
        for (TopExp_Explorer it(bracket, TopAbs_EDGE); it.More(); it.Next()) {
            auto edge = TopoDS::Edge(it.Current());
            BRepAdaptor_Curve curve(edge);
            auto mid = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
            if (std::abs(mid.X()) < 1e-8 && std::abs(mid.Y()) < 1e-8 &&
                std::abs(mid.Z() - 5) < 1e-8) {
                selected = edge;
                break;
            }
        }
        CHECK(!selected.IsNull());
        BRepFilletAPI_MakeFillet fillet(bracket);
        fillet.Add(2, selected);
        fillet.Build();
        CHECK(fillet.IsDone());
        valid(fillet.Shape());
        near(volume(fillet.Shape()), 35960 - 150 * pi);
        auto lastValid = fillet.Shape();
        auto previousVolume = volume(lastValid);
        bool rejected = false;
        try {
            BRepFilletAPI_MakeFillet invalid(bracket);
            invalid.Add(500, selected);
            invalid.Build();
            rejected = !invalid.IsDone() || !BRepCheck_Analyzer(invalid.Shape()).IsValid();
        } catch (const Standard_Failure &) {
            rejected = true;
        }
        CHECK(rejected);
        near(volume(lastValid), previousVolume);
        rejects([&] { metersToMillimeters(std::numeric_limits<double>::quiet_NaN()); });
        BRepMesh_IncrementalMesh fine(lastValid, .05);
        auto exactBefore = volume(lastValid);
        BRepMesh_IncrementalMesh coarse(lastValid, 1.0);
        near(volume(lastValid), exactBefore);
        auto changed = BRepPrimAPI_MakeBox(90, 50, 10).Shape();
        valid(changed);
        near(volume(changed), 45000);
        BRepAlgoAPI_Cut split(base, BRepPrimAPI_MakeBox(gp_Pnt(39, -1, -1), 2, 52, 12).Shape());
        CHECK(split.IsDone());
        CHECK(count(split.Shape(), TopAbs_SOLID) == 2);
        TopoDS_Face top;
        for (TopExp_Explorer it(base, TopAbs_FACE); it.More(); it.Next()) {
            GProp_GProps p;
            BRepGProp::SurfaceProperties(it.Current(), p);
            if (std::abs(p.CentreOfMass().Z() - 10) < 1e-8)
                top = TopoDS::Face(it.Current());
        }
        CHECK(!top.IsNull());
        CHECK(split.Modified(top).Size() == 2);
        int candidates = 0;
        for (TopExp_Explorer it(split.Shape(), TopAbs_FACE); it.More(); it.Next()) {
            GProp_GProps p;
            BRepGProp::SurfaceProperties(it.Current(), p);
            if (std::abs(p.CentreOfMass().Z() - 10) < 1e-8 && std::abs(p.Mass() - 1950) < 1e-6)
                ++candidates;
        }
        auto resolution = candidates == 1   ? "Resolved"
                          : candidates == 0 ? "ReferenceMissing"
                                            : "ReferenceAmbiguous";
        CHECK(std::string(resolution) == "ReferenceAmbiguous");
        BRepAlgoAPI_Cut removed(base, base);
        CHECK(removed.IsDone());
        CHECK(removed.IsDeleted(top));
        CHECK(count(removed.Shape(), TopAbs_SOLID) == 0);
        QTemporaryDir fixture(QDir::tempPath() + "/cad-step-zero-b4-XXXXXX");
        CHECK(fixture.isValid());
        auto known = BRepPrimAPI_MakeBox(10, 20, 30).Shape();
        auto stepPath = (fixture.path() + "/box.step").toUtf8();
        STEPControl_Writer step;
        CHECK(step.Transfer(known, STEPControl_AsIs) == IFSelect_RetDone);
        APIHeaderSection_MakeHeader header;
        header.SetName(new TCollection_HAsciiString("B4-box.step"));
        header.SetTimeStamp(new TCollection_HAsciiString("2026-10-19T00:00:00"));
        header.Apply(step.Model());
        CHECK(step.Write(stepPath.constData()) == IFSelect_RetDone);
        STEPControl_Reader stepIn;
        CHECK(stepIn.ReadFile(stepPath.constData()) == IFSelect_RetDone);
        CHECK(stepIn.TransferRoots() > 0);
        valid(stepIn.OneShape());
        near(volume(stepIn.OneShape()), 6000);
        // Real imported source retained in SQLite, caches deleted, then rebuilt.
        QFile source(QString::fromUtf8(stepPath));
        CHECK(source.open(QIODevice::ReadOnly));
        auto sourceBytes = source.readAll();
        source.close();
        sqlite3 *retention = nullptr;
        auto database = (fixture.path() + "/retention.sqlite").toUtf8();
        CHECK(sqlite3_open(database.constData(), &retention) == SQLITE_OK);
        CHECK(
            sqlite3_exec(
                retention,
                "PRAGMA foreign_keys=ON;CREATE TABLE objects(id TEXT PRIMARY KEY,kind TEXT,payload "
                "BLOB);CREATE TABLE roots(kind TEXT,object_id TEXT REFERENCES objects(id));",
                nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_stmt *insert = nullptr;
        CHECK(sqlite3_prepare_v2(retention,
                                 "INSERT INTO objects VALUES('import','authoritative',?)", -1,
                                 &insert, nullptr) == SQLITE_OK);
        CHECK(sqlite3_bind_blob(insert, 1, sourceBytes.constData(), int(sourceBytes.size()),
                                SQLITE_TRANSIENT) == SQLITE_OK);
        CHECK(sqlite3_step(insert) == SQLITE_DONE);
        sqlite3_finalize(insert);
        CHECK(sqlite3_exec(
                  retention,
                  "INSERT INTO roots "
                  "VALUES('undo','import'),('recovery','import'),('job','import');INSERT INTO "
                  "objects VALUES('mesh','cache',x'00');DELETE FROM objects WHERE "
                  "kind='cache';DELETE FROM objects WHERE id NOT IN (SELECT object_id FROM roots);",
                  nullptr, nullptr, nullptr) == SQLITE_OK);
        BRepTools::Clean(known);
        CHECK(QFile::remove(QString::fromUtf8(stepPath)));
        sqlite3_stmt *read = nullptr;
        CHECK(sqlite3_prepare_v2(retention, "SELECT payload FROM objects WHERE id='import'", -1,
                                 &read, nullptr) == SQLITE_OK);
        CHECK(sqlite3_step(read) == SQLITE_ROW);
        QByteArray retained(static_cast<const char *>(sqlite3_column_blob(read, 0)),
                            sqlite3_column_bytes(read, 0));
        CHECK(retained == sourceBytes);
        sqlite3_finalize(read);
        CHECK(sqlite3_close(retention) == SQLITE_OK);
        QFile restored(QString::fromUtf8(stepPath));
        CHECK(restored.open(QIODevice::WriteOnly));
        CHECK(restored.write(retained) == retained.size());
        restored.close();
        STEPControl_Reader regenerated;
        CHECK(regenerated.ReadFile(stepPath.constData()) == IFSelect_RetDone);
        CHECK(regenerated.TransferRoots() > 0);
        valid(regenerated.OneShape());
        near(volume(regenerated.OneShape()), 6000);
        std::cout << "Imported STEP regenerated from retained SQLite BLOB after source/cache "
                     "deletion; undo/recovery/job roots preserved\n";
        auto igesPath = (fixture.path() + "/box.iges").toUtf8();
        IGESControl_Writer iges("MM", 1);
        CHECK(iges.AddShape(known));
        iges.Model()->ChangeGlobalSection().SetFileName(
            new TCollection_HAsciiString("B4-box.iges"));
        iges.Model()->ChangeGlobalSection().SetDate(
            new TCollection_HAsciiString("20261019.000000"));
        iges.Model()->ChangeGlobalSection().SetLastChangeDate(
            new TCollection_HAsciiString("20261019.000000"));
        CHECK(iges.Write(igesPath.constData()));
        IGESControl_Reader igesIn;
        CHECK(igesIn.ReadFile(igesPath.constData()) == IFSelect_RetDone);
        CHECK(igesIn.TransferRoots() > 0);
        valid(igesIn.OneShape());
        for (const auto &path : {stepPath, igesPath}) {
            QFile file(QString::fromUtf8(path));
            CHECK(file.open(QIODevice::ReadOnly));
            std::cout << "B4 generated " << QFileInfo(file).fileName().toStdString()
                      << " bytes=" << file.size() << " sha256="
                      << QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256)
                             .toHex()
                             .constData()
                      << '\n';
        }
        std::cout << "B1 final_volume_mm3=" << std::setprecision(14) << previousVolume
                  << " faces=" << count(lastValid, TopAbs_FACE)
                  << " solids=1; split history=2; ambiguity explicit\n";
    });
}

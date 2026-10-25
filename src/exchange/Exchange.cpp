#include "Exchange.hpp"
#include "protocol/Limits.hpp"
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <IGESData_IGESModel.hxx>
#include <Interface_Check.hxx>
#include <Interface_CheckIterator.hxx>
#include <Interface_Static.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Poly_Triangulation.hxx>
#include <QFile>
#include <QFileInfo>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <ShapeFix_Shape.hxx>
#include <StlAPI_Writer.hxx>
#include <TColStd_SequenceOfAsciiString.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <Transfer_TransientProcess.hxx>
#include <XSControl_TransferReader.hxx>
#include <XSControl_WorkSession.hxx>
namespace cad::exchange {
using domain::check;
using sketch::fields;
namespace {
class Progress : public Message_ProgressIndicator {
    const Control &control;
    int last = -1;
    Standard_Boolean UserBreak() override {
        return control.canceled && control.canceled();
    }
    void Show(const Message_ProgressScope &, Standard_Boolean) override {
        int percent = 25 + int(GetPosition() * 45);
        if (percent != last && control.progress) {
            last = percent;
            control.progress("Translating geometry", percent);
        }
    }

  public:
    explicit Progress(const Control &c) : control(c) {}
};
double mm(QString name) {
    name = name.toLower();
    if (name == "mm" || name == "millimetre" || name == "millimeter")
        return 1;
    if (name == "cm" || name == "centimetre" || name == "centimeter")
        return 10;
    if (name == "m" || name == "metre" || name == "meter")
        return 1000;
    if (name == "in" || name == "inch")
        return 25.4;
    throw domain::Error("UnitMismatch", "Unsupported or missing exchange length unit: " + name);
}
TopoDS_Shape scaled(TopoDS_Shape shape, double ratio) {
    if (std::abs(ratio - 1) < 1e-12)
        return shape;
    gp_Trsf transform;
    transform.SetScale(gp_Pnt(0, 0, 0), ratio);
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}
int count(const TopoDS_Shape &shape, TopAbs_ShapeEnum type) {
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(shape, type, shapes);
    return shapes.Extent();
}
void admit(const TopoDS_Shape &shape) {
    check(!shape.IsNull(), "InvalidGeometry", "Translation produced no geometry");
    check(count(shape, TopAbs_FACE) <= 50000 && count(shape, TopAbs_EDGE) <= 200000 &&
              count(shape, TopAbs_SOLID) <= 9,
          "ResourceLimit", "Imported topology exceeds face/edge/body admission");
    check(count(shape, TopAbs_SOLID) > 0 && BRepCheck_Analyzer(shape).IsValid(), "InvalidGeometry",
          "Only valid closed solid bodies are supported; choose healing explicitly or repair the "
          "source");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    check(std::isfinite(properties.Mass()) && properties.Mass() > 1e-15, "InvalidGeometry",
          "Empty or inverted imported solid");
    for (TopExp_Explorer shells(shape, TopAbs_SHELL); shells.More(); shells.Next())
        check(BRep_Tool::IsClosed(shells.Current()), "InvalidGeometry",
              "Imported solid has an open shell");
    for (auto kind : {TopAbs_VERTEX, TopAbs_EDGE, TopAbs_FACE})
        for (TopExp_Explorer it(shape, kind); it.More(); it.Next()) {
            double tolerance =
                kind == TopAbs_VERTEX ? BRep_Tool::Tolerance(TopoDS::Vertex(it.Current()))
                : kind == TopAbs_EDGE ? BRep_Tool::Tolerance(TopoDS::Edge(it.Current()))
                                      : BRep_Tool::Tolerance(TopoDS::Face(it.Current()));
            check(std::isfinite(tolerance) && tolerance <= 1e-4, "InvalidGeometry",
                  "Imported tolerance exceeds 0.0001 mm policy; repair the source");
        }
    TopTools_IndexedMapOfShape containedFaces, containedEdges;
    for (TopExp_Explorer solids(shape, TopAbs_SOLID); solids.More(); solids.Next()) {
        TopExp::MapShapes(solids.Current(), TopAbs_FACE, containedFaces);
        TopExp::MapShapes(solids.Current(), TopAbs_EDGE, containedEdges);
    }
    check(containedFaces.Extent() == count(shape, TopAbs_FACE) &&
              containedEdges.Extent() == count(shape, TopAbs_EDGE),
          "UnsupportedOperation", "Detached surfaces or curves are not solid-body imports");
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds, false);
    double a, b, c, d, e, f;
    bounds.Get(a, b, c, d, e, f);
    for (double v : {a, b, c, d, e, f})
        check(std::isfinite(v) && std::abs(v) <= 1e6, "ResourceLimit",
              "Exchange coordinates exceed 1000 m admission");
}
} // namespace
void Control::checkpoint(QString stage, int percent) const {
    check(!canceled || !canceled(), "Canceled", "Operation canceled before durable publication");
    if (progress)
        progress(stage, percent);
    check(!canceled || !canceled(), "Canceled", "Operation canceled before durable publication");
}
QString format(QString path) {
    auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "step" || suffix == "stp")
        return "STEP";
    if (suffix == "iges" || suffix == "igs")
        return "IGES";
    if (suffix == "stl")
        return "STL";
    throw domain::Error("UnsupportedOperation", "Choose STEP, IGES or STL");
}
void preflight(QString path, QString kind, const Control &control) {
    check(kind == "STEP" || kind == "IGES", "UnsupportedOperation",
          "Import supports STEP and IGES");
    QFile input(path);
    check(input.open(QIODevice::ReadOnly), "PermissionDenied", "Cannot read exchange source");
    check(input.size() > 0 && input.size() <= qint64(limits::exchange), "ResourceLimit",
          "Source admission is 100 MiB");
    // Bound parser records before OCCT allocation. This is an admission envelope,
    // not a claim that arbitrary kernel allocation is bounded by source bytes.
    qint64 entities = 0, record = 0, bytes = 0;
    QByteArray tail, first;
    while (!input.atEnd()) {
        control.checkpoint("Checking source", int(bytes * 20 / input.size()));
        auto chunk = input.read(65536);
        check(!chunk.isEmpty(), "InvalidInput", "Source read failed");
        if (first.isEmpty())
            first = chunk.left(128);
        auto scan = (tail + chunk).toUpper();
        check(!scan.contains("EXTERNAL_SOURCE") && !scan.contains("EXTERNALLY_DEFINED") &&
                  !scan.contains("DOCUMENT_FILE") &&
                  !scan.contains("NEXT_ASSEMBLY_USAGE_OCCURRENCE") && !scan.contains("MAPPED_ITEM"),
              "UnsupportedOperation",
              "External references and assembly/mapped occurrences are unsupported by this part "
              "importer");
        tail = scan.right(64);
        for (char ch : chunk) {
            ++record;
            if ((kind == "STEP" && ch == ';') || (kind == "IGES" && ch == '\n')) {
                record = 0;
                ++entities;
            }
            check(record <= 65536 && entities <= 200000, "ResourceLimit",
                  "Exchange record/entity expansion admission exceeded");
        }
        bytes += chunk.size();
    }
    check(kind != "STEP" || first.trimmed().startsWith("ISO-10303-21;"), "InvalidInput",
          "STEP header is missing");
    check(record <= 128 && entities > 0, "InvalidInput", "Truncated exchange records");
    check(kind != "STEP" || tail.trimmed().endsWith("END-ISO-10303-21;"), "InvalidInput",
          "STEP terminator is missing");
    if (kind == "IGES") {
        auto end = tail.trimmed();
        check(end.size() >= 8 && end.at(end.size() - 8) == 'T', "InvalidInput",
              "IGES terminate record is missing");
    }
}
QCborMap invariants(const TopoDS_Shape &shape) {
    GProp_GProps volume, area;
    BRepGProp::VolumeProperties(shape, volume);
    BRepGProp::SurfaceProperties(shape, area);
    Bnd_Box box;
    BRepBndLib::Add(shape, box, false);
    double a, b, c, d, e, f;
    box.Get(a, b, c, d, e, f);
    return fields({{"solids", count(shape, TopAbs_SOLID)},
                   {"faces", count(shape, TopAbs_FACE)},
                   {"volume_mm3", volume.Mass()},
                   {"area_mm2", area.Mass()},
                   {"bounds_mm", QCborArray{a, b, c, d, e, f}}});
}
Import read(QString path, QString kind, QString units, QString healing, const Control &control) {
    preflight(path, kind, control);
    check(units == "File" || QStringList{"mm", "cm", "m", "in"}.contains(units), "UnitMismatch",
          "Choose file units or an explicit override");
    check(healing == "None" || healing == "Fix", "InvalidInput",
          "Choose None or Fix healing explicitly");
    control.checkpoint("Parsing source (parser may not report intermediate progress)", 20);
    Handle(Progress) progress = new Progress(control);
    TopoDS_Shape shape;
    QString declared;
    int translatorWarnings = 0;
    auto modelChecked = [&](XSControl_Reader &reader) {
        check(reader.Model()->NbEntities() <= 200000, "ResourceLimit",
              "Parser entity admission exceeded");
        auto checks = reader.WS()->ModelCheckList(false);
        for (checks.Start(); checks.More(); checks.Next()) {
            check(checks.Value()->NbFails() == 0, "InvalidInput",
                  "Source contains malformed entities; partial import refused");
            translatorWarnings += checks.Value()->NbWarnings();
        }
    };
    auto translated = [&](XSControl_Reader &reader) {
        auto checks = reader.WS()->TransferReader()->TransientProcess()->CheckList(false);
        for (checks.Start(); checks.More(); checks.Next()) {
            check(checks.Value()->NbFails() == 0, "InvalidGeometry",
                  "Translator reported failed entities; partial import refused");
            translatorWarnings += checks.Value()->NbWarnings();
        }
    };
    if (kind == "STEP") {
        STEPControl_Reader reader;
        check(reader.ReadFile(QFile::encodeName(path).constData()) == IFSelect_RetDone,
              "InvalidInput", "STEP parser failed");
        modelChecked(reader);
        reader.SetSystemLengthUnit(1.);
        TColStd_SequenceOfAsciiString length, angle, solid;
        reader.FileUnits(length, angle, solid);
        check(length.Length() == 1, "UnitMismatch", "STEP must declare one supported length unit");
        declared = QString::fromLatin1(length.Value(1).ToCString());
        mm(declared);
        ShapeProcess::OperationsFlags flags;
        reader.SetShapeProcessFlags(flags);
        control.checkpoint("Transferring STEP", 25);
        check(reader.TransferRoots(progress->Start()) > 0, "InvalidGeometry",
              "STEP transfer failed");
        translated(reader);
        shape = reader.OneShape();
    } else {
        IGESControl_Controller::Init();
        Interface_Static::SetCVal("xstep.cascade.unit", "MM");
        IGESControl_Reader reader;
        check(reader.ReadFile(QFile::encodeName(path).constData()) == IFSelect_RetDone,
              "InvalidInput", "IGES parser failed");
        modelChecked(reader);
        auto header = reader.IGESModel()->GlobalSection();
        declared = header.UnitName().IsNull() ? QString()
                                              : QString::fromLatin1(header.UnitName()->ToCString());
        mm(declared);
        ShapeProcess::OperationsFlags flags;
        reader.SetShapeProcessFlags(flags);
        control.checkpoint("Transferring IGES", 25);
        check(reader.TransferRoots(progress->Start()) > 0, "InvalidGeometry",
              "IGES transfer failed");
        translated(reader);
        shape = reader.OneShape();
    }
    control.checkpoint("Validating imported geometry", 75);
    check(!shape.IsNull(), "InvalidGeometry", "Empty translated result");
    if (units != "File")
        shape = scaled(shape, mm(units) / mm(declared));
    auto before = invariants(shape);
    if (healing == "Fix") {
        ShapeFix_Shape fix(shape);
        fix.SetPrecision(1e-4);
        fix.SetMaxTolerance(1e-4);
        fix.Perform(progress->Start());
        shape = fix.Shape();
    }
    control.checkpoint("Checking shape validity", 85);
    admit(shape);
    QCborArray warnings{"Imported geometry only; proprietary feature history is not recovered.",
                        "Names, colors, layers, PMI and assembly structure are not retained by "
                        "this solid-body adapter."};
    if (translatorWarnings)
        warnings.append(QString("Translator reported %1 warning(s); validate the imported geometry "
                                "before downstream use.")
                            .arg(translatorWarnings));
    if (units != "File")
        warnings.append("File units explicitly overridden; dimensions were rescaled.");
    if (healing == "Fix")
        warnings.append("Explicit ShapeFix applied; compare before/after invariants.");
    return {shape, fields({{"format", kind},
                           {"file_units", declared},
                           {"units_choice", units},
                           {"healing", healing},
                           {"before", before},
                           {"after", invariants(shape)},
                           {"warnings", warnings},
                           {"translator_warning_count", translatorWarnings},
                           {"engine", "OCCT 7.9.2;exchange=1"}})};
}
QCborMap write(const TopoDS_Shape &input, QString path, QString kind, QString units,
               double deflectionMM, const Control &control) {
    admit(input);
    domain::units(units);
    check(kind == "STEP" || kind == "IGES" || kind == "STL", "UnsupportedOperation",
          "Unknown export format");
    check(std::isfinite(deflectionMM) && deflectionMM >= .01 && deflectionMM <= 5, "InvalidInput",
          "STL detail must be 0.01–5 mm");
    control.checkpoint("Preparing export", 10);
    Handle(Progress) progress = new Progress(control);
    bool done = false;
    if (kind == "STEP") {
        STEPControl_Writer writer;
        auto unit = units == "in" ? QByteArray("INCH") : units.toUpper().toLatin1();
        Interface_Static::SetCVal("write.step.unit", unit.constData());
        done =
            writer.Transfer(input, STEPControl_AsIs, true, progress->Start()) == IFSelect_RetDone &&
            writer.Write(QFile::encodeName(path).constData()) == IFSelect_RetDone;
    } else if (kind == "IGES") {
        IGESControl_Controller::Init();
        auto unit = units == "in" ? QByteArray("INCH") : units.toUpper().toLatin1();
        IGESControl_Writer writer(unit.constData(), 1);
        done = writer.AddShape(input, progress->Start()) &&
               writer.Write(QFile::encodeName(path).constData());
    } else {
        auto shape = BRepBuilderAPI_Copy(input, true, false).Shape();
        shape = scaled(shape, 1 / mm(units));
        BRepMesh_IncrementalMesh mesher;
        mesher.SetShape(shape);
        mesher.ChangeParameters().Deflection = deflectionMM / mm(units);
        mesher.ChangeParameters().Angle = .3;
        mesher.ChangeParameters().InParallel = false;
        mesher.Perform(progress->Start());
        control.checkpoint("Validating STL mesh", 75);
        check(mesher.IsDone(), "InvalidGeometry", "STL tessellation failed");
        qint64 triangles = 0;
        for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
            TopLoc_Location location;
            auto mesh = BRep_Tool::Triangulation(TopoDS::Face(faces.Current()), location);
            check(!mesh.IsNull(), "InvalidGeometry", "Missing STL face triangulation");
            triangles += mesh->NbTriangles();
            check(triangles <= 1000000, "ResourceLimit",
                  "STL exceeds one million triangles; choose coarser detail");
        }
        StlAPI_Writer writer;
        writer.ASCIIMode() = false;
        done = writer.Write(shape, QFile::encodeName(path).constData(), progress->Start());
    }
    control.checkpoint("Checking translated output", 90);
    check(done && QFileInfo(path).size() > 0, "StorageFull",
          "Export writer failed; destination preserved");
    check(QFileInfo(path).size() <= qint64(limits::exchange), "ResourceLimit",
          "Export exceeds 100 MiB output admission");
    return fields(
        {{"format", kind},
         {"units", units},
         {"deflection_mm", deflectionMM},
         {"geometry", invariants(input)},
         {"warnings",
          QCborArray{
              "Geometry only; history, names, colors, PMI and document relationships are omitted.",
              kind == "STL"
                  ? "STL is faceted and unitless; use the selected length unit when opening it."
                  : "Neutral geometry does not preserve proprietary design intent."}}});
}
} // namespace cad::exchange

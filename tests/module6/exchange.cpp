#include "exchange/Exchange.hpp"
#include "support/check.hpp"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Writer.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <bit>
#include <gp_Pln.hxx>
using namespace cad;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module6-exchange-XXXXXX");
        CHECK(root.isValid());
        QFile mark(root.filePath(".cad-test-root"));
        CHECK(mark.open(QIODevice::WriteOnly));
        mark.write("disposable\n");
        mark.close();
        auto box = BRepPrimAPI_MakeBox(10, 20, 30).Shape();
        QJsonArray actualFiles;
        auto recordFile = [&](QString path, QCborMap report) {
            QFile input(path);
            CHECK(input.open(QIODevice::ReadOnly));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            CHECK(hash.addData(&input));
            auto row = QCborValue(report).toJsonValue().toObject();
            row.insert("name", QFileInfo(path).fileName());
            row.insert("bytes", input.size());
            row.insert("sha256", QString::fromLatin1(hash.result().toHex()));
            actualFiles.append(row);
        };
        int files = 0;
        for (auto kind : {QString("STEP"), QString("IGES")})
            for (auto unit : {QString("mm"), QString("in"), QString("cm"), QString("m")}) {
                auto path = root.filePath(
                    QString("b4-%1-%2.%3").arg(kind, unit, kind == "STEP" ? "step" : "iges"));
                auto report = exchange::write(box, path, kind, unit, .2);
                CHECK(!report.value("warnings").toArray().empty());
                auto result = exchange::read(path, kind, "File", "None");
                recordFile(path, result.report);
                auto invariants = exchange::invariants(result.shape);
                CHECK(invariants.value("faces").toInteger() == 6);
                CHECK(invariants.value("solids").toInteger() == 1);
                near(invariants.value("volume_mm3").toDouble(), 6000, 1e-4);
                near(invariants.value("area_mm2").toDouble(), 2200, 1e-4);
                CHECK(result.report.value("file_units").isString());
                // Deliberately wrong explicit units rescale; never silently guess.
                if (unit == "mm") {
                    auto wrong = exchange::read(path, kind, "in", "None");
                    near(exchange::invariants(wrong.shape).value("volume_mm3").toDouble(),
                         6000 * 25.4 * 25.4 * 25.4, .1);
                    CHECK(wrong.report.value("warnings").toArray().size() >= 3);
                    auto healed = exchange::read(path, kind, "File", "Fix");
                    near(exchange::invariants(healed.shape).value("volume_mm3").toDouble(), 6000,
                         1e-4);
                }
                rejects([&] { exchange::read(path, kind, "unknown", "None"); });
                rejects([&] { exchange::read(path, kind, "File", "automatic"); });
                bool canceled = false;
                exchange::Control control{[&] { return canceled; },
                                          [&](QString, int p) {
                                              if (p >= 25)
                                                  canceled = true;
                                          }};
                rejects([&] { exchange::read(path, kind, "File", "None", control); });
                ++files;
            }
        for (auto unit : {QString("mm"), QString("in")}) {
            auto path = root.filePath("mesh-" + unit + ".stl");
            exchange::write(box, path, "STL", unit, .1);
            QFile input(path);
            CHECK(input.open(QIODevice::ReadOnly));
            auto bytes = input.readAll();
            CHECK(bytes.size() >= 84);
            auto triangles =
                qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + 80));
            CHECK(triangles == 12 && bytes.size() == 84 + 50 * triangles);
            float maxX = 0, maxY = 0, maxZ = 0;
            for (quint32 i = 0; i < triangles; ++i)
                for (int v = 0; v < 3; ++v)
                    for (int axis = 0; axis < 3; ++axis) {
                        auto bits = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(
                            bytes.constData() + 84 + 50 * i + 12 + 12 * v + 4 * axis));
                        float value = std::bit_cast<float>(bits);
                        if (axis == 0)
                            maxX = qMax(maxX, value);
                        if (axis == 1)
                            maxY = qMax(maxY, value);
                        if (axis == 2)
                            maxZ = qMax(maxZ, value);
                    }
            double factor = unit == "in" ? 25.4 : 1;
            near(maxX * factor, 10, 1e-5);
            near(maxY * factor, 20, 1e-5);
            near(maxZ * factor, 30, 1e-5);
        }
        // B4 named/colored source: unsupported metadata must produce explicit loss warnings.
        Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
        auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
        auto label = shapes->AddShape(box, false);
        TDataStd_Name::Set(label, "B4 colored box");
        XCAFDoc_DocumentTool::ColorTool(document->Main())
            ->SetColor(label, Quantity_Color(.8, .1, .2, Quantity_TOC_RGB), XCAFDoc_ColorGen);
        STEPCAFControl_Writer coloredWriter;
        coloredWriter.SetNameMode(true);
        coloredWriter.SetColorMode(true);
        CHECK(coloredWriter.Transfer(document, STEPControl_AsIs));
        auto colored = root.filePath("colored.step");
        CHECK(coloredWriter.Write(QFile::encodeName(colored).constData()) == IFSelect_RetDone);
        auto coloredResult = exchange::read(colored, "STEP");
        recordFile(colored, coloredResult.report);
        near(exchange::invariants(coloredResult.shape).value("volume_mm3").toDouble(), 6000, 1e-4);
        CHECK(coloredResult.report.value("warnings").toArray().at(1).toString().contains("colors"));
        // A valid surface-only exchange is unsupported, rather than silently accepted as a solid.
        auto surface = root.filePath("surface.step");
        STEPControl_Writer surfaceWriter;
        CHECK(surfaceWriter.Transfer(
                  BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 0, 10, 0, 10)
                      .Shape(),
                  STEPControl_AsIs) == IFSelect_RetDone);
        CHECK(surfaceWriter.Write(QFile::encodeName(surface).constData()) == IFSelect_RetDone);
        rejects([&] { exchange::read(surface, "STEP"); });
        auto bad = root.filePath("invalid.step");
        auto put = [&](QByteArray bytes) {
            QFile f(bad);
            CHECK(f.open(QIODevice::WriteOnly));
            CHECK(f.write(bytes) == bytes.size());
        };
        put("ISO-10303-21;\nDATA;\n#1=NOT_A_VALID_ENTITY();\nENDSEC;\nEND-ISO-10303-21;\n");
        rejects([&] { exchange::read(bad, "STEP"); });
        put("ISO-10303-21;\n#1=TRUNCATED(");
        rejects([&] { exchange::read(bad, "STEP"); });
        put("ISO-10303-21;\n" + QByteArray(65537, 'A') + ";\n");
        rejects([&] { exchange::preflight(bad, "STEP"); });
        put("ISO-10303-21;\n#1=EXTERNAL_SOURCE('file');\n");
        rejects([&] { exchange::preflight(bad, "STEP"); });
        {
            QFile f(bad);
            CHECK(f.open(QIODevice::WriteOnly));
            CHECK(f.resize(qint64(limits::exchange) + 1));
        }
        rejects([&] { exchange::preflight(bad, "STEP"); });
        auto empty = root.filePath("empty.iges");
        {
            QFile f(empty);
            CHECK(f.open(QIODevice::WriteOnly));
        }
        rejects([&] { exchange::read(empty, "IGES"); });
        CHECK(files == 8);
        auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
        QFile evidence((directory.isEmpty() ? QDir::currentPath() : directory) +
                       "/module6-b4.json");
        CHECK(evidence.open(QIODevice::WriteOnly));
        evidence.write(
            QJsonDocument(
                QJsonObject{
                    {"fixture",
                     "B4 frozen 10/20/30 mm box recipe plus unit/colored/adversarial variants"},
                    {"status", "Pass"},
                    {"files", actualFiles},
                    {"limits", "Small analytic corpus; not full industrial corpus or "
                               "physical-memory acceptance"}})
                .toJson());
        std::cout
            << "B4: 8 unit-tagged STEP/IGES round trips; 6 faces, 1 solid, 6000 mm3 / 2200 mm2; "
               "STL mm/in coordinates, malformed/oversized/expansion/cancel corpus passed\n";
    });
}

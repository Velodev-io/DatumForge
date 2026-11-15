#include "persistence/ProjectStore.hpp"
#include "sketch/Constructions.hpp"
#include "sketch/Editing.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
using sketch::fields;
namespace {
std::vector<sketch::Vec2> inputs(QString method, double scale = 1) {
    std::vector<sketch::Vec2> p;
    if (method == "Corner" || method == "Center")
        p = {{-.01, .002}, {.02, .022}};
    else if (method == "3 Point Corner")
        p = {{.01, .01}, {.034, .028}, {.0252, .0464}};
    else if (method == "3 Point Center")
        p = {{.01, .01}, {.034, .028}, {.0266, .0412}};
    else
        p = {{.01, .01}, {.034, .028}, {.044, .058}};
    for (auto &v : p) {
        v.x *= scale;
        v.y *= scale;
    }
    return p;
}
double area(const sketch::Sketch &s, const sketch::ConstructionGroup &g) {
    double sum = 0;
    for (int i = 0; i < 4; ++i) {
        auto a = s.entity(g.vertices[i]).point, b = s.entity(g.vertices[(i + 1) % 4]).point;
        sum += a.x * b.y - a.y * b.x;
    }
    return std::abs(sum) / 2;
}
void analytical() {
    for (double scale : {.001, 1., 1000.})
        for (auto method : sketch::rectangleMethods()) {
            std::cout << "Rectangle corpus " << method.toStdString() << " scale " << scale << "\n";
            sketch::Sketch s;
            sketch::createRectangle(s, method, inputs(method, scale));
            CHECK(s.map().value("version").toInteger() == 3);
            auto g = s.groups[0];
            auto before = sketch::rectangleParameters(s, g);
            auto unchanged = sketch::dragEntity(s, g.vertices[0], {.01, .02}, {.01, .02});
            CHECK(unchanged.map() == s.map());
            rejects([&] { sketch::dragEntity(s, newId(), {}, {}); });
            auto out = sketch::solve(sketch::Sketch::parse(s.map()));
            CHECK(out.valid && out.maximumResidual <= 1e-7);
            int expectedFreedom = method == "Parallelogram"    ? 6
                                  : method.contains("3 Point") ? 5
                                                               : 4;
            CHECK(out.freedom == expectedFreedom);
            auto &solved = out.sketch;
            double expected = method == "Corner"           ? .03 * .02
                              : method == "Center"         ? .06 * .04
                              : method == "3 Point Corner" ? .03 * .02
                              : method == "3 Point Center" ? .03 * .06
                                                           : .00054;
            near(area(solved, g), expected * scale * scale, 1e-9 * scale * scale);
            if (!g.center.isEmpty())
                for (int i = 0; i < 2; ++i) {
                    auto a = solved.entity(g.vertices[i]).point,
                         b = solved.entity(g.vertices[i + 2]).point,
                         c = solved.entity(g.center).point;
                    near((a.x + b.x) / 2, c.x, 1e-9 * scale);
                    near((a.y + b.y) / 2, c.y, 1e-9 * scale);
                }
            auto changed = before;
            changed.width *= 1.4;
            changed.side *= .8;
            if (method.contains("3 Point") || method == "Parallelogram")
                changed.rotation += .2;
            if (method == "Parallelogram")
                changed.sideAngle *= .9;
            sketch::resizeRectangle(s, g, changed);
            auto edited = sketch::solve(s);
            CHECK(edited.valid);
            auto actual = sketch::rectangleParameters(edited.sketch, g);
            near(actual.width, changed.width, 1e-8 * scale);
            near(actual.side, changed.side, 1e-8 * scale);
            CHECK(edited.sketch.groups[0].map() == g.map());
            auto anchored = s;
            sketch::Constraint fixed;
            fixed.kind = "Fixed";
            fixed.a = g.center.isEmpty() ? g.vertices[0] : g.center;
            auto position = anchored.entity(fixed.a).point;
            fixed.value = position.x;
            fixed.second = position.y;
            anchored.constraints.push_back(fixed);
            auto resized = anchored;
            auto parameters = sketch::rectangleParameters(anchored, g);
            parameters.width *= 1.1;
            sketch::resizeRectangle(resized, g, parameters);
            sketch::checkConstructionEdit(anchored, resized, g);
            CHECK(sketch::solve(resized).valid);
            parameters.anchor.x += .002 * scale;
            sketch::resizeRectangle(resized, g, parameters);
            rejects([&] { sketch::checkConstructionEdit(anchored, resized, g); });
            for (auto id : sketch::constructionMembers(g))
                CHECK(edited.sketch.entity(id).id == id);
            auto dragged = sketch::dragEntity(s, g.edges[0], {}, {.001 * scale, .002 * scale});
            CHECK(sketch::solve(dragged).valid);
            auto corner = sketch::dragEntity(s, g.vertices[2], {}, {.001 * scale, .001 * scale});
            CHECK(sketch::solve(corner).valid);
            if (!g.center.isEmpty())
                near(sketch::length(corner.entity(g.center).point, s.entity(g.center).point), 0,
                     1e-10 * scale);
            auto old = s.map();
            old.insert(QStringLiteral("version"), 2);
            rejects([&] { sketch::Sketch::parse(old); });
            auto bad = s;
            bad.constraints.erase(bad.constraints.begin());
            rejects([&] { bad.validate(); });
            bad = s;
            bad.groups[0].edges[0] = g.edges[1];
            rejects([&] { bad.validate(); });
            bad = s;
            bad.groups[0].id = s.id;
            rejects([&] { bad.validate(); });
            auto empty = sketch::eraseEntity(s, g.vertices[0]);
            CHECK(empty.entities.empty() && empty.constraints.empty() && empty.groups.empty());
            sketch::Constraint c;
            c.kind = "Length";
            c.a = g.edges[0];
            c.expression = "Span";
            s.names["Span"] = QString::number(changed.width) + " m";
            s.constraints.push_back(c);
            auto dimensioned = sketch::solve(s);
            CHECK(dimensioned.valid && dimensioned.freedom == expectedFreedom - 1);
            CHECK(sketch::dragEntity(s, g.vertices[0], {}, {}).map() == s.map());
            rejects([&] { sketch::eraseEntity(s, g.edges[0]); });
            rejects([&] { sketch::dragEntity(s, g.vertices[0], {}, {.001 * scale, 0}); });
        }
    sketch::Sketch s;
    auto before = s.map();
    for (QString method : sketch::rectangleMethods())
        rejects([&] { sketch::createRectangle(s, method, {{0, 0}, {0, 0}, {0, 0}}); });
    CHECK(s.map() == before);
    for (int i = 0; i < 250; ++i)
        s.point({i * .0001, 0});
    before = s.map();
    rejects([&] { sketch::createRectangle(s, "Corner", {{0, 0}, {.01, .01}}); });
    CHECK(s.map() == before);
}
void mark(QString path) {
    QFile f(path + "/.cad-test-root");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
QByteArray bytes(QString path) {
    QFile f(path);
    CHECK(f.open(QIODevice::ReadOnly));
    return f.readAll();
}
void persistence() {
    for (auto method : sketch::rectangleMethods()) {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-rect-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-cm1-rect-files-XXXXXX");
        CHECK(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        storage::ProjectStore store(root.path());
        auto project = newId(), doc = newId();
        store.create(project);
        auto command = [&](QString name, QCborMap payload = QCborMap{}) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = doc;
            e.command = name;
            e.payload = payload;
            e.revision = store.revision();
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"type", "Part"},
                                                        {"name", "Rectangle"},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
        sketch::Sketch legacy;
        legacy.rectangle({0, 0}, {.02, .01});
        store.execute(command("SetSketches", fields({{"sketches", sketch::encode({legacy})}})));
        auto legacyState = store.sketches(doc);
        auto legacyPath = files.filePath("legacy.cadproj");
        store.save(command("SaveProject"), legacyPath);
        auto original = bytes(legacyPath);
        auto relations = legacy;
        sketch::Constraint alignment;
        alignment.kind = "Horizontal";
        alignment.a = relations.entities[0].id;
        alignment.b = relations.entities[1].id;
        relations.constraints.push_back(alignment);
        store.execute(command("SetSketches", fields({{"sketches", sketch::encode({relations})}})));
        auto relationPath = files.filePath("relations-v2.cadproj");
        store.save(command("SaveProject"), relationPath);
        auto relationBytes = bytes(relationPath);
        store.execute(command("SetSketches", fields({{"sketches", legacyState}})));
        sketch::Sketch s;
        sketch::createRectangle(s, method, inputs(method));
        auto accepted = sketch::encode({s});
        auto edit = command("SetSketches", fields({{"sketches", accepted}}));
        store.execute(edit);
        accepted = store.sketches(doc);
        CHECK(sketch::parse(accepted)[0].groups[0].map() == s.groups[0].map());
        auto rev = store.revision();
        store.execute(edit);
        CHECK(store.revision() == rev);
        bool refused = false;
        try {
            store.save(command("SaveProject"), legacyPath);
        } catch (const domain::Error &e) {
            refused = e.code == "UpgradeCopyRequired";
        }
        CHECK(refused && bytes(legacyPath) == original);
        refused = false;
        try {
            store.save(command("SaveProject"), relationPath);
        } catch (const domain::Error &e) {
            refused = e.code == "UpgradeCopyRequired";
        }
        CHECK(refused && bytes(relationPath) == relationBytes);
        auto lowerPath = files.filePath("relation-only.cadproj");
        sketch::Sketch relationOnly;
        auto a = relationOnly.point({0, 0}), b = relationOnly.point({.01, 0});
        sketch::Constraint h;
        h.kind = "Horizontal";
        h.a = a;
        h.b = b;
        relationOnly.constraints.push_back(h);
        store.execute(
            command("SetSketches", fields({{"sketches", sketch::encode({relationOnly})}})));
        // The manifest remains at group level even after replacing the active state.
        {
            storage::Database db(store.workingPath(), true);
            CHECK(db.scalar("SELECT manifest FROM project_meta")
                      .toString()
                      .endsWith(";sketch-groups=1"));
        }
        store.execute(command("Undo"));
        CHECK(store.sketches(doc) == accepted);
        auto conflict = s;
        for (int i : {0, 2}) {
            sketch::Constraint c;
            c.kind = "Length";
            c.a = s.groups[0].edges[i];
            c.value = i == 0 ? .01 : .02;
            conflict.constraints.push_back(c);
        }
        auto unchangedRevision = store.revision();
        rejects([&] {
            store.execute(
                command("SetSketches", fields({{"sketches", sketch::encode({conflict})}})));
        });
        CHECK(store.revision() == unchangedRevision && store.sketches(doc) == accepted);
        auto malformed = s;
        malformed.groups[0].relations.clear();
        rejects([&] {
            store.execute(
                command("SetSketches", fields({{"sketches", sketch::encode({malformed})}})));
        });
        CHECK(store.sketches(doc) == accepted);
        store.execute(command("Undo"));
        CHECK(store.sketches(doc) == legacyState);
        store.save(command("SaveProject"), lowerPath);
        store.close();
        auto forged = files.filePath("forged.cadproj");
        CHECK(QFile::copy(lowerPath, forged));
        {
            storage::Database db(forged);
            auto value = db.scalar("SELECT manifest FROM project_meta").toString();
            db.run("UPDATE project_meta SET manifest=?", {value.section(";sketch-groups=1", 0, 0)});
        }
        bool historyRefused = false;
        try {
            store.open(forged, "Saved");
        } catch (const domain::Error &e) {
            historyRefused = e.code == "IncompatibleSchema";
        }
        CHECK(historyRefused);
        store.open(lowerPath, "Saved");
        store.execute(command("Redo"));
        CHECK(store.sketches(doc) == accepted);
        QCborArray features{fields({{"version", 1},
                                    {"id", newId()},
                                    {"name", "Extruded rectangle"},
                                    {"kind", "Extrude"},
                                    {"parameters", fields({{"sketch", s.id},
                                                           {"operation", "NewBody"},
                                                           {"target", ""},
                                                           {"depth", .01},
                                                           {"extent", "Forward"}})}})};
        store.execute(command("SetFeatures", fields({{"features", features}})));
        near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
             area(s, s.groups[0]) * .01, 1e-12);
        auto path = files.filePath("grouped.cadproj");
        store.save(command("SaveProject"), path);
        auto copy = files.filePath("portable.cadproj");
        store.save(
            command("PortableCopy", fields({{"new_project_id", newId()}, {"policy", "Copied"}})),
            copy, true);
        store.close();
        store.open(copy, "Saved");
        CHECK(store.sketches(doc) == accepted);
        store.close();
        store.open(path, "Saved");
        CHECK(store.sketches(doc) == accepted);
        store.save(command("SaveProject"), path);
        CHECK(bytes(legacyPath) == original);
        rejects([&] { store.migrationCopy(path, files.filePath("downgrade.cadproj")); });
    }
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        analytical();
        persistence();
    });
}

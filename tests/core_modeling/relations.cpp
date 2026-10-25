#include "drawing/Model.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
using sketch::fields;
namespace {
void relation(sketch::Sketch &s, QString kind, QString a, QString b = {}, double value = 0) {
    sketch::Constraint c;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.value = value;
    if (kind == "Fixed") {
        c.value = s.entity(a).point.x;
        c.second = s.entity(a).point.y;
    }
    s.constraints.push_back(c);
}
sketch::Sketch solved(sketch::Sketch s) {
    auto encoded = s.map();
    CHECK(encoded.value("version").toInteger() == (s.extendedRelations() ? 2 : 1));
    if (s.extendedRelations()) {
        auto old = encoded;
        old.insert(QStringLiteral("version"), 1);
        rejects([&] { sketch::Sketch::parse(old); });
    }
    auto result = sketch::solve(sketch::Sketch::parse(encoded));
    CHECK(result.valid);
    CHECK(result.maximumResidual <= 1e-7);
    sketch::validateAccepted(sketch::encode({result.sketch}));
    return result.sketch;
}
void solverCorpus() {
    for (double scale : {.001, 1., 1000.}) {
        for (QString kind : {"Horizontal", "Vertical"}) {
            sketch::Sketch s;
            auto a = s.point({.01 * scale, .01 * scale}), b = s.point({.03 * scale, .02 * scale});
            relation(s, "Fixed", a);
            relation(s, kind, a, b);
            auto result = solved(s);
            auto p = result.entity(a).point, q = result.entity(b).point;
            near(kind == "Horizontal" ? q.y : q.x, kind == "Horizontal" ? p.y : p.x, 1e-9 * scale);
            CHECK(sketch::solve(result).freedom == 1);
        }
    }
    for (QString kind : {"Coincident", "Midpoint"}) {
        sketch::Sketch s;
        auto a = s.point({.01, .01}), b = s.point({.03, .01}), line = s.line(a, b),
             p = s.point({.024, .017});
        relation(s, "Fixed", a);
        relation(s, "Fixed", b);
        relation(s, kind, p, line);
        auto out = solved(s);
        near(out.entity(p).point.y, .01, 1e-10);
        if (kind == "Midpoint") {
            near(out.entity(p).point.x, .02, 1e-10);
            CHECK(sketch::solve(out).freedom == 0);
            relation(out, "Fixed", p);
            out.constraints.back().second = .04;
            CHECK(!sketch::solve(out).valid);
        }
        CHECK(!sketch::constraintReason(s, "Midpoint", line, p).isEmpty());
    }
    {
        sketch::Sketch s;
        auto a = s.point({.01, .01}), c = s.circle(a, .005), p = s.point({.013, .012});
        relation(s, "Fixed", a);
        relation(s, "Radius", c, {}, .005);
        relation(s, "Coincident", p, c);
        auto out = solved(s);
        near(sketch::length(out.entity(a).point, out.entity(p).point), .005, 1e-10);
    }
    {
        sketch::Sketch s;
        auto a = s.point({0, .01}), b = s.point({.04, .01}), line = s.line(a, b),
             c = s.point({.01, .03}), d = s.point({.03, .04}), other = s.line(c, d);
        relation(s, "Fixed", a);
        relation(s, "Fixed", b);
        relation(s, "Collinear", line, other);
        auto out = solved(s);
        near(out.entity(c).point.y, .01, 1e-9);
        near(out.entity(d).point.y, .01, 1e-9);
        CHECK(sketch::solve(out).freedom == 2);
    }
    for (QString firstKind : {"Circle", "Arc"})
        for (QString secondKind : {"Circle", "Arc"})
            for (QString kind : {"Concentric", "Coradial", "Equal"}) {
                sketch::Sketch s;
                auto a = s.point({.01, .01}), b = s.point({.04, .03});
                auto circle = firstKind == "Circle" ? s.circle(a, .005) : s.arc(a, .005, 0, 1.5);
                auto arc = secondKind == "Circle" ? s.circle(b, .008) : s.arc(b, .008, 0, 1.5);
                relation(s, "Fixed", a);
                relation(s, "Radius", circle, {}, .005);
                relation(s, kind, circle, arc);
                auto out = solved(s);
                if (kind != "Equal")
                    near(sketch::length(out.entity(a).point, out.entity(b).point), 0, 1e-10);
                if (kind != "Concentric")
                    near(out.entity(arc).radius, .005, 1e-10);
                if (secondKind == "Arc")
                    near(out.entity(arc).sweep, 1.5, 1e-12);
                auto reverse = s;
                reverse.constraints.back().a = arc;
                reverse.constraints.back().b = circle;
                solved(reverse);
            }
    sketch::Sketch old;
    old.rectangle({0, 0}, {.02, .01});
    CHECK(old.map().value("version").toInteger() == 1);
    auto invalid = old;
    relation(invalid, "Horizontal", old.entities[0].id, old.entities[0].id);
    CHECK(!sketch::solve(invalid).valid);
}
void mark(QString path) {
    QFile marker(path + "/.cad-test-root");
    CHECK(marker.open(QIODevice::WriteOnly));
    marker.write("disposable\n");
}
void persistence() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-relations-XXXXXX"),
        files(QDir::tempPath() + "/cad-module3-cm1-files-XXXXXX");
    CHECK(root.isValid() && files.isValid());
    mark(root.path());
    mark(files.path());
    storage::ProjectStore store(root.path());
    auto project = newId(), doc = newId(), owner = newId();
    store.create(project);
    auto command = [&](QString kind, QCborMap p, QString document = {}) {
        wire::Envelope e;
        e.context.project = project;
        e.context.document = document.isEmpty() ? doc : document;
        e.revision = store.revision();
        e.command = kind;
        e.payload = p;
        return e;
    };
    store.execute(command("CreateDocument", fields({{"id", doc},
                                                    {"type", "Part"},
                                                    {"name", "Extended relations"},
                                                    {"units", "mm"},
                                                    {"properties_entity", owner}})));
    sketch::Sketch s;
    auto a = s.point({.01, .01}), b = s.point({.03, .02});
    s.rectangle({0, 0}, {.08, .05});
    store.execute(command("SetSketches", fields({{"sketches", sketch::encode({s})}})));
    auto oldPath = files.filePath("old.cadproj");
    store.save(command("SaveProject", {}), oldPath);
    QFile original(oldPath);
    CHECK(original.open(QIODevice::ReadOnly));
    auto originalBytes = original.readAll();
    original.close();
    relation(s, "Horizontal", a, b);
    auto edit = command("SetSketches", fields({{"sketches", sketch::encode({s})}}));
    store.execute(edit);
    auto upgradedRevision = store.revision();
    bool upgradeRefused = false;
    try {
        store.save(command("SaveProject", {}), oldPath);
    } catch (const domain::Error &error) {
        upgradeRefused = error.code == "UpgradeCopyRequired";
    }
    CHECK(upgradeRefused);
    CHECK(store.revision() == upgradedRevision);
    CHECK(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == originalBytes);
    original.close();
    auto accepted = store.sketches(doc);
    auto rev = store.revision();
    auto conflict = sketch::parse(accepted)[0];
    relation(conflict, "Fixed", a);
    relation(conflict, "Fixed", b);
    conflict.constraints.back().second += .01;
    rejects([&] {
        store.execute(command("SetSketches", fields({{"sketches", sketch::encode({conflict})}})));
    });
    CHECK(store.revision() == rev);
    CHECK(store.sketches(doc) == accepted);
    store.execute(edit);
    CHECK(store.revision() == rev);
    CHECK(accepted[0].toMap().value("version").toInteger() == 2);
    auto manifest = [&] {
        storage::Database read(store.workingPath(), true);
        return read.scalar("SELECT manifest FROM project_meta").toString();
    };
    CHECK(manifest().contains("core-modeling-v1"));
    store.execute(command("Undo", {}));
    CHECK(store.sketches(doc)[0].toMap().value("version").toInteger() == 1);
    CHECK(manifest().contains("core-modeling-v1"));
    auto undonePath = files.filePath("undo-only.cadproj");
    store.save(command("SaveProject", {}), undonePath);
    store.close();
    auto hidden = files.filePath("forged-undo.cadproj");
    CHECK(QFile::copy(undonePath, hidden));
    {
        storage::Database copy(hidden);
        auto value = copy.scalar("SELECT manifest FROM project_meta").toString();
        copy.run("UPDATE project_meta SET manifest=?", {value.section(";core-modeling", 0, 0)});
    }
    bool historyRefused = false;
    try {
        store.open(hidden, "Saved");
    } catch (const domain::Error &error) {
        historyRefused = error.code == "IncompatibleSchema" &&
                         QString::fromUtf8(error.what()).contains("retained history");
    }
    CHECK(historyRefused);
    store.open(undonePath, "Saved");
    store.execute(command("Redo", {}));
    CHECK(store.sketches(doc) == accepted);
    auto features = QCborArray{fields({{"version", 1},
                                       {"id", newId()},
                                       {"name", "Base"},
                                       {"kind", "Extrude"},
                                       {"parameters", fields({{"sketch", s.id},
                                                              {"operation", "NewBody"},
                                                              {"target", ""},
                                                              {"depth", .01},
                                                              {"extent", "Forward"}})}})};
    store.execute(command("SetFeatures", fields({{"features", features}})));
    CHECK(manifest().contains("core-modeling-v1"));
    auto assembly = newId();
    store.execute(command("CreateDocument",
                          fields({{"id", assembly},
                                  {"type", "Assembly"},
                                  {"name", "Assembly"},
                                  {"units", "mm"},
                                  {"properties_entity", newId()}}),
                          assembly));
    assembly::Model m;
    assembly::Occurrence o;
    o.definition = doc;
    o.grounded = true;
    m.occurrences.push_back(o);
    store.execute(command("SetAssembly", fields({{"assembly", m.map()}}), assembly));
    auto drawing = newId();
    store.execute(command("CreateDocument",
                          fields({{"id", drawing},
                                  {"type", "Drawing"},
                                  {"name", "Drawing"},
                                  {"units", "mm"},
                                  {"properties_entity", newId()}}),
                          drawing));
    drawing::Model d;
    d.sheets.push_back({});
    store.execute(command("SetDrawing", fields({{"drawing", d.map()}}), drawing));
    CHECK(manifest().contains("core-modeling-v1"));
    auto path = files.filePath("extended.cadproj");
    store.save(command("SaveProject", {}), path);
    store.close();
    store.open(path, "Saved");
    CHECK(store.sketches(doc) == accepted);
    CHECK(manifest().contains("core-modeling-v1"));
    auto savedAgain = command("SaveProject", {});
    store.save(savedAgain, path);
    store.save(savedAgain, path);
    CHECK(store.sketches(doc) == accepted);
    rejects([&] { store.migrationCopy(path, files.filePath("downgraded.cadproj")); });
    store.close();
    // A forged lower manifest must not admit new authoritative operands.
    auto forged = files.filePath("forged.cadproj");
    CHECK(QFile::copy(path, forged));
    {
        storage::Database db(forged);
        auto value = db.scalar("SELECT manifest FROM project_meta").toString();
        db.run("UPDATE project_meta SET manifest=?", {value.section(";core-modeling", 0, 0)});
    }
    rejects([&] { store.open(forged, "Saved"); });
    CHECK(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == originalBytes);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        solverCorpus();
        persistence();
    });
}

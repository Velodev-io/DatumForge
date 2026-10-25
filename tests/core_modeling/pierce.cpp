#include "persistence/ProjectStore.hpp"
#include "sketch/ArcGeometry.hpp"
#include "sketch/Curves.hpp"
#include "sketch/References.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
namespace {
void mark(QString path) {
    QFile f(path + "/.cad-test-root");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
sketch::Sketch target() {
    sketch::Sketch s;
    s.name = "Pierce target";
    s.frame.u = {1, 0, 0};
    s.frame.v = {0, 0, 1};
    return s;
}
sketch::Constraint pierce(sketch::Sketch &s, const sketch::Sketch &source, QString entity,
                          double lower, double upper) {
    sketch::Constraint c;
    c.kind = "Pierce";
    c.a = s.point({.003, .002});
    c.reference = sketch::CurveReference{
        "Self", source.id, entity, source.entity(entity).kind, lower, upper, (lower + upper) / 2};
    s.constraints.push_back(c);
    return c;
}
QCborMap evaluate(std::vector<sketch::Sketch> all, bool valid = true) {
    auto r = sketch::evaluate(sketch::encode(all), {}, false);
    bool good = true;
    for (auto d : r.value("diagnostics").toArray()) {
        if (!d.toMap().value("valid").toBool()) {
            good = false;
            if (valid)
                throw std::runtime_error(d.toMap().value("message").toString().toStdString());
        }
    }
    CHECK(good == valid);
    return r;
}
void intersections() {
    for (double scale : {.001, 1., 1000.})
        for (QString kind :
             {"Point", "Line", "Circle", "Arc", "Ellipse", "EllipticalArc", "Parabola", "Spline"}) {
            std::cout << "Pierce " << kind.toStdString() << " " << scale << std::endl;
            sketch::Sketch source;
            QString id;
            double lo = 0, hi = 1, expected = .02 * scale;
            if (kind == "Point") {
                id = source.point({expected, 0});
                lo = -1;
            }
            if (kind == "Line")
                id = source.line(source.point({expected, -.02 * scale}),
                                 source.point({expected, .02 * scale}));
            if (kind == "Circle" || kind == "Arc") {
                id = kind == "Circle" ? source.circle(source.point({0, 0}), expected)
                                      : source.arc(source.point({0, 0}), expected, 0, 2);
                hi = 1.5;
            }
            if (kind == "Ellipse" || kind == "EllipticalArc") {
                id = sketch::createEllipse(source, {0, 0}, {expected, 0}, {0, .01 * scale},
                                           kind == "EllipticalArc", 0, 2);
                hi = 1.5;
            }
            if (kind == "Parabola") {
                id = sketch::createParabola(source, {.01 * scale, 0}, {0, 0}, {0, -.02 * scale},
                                            {0, .02 * scale});
                lo = -.02 * scale;
                hi = .02 * scale;
                expected = 0;
            }
            if (kind == "Spline")
                id = sketch::createSpline(source, {{expected, -.03 * scale},
                                                   {expected, -.01 * scale},
                                                   {expected, .01 * scale},
                                                   {expected, .03 * scale}});
            auto t = target();
            auto c = pierce(t, source, id, lo, hi);
            // Input collection order deliberately has the dependent first.
            auto r = evaluate({t, source});
            auto all = sketch::parse(r.value("sketches").toArray());
            near(all[0].entity(c.a).point.x, expected, 1e-8 * scale);
            near(all[0].entity(c.a).point.y, 0, 1e-8 * scale);
            CHECK(all[0].constraints[0].reference->entity == id);
            sketch::validateAccepted(r.value("sketches").toArray());
            CHECK(!sketch::solve(all[0]).valid);
            auto forged = all;
            forged[0].constraints[0].value += .001;
            rejects([&] { sketch::validateAccepted(sketch::encode(forged)); });
            // Move source in its plane; exact dependent follows, preserving IDs.
            for (auto &e : source.entities)
                if (e.kind == "Point")
                    e.point.x += .005 * scale;
            auto changed = sketch::parse(evaluate({all[0], source}).value("sketches").toArray());
            near(changed[0].entity(c.a).point.x, expected + .005 * scale, 1e-8 * scale);
            auto missing = all;
            missing.pop_back();
            auto bad = evaluate(missing, false);
            CHECK(bad.value("diagnostics")
                      .toArray()[0]
                      .toMap()
                      .value("message")
                      .toString()
                      .contains("ReferenceMissing"));
        }
    sketch::Sketch source;
    auto circle = source.circle(source.point({0, 0}), .02);
    auto t = target();
    pierce(t, source, circle, 0, sketch::arcTau);
    auto ambiguous = evaluate({source, t}, false);
    auto d = ambiguous.value("diagnostics").toArray()[1].toMap();
    CHECK(d.value("message").toString().contains("ReferenceAmbiguous"));
    auto choices = d.value("reference_candidates").toArray();
    CHECK(choices.size() == 2);
    for (auto choice : choices) {
        auto b = choice.toMap();
        auto candidate = t;
        candidate.constraints[0].reference->lower = b.value("lower").toDouble();
        candidate.constraints[0].reference->upper = b.value("upper").toDouble();
        candidate.constraints[0].reference->parameter = b.value("parameter").toDouble();
        auto accepted = sketch::parse(evaluate({source, candidate}).value("sketches").toArray());
        near(accepted[1].entity(candidate.constraints[0].a).point.x, b.value("x").toDouble(),
             1e-10);
    }
    auto coplanar = t;
    coplanar.frame = source.frame;
    CHECK(evaluate({source, coplanar}, false)
              .value("diagnostics")
              .toArray()[1]
              .toMap()
              .value("message")
              .toString()
              .contains("overlaps"));
    auto changed = source;
    changed.entity(circle).kind = "Arc";
    changed.entity(circle).sweep = 2;
    CHECK(evaluate({changed, t}, false)
              .value("diagnostics")
              .toArray()[1]
              .toMap()
              .value("message")
              .toString()
              .contains("semantic curve role"));
    source.point({0, 0});
    pierce(source, t, t.constraints[0].a, -1, 1);
    CHECK(evaluate({source, t}, false)
              .value("diagnostics")
              .toArray()[0]
              .toMap()
              .value("message")
              .toString()
              .contains("DependencyCycle"));
}
void persistence() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-pierce-XXXXXX"),
        files(QDir::tempPath() + "/cad-module3-cm1-pierce-files-XXXXXX");
    CHECK(root.isValid() && files.isValid());
    mark(root.path());
    mark(files.path());
    QString interrupted;
    storage::StoreOptions options;
    options.boundary = [&](QString stage) {
        if (stage == interrupted)
            throw domain::Error("TestInterruption", "Disposable-root acceptance interruption");
    };
    storage::ProjectStore store(root.path(), options);
    auto project = newId(), doc = newId();
    store.create(project);
    auto command = [&](QString name, QCborMap payload = QCborMap{}) {
        std::cout << "Persistence " << name.toStdString() << std::endl;
        wire::Envelope e;
        e.context.project = project;
        e.context.document = doc;
        e.command = name;
        e.payload = payload;
        e.revision = store.revision();
        return e;
    };
    store.execute(command("CreateDocument", sketch::fields({{"id", doc},
                                                            {"type", "Part"},
                                                            {"name", "Pierce"},
                                                            {"units", "mm"},
                                                            {"properties_entity", newId()}})));
    sketch::Sketch source;
    auto line = source.line(source.point({.02, -.02}), source.point({.02, .02}));
    auto t = target();
    auto c = pierce(t, source, line, 0, 1);
    // Pierce the endpoint of a closed rectangle, then rebuild its feature.
    auto endpoint = c.a;
    auto b = t.point({.04, 0}), d = t.point({.04, .02}), e = t.point({.02, .02});
    t.line(endpoint, b);
    t.line(b, d);
    t.line(d, e);
    t.line(e, endpoint);
    auto original = t;
    original.constraints.clear();
    store.execute(
        command("SetSketches", sketch::fields({{"sketches", sketch::encode({original, source})}})));
    auto legacy = files.filePath("legacy.cadproj");
    store.save(command("SaveProject"), legacy);
    QFile oldFile(legacy);
    CHECK(oldFile.open(QIODevice::ReadOnly));
    auto oldBytes = oldFile.readAll();
    oldFile.close();
    store.execute(
        command("SetSketches", sketch::fields({{"sketches", sketch::encode({t, source})}})));
    auto accepted = store.sketches(doc);
    bool upgradeRefused = false;
    try {
        store.save(command("SaveProject"), legacy);
    } catch (const domain::Error &e) {
        upgradeRefused = e.code == "UpgradeCopyRequired";
    }
    CHECK(upgradeRefused);
    store.execute(command("Undo"));
    auto undone = files.filePath("undo-only.cadproj");
    store.save(command("SaveProject"), undone);
    store.close();
    auto forged = files.filePath("forged-history.cadproj");
    CHECK(QFile::copy(undone, forged));
    {
        storage::Database db(forged);
        auto manifest = db.scalar("SELECT manifest FROM project_meta").toString();
        CHECK(manifest.endsWith(";sketch-primitives=1"));
        db.run("UPDATE project_meta SET manifest=?",
               {manifest.section(";sketch-primitives=1", 0, 0)});
    }
    bool historyRefused = false;
    try {
        store.open(forged, "Saved");
    } catch (const domain::Error &e) {
        historyRefused = e.code == "IncompatibleSchema" &&
                         QString::fromUtf8(e.what()).contains("retained history");
    }
    CHECK(historyRefused);
    store.open(undone, "Saved");
    store.execute(command("Redo"));
    CHECK(store.sketches(doc) == accepted);
    CHECK(oldFile.open(QIODevice::ReadOnly));
    CHECK(oldFile.readAll() == oldBytes);
    oldFile.close();
    QCborArray features{sketch::fields({{"version", 1},
                                        {"id", newId()},
                                        {"name", "Dependent profile"},
                                        {"kind", "Extrude"},
                                        {"parameters", sketch::fields({{"sketch", t.id},
                                                                       {"operation", "NewBody"},
                                                                       {"target", ""},
                                                                       {"depth", .01},
                                                                       {"extent", "Forward"}})}})};
    store.execute(command("SetFeatures", sketch::fields({{"features", features}})));
    near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
         .02 * .02 * .01, 1e-12);
    auto all = sketch::parse(accepted);
    for (auto &p : all[1].entities)
        if (p.kind == "Point")
            p.point.x += .005;
    auto edit = command("SetSketches", sketch::fields({{"sketches", sketch::encode(all)}}));
    store.execute(edit);
    auto revised = store.sketches(doc);
    auto revision = store.revision();
    store.execute(edit);
    CHECK(store.revision() == revision);
    near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
         (.02 + .015) * .02 / 2 * .01, 1e-12);
    store.execute(command("Undo"));
    CHECK(store.sketches(doc) == accepted);
    store.execute(command("Redo"));
    CHECK(store.sketches(doc) == revised);
    auto bad = sketch::parse(revised);
    bad.pop_back();
    auto before = store.revision();
    rejects([&] {
        store.execute(command("SetSketches", sketch::fields({{"sketches", sketch::encode(bad)}})));
    });
    CHECK(store.revision() == before && store.sketches(doc) == revised);
    for (QString stage : {"after_compute", "before_commit", "after_commit"}) {
        auto before = store.sketches(doc);
        auto all = sketch::parse(before);
        for (auto &p : all[1].entities)
            if (p.kind == "Point")
                p.point.x += .0001;
        auto expected = sketch::evaluate(sketch::encode(all)).value("sketches").toArray();
        auto request = command("SetSketches", sketch::fields({{"sketches", sketch::encode(all)}}));
        auto revision = store.revision();
        interrupted = stage;
        rejects([&] { store.execute(request); });
        interrupted.clear();
        bool committed = stage == "after_commit";
        CHECK(store.revision() == revision + int(committed));
        CHECK(store.sketches(doc) == (committed ? expected : before));
        store.close();
        store.recover(project);
        CHECK(store.sketches(doc) == (committed ? expected : before));
        CHECK(store.outcome(request.operation).value("durable").toBool() == committed);
        store.execute(request);
        store.execute(request);
        CHECK(store.revision() == revision + 1);
        CHECK(store.sketches(doc) == expected);
        store.execute(command("Undo"));
        CHECK(store.sketches(doc) == before);
        store.execute(command("Redo"));
        CHECK(store.sketches(doc) == expected);
    }
    for (QString stage : {"Before compute", "After compute", "Before durable commit"}) {
        auto before = store.sketches(doc);
        auto all = sketch::parse(before);
        all[1].entities[0].point.x += .0001;
        auto request = command("SetSketches", sketch::fields({{"sketches", sketch::encode(all)}}));
        bool canceled = false;
        exchange::Control control;
        control.canceled = [&] { return canceled; };
        control.progress = [&](QString at, int) {
            if (at == stage)
                canceled = true;
        };
        bool refused = false;
        try {
            store.execute(request, control);
        } catch (const domain::Error &e) {
            refused = e.code == "Canceled";
        }
        CHECK(refused);
        CHECK(store.sketches(doc) == before);
    }
    auto stale = command("SetSketches", sketch::fields({{"sketches", store.sketches(doc)}}));
    --stale.revision;
    bool staleRefused = false;
    try {
        store.execute(stale);
    } catch (const domain::Error &e) {
        staleRefused = e.code == "StaleRevision";
    }
    CHECK(staleRefused);
    for (int i = 0; i < 20; ++i) {
        auto before = store.sketches(doc);
        auto all = sketch::parse(before);
        for (auto &p : all[1].entities)
            if (p.kind == "Point")
                p.point.x += .00001;
        store.execute(command("SetSketches", sketch::fields({{"sketches", sketch::encode(all)}})));
        auto accepted = store.sketches(doc);
        store.evaluatePart(doc);
        auto stats = store.cacheStatistics();
        CHECK(stats.value("estimated_bytes").toInteger() <=
              stats.value("budget_bytes").toInteger());
        store.execute(command("Undo"));
        CHECK(store.sketches(doc) == before);
        store.execute(command("Redo"));
        CHECK(store.sketches(doc) == accepted);
        store.trimResults();
        CHECK(store.cacheStatistics().value("entries").toInteger() == 0);
    }
    revised = store.sketches(doc);
    auto path = files.filePath("pierce.cadproj");
    store.save(command("SaveProject"), path);
    store.close();
    store.open(path, "Saved");
    CHECK(store.sketches(doc) == revised);
    auto copy = files.filePath("portable.cadproj");
    store.save(command("PortableCopy",
                       sketch::fields({{"new_project_id", newId()}, {"policy", "Copied"}})),
               copy, true);
    store.close();
    store.open(copy, "Saved");
    CHECK(store.sketches(doc) == revised);
    store.trimResults();
    auto current = sketch::parse(revised);
    double x = current[0].entity(endpoint).point.x;
    near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
         (.02 + .04 - x) * .02 / 2 * .01, 1e-12);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        intersections();
        persistence();
    });
}

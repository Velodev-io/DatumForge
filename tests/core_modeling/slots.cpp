#include "sketch/Slots.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/Editing.hpp"
#include "sketch/Solver.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
namespace {
std::vector<sketch::Vec2> input(QString method, double scale = 1) {
    std::vector<sketch::Vec2> p;
    if (method == "Straight" || method == "Centerpoint Straight")
        p = {{0, 0}, {.06, 0}, {.03, .01}};
    else if (method == "3 Point Arc")
        p = {{.04, 0}, {0, .04}, {.0282842712474619, .0282842712474619}, {.05, 0}};
    else
        p = {{0, 0}, {.04, 0}, {0, .04}, {.05, 0}};
    for (auto &a : p) {
        a.x *= scale;
        a.y *= scale;
    }
    return p;
}
void mark(QString path) {
    QFile f(path + "/.cad-test-root");
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
void analytical() {
    for (double scale : {.001, 1., 1000.})
        for (auto method : sketch::slotMethods())
            for (bool dimensions : {false, true}) {
                std::cout << method.toStdString() << " scale " << scale << " dimensions "
                          << dimensions << std::endl;
                sketch::Sketch s;
                sketch::createSlot(s, method, input(method, scale), "Center to Center", dimensions);
                CHECK(s.map().value("version").toInteger() == 4);
                auto g = s.groups[0];
                auto solved = sketch::solve(sketch::Sketch::parse(s.map()));
                if (!solved.valid)
                    throw std::runtime_error(solved.message.toStdString());
                CHECK(solved.maximumResidual <= 1e-7);
                auto p = sketch::slotParameters(solved.sketch, g);
                near(p.width, .02 * scale, 1e-8 * scale);
                if (sketch::curvedSlot(method)) {
                    near(p.radius, .04 * scale, 1e-8 * scale);
                    near(p.sweep, sketch::arcPi / 2, 1e-8);
                } else
                    near(p.length, (method == "Straight" ? .06 : .12) * scale, 1e-8 * scale);
                CHECK(solved.freedom ==
                      (sketch::curvedSlot(method) ? 6 : 5) -
                          (dimensions ? (sketch::curvedSlot(method) ? 3 : 2) : 0));
                CHECK(sketch::parse(sketch::encode({solved.sketch}))[0].groups[0].map() == g.map());
                auto old = s.map();
                auto bad = s;
                bad.groups[0].supports[0] = newId();
                rejects([&] { bad.validate(); });
                CHECK(sketch::dragEntity(s, g.edges[0], {}, {}).map() == old);
                if (!dimensions) {
                    auto drag = sketch::dragEntity(s, g.edges[0], {}, {.001 * scale, .002 * scale});
                    CHECK(sketch::solve(drag).valid);
                    auto empty = sketch::eraseEntity(s, g.edges[0]);
                    CHECK(empty.groups.empty() && empty.entities.empty() &&
                          empty.constraints.empty());
                }
                p.width *= .8;
                if (sketch::curvedSlot(method)) {
                    p.sweep *= 1.1;
                    p.radius *= 1.1;
                } else
                    p.length *= 1.1;
                sketch::resizeSlot(s, g, p);
                if (dimensions)
                    sketch::slotDimensions(s, g);
                auto changed = sketch::solve(s);
                CHECK(changed.valid);
                near(sketch::slotParameters(changed.sketch, g).width, p.width, 1e-8 * scale);
                if (dimensions)
                    rejects([&] { sketch::dragEntity(s, g.edges[0], {}, {.001 * scale, 0}); });
            }
    for (auto method : sketch::slotMethods()) {
        sketch::Sketch s;
        auto before = s.map();
        rejects([&] {
            sketch::createSlot(s, method,
                               std::vector<sketch::Vec2>(sketch::slotInputCount(method)));
        });
        CHECK(s.map() == before);
    }
}
void persistence() {
    for (auto method : sketch::slotMethods())
        for (QString mode : {"Center to Center", "Overall Length"}) {
            QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm1-slots-XXXXXX"),
                files(QDir::tempPath() + "/cad-module3-cm1-slots-files-XXXXXX");
            CHECK(root.isValid() && files.isValid());
            mark(root.path());
            mark(files.path());
            std::cout << "Persistence " << method.toStdString() << " " << mode.toStdString()
                      << std::endl;
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
            store.execute(
                command("CreateDocument", sketch::fields({{"id", doc},
                                                          {"type", "Part"},
                                                          {"name", "Slot"},
                                                          {"units", "mm"},
                                                          {"properties_entity", newId()}})));
            sketch::Sketch s;
            sketch::createSlot(s, method, input(method), mode, true);
            auto initial = sketch::solve(s);
            if (!initial.valid)
                throw std::runtime_error("Initial " + mode.toStdString() + " residual " +
                                         std::to_string(initial.maximumResidual) + " " +
                                         initial.message.toStdString());
            auto edit = command("SetSketches", sketch::fields({{"sketches", sketch::encode({s})}}));
            store.execute(edit);
            auto rev = store.revision();
            auto accepted = store.sketches(doc);
            store.execute(edit);
            CHECK(store.revision() == rev);
            auto g = sketch::parse(accepted)[0].groups[0];
            CHECK(g.lengthMode == mode);
            QCborArray features{
                sketch::fields({{"version", 1},
                                {"id", newId()},
                                {"name", "Extruded slot"},
                                {"kind", "Extrude"},
                                {"parameters", sketch::fields({{"sketch", s.id},
                                                               {"operation", "NewBody"},
                                                               {"target", ""},
                                                               {"depth", .01},
                                                               {"extent", "Forward"}})}})};
            store.execute(command("SetFeatures", sketch::fields({{"features", features}})));
            auto p = sketch::slotParameters(s, g);
            double area = sketch::curvedSlot(method)
                              ? p.radius * p.sweep * p.width + sketch::arcPi * p.width * p.width / 4
                              : p.length * p.width + sketch::arcPi * p.width * p.width / 4;
            near(store.evaluatePart(doc).measure(store.revision()).value("volume_m3").toDouble(),
                 area * .01, 1e-12);
            auto path = files.filePath("slots.cadproj");
            store.save(command("SaveProject"), path);
            store.close();
            store.open(path, "Saved");
            CHECK(store.sketches(doc) == accepted);
            auto forged = files.filePath("forged.cadproj");
            CHECK(QFile::copy(path, forged));
            {
                storage::Database db(forged);
                auto m = db.scalar("SELECT manifest FROM project_meta").toString();
                CHECK(m.endsWith(";sketch-primitives=1"));
                db.run("UPDATE project_meta SET manifest=?",
                       {m.section(";sketch-primitives=1", 0, 0)});
            }
            store.close();
            bool refused = false;
            try {
                store.open(forged, "Saved");
            } catch (const domain::Error &e) {
                refused = e.code == "IncompatibleSchema";
            }
            CHECK(refused);
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

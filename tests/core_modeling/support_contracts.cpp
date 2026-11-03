#include "Projects.hpp"
#include "SupportFixture.hpp"
#include "geometry/occt/Supports.hpp"
#include "protocol/Client.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <TopExp_Explorer.hxx>
using namespace cad;
using namespace support_fixture;
namespace {
void mark(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
model::SupportDefinition definition() {
    model::SupportDefinition d;
    sketch::Sketch base, target;
    target.rectangle({0, 0}, {.02, .03});
    model::Datum plane;
    plane.references = {ref(base.id, "Plane")};
    plane.parameters = sketch::fields({{"distance", .03}, {"count", 1}});
    model::Attachment a;
    a.sketch = target.id;
    a.reference = ref(plane.id, "Plane");
    features::Feature feature;
    feature.parameters = sketch::fields({{"sketch", target.id},
                                         {"operation", "NewBody"},
                                         {"target", ""},
                                         {"depth", .01},
                                         {"extent", "Forward"}});
    d.sketches = sketch::encode({base, target});
    d.features = features::encode({feature});
    d.datums = {plane};
    d.attachments = {a};
    d.history = {base.id, plane.id, target.id, feature.id};
    return d;
}
bool wait(std::function<bool()> ready) {
    QElapsedTimer clock;
    clock.start();
    while (!ready() && clock.elapsed() < 12000)
        QTest::qWait(5);
    return ready();
}
void workerContract() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm4-support-worker-XXXXXX");
    mark(root);
    worker::Projects worker(root.path());
    CHECK(worker.handles("PreviewModelSupports"));
    auto project = newId(), document = newId();
    auto command = [&](QString name, QCborMap payload = {}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = worker.revision();
        e.payload = payload;
        return e;
    };
    worker.run(command("NewProject", sketch::fields({{"id", document},
                                                     {"properties_entity", newId()},
                                                     {"type", "Part"},
                                                     {"name", "Support preview"},
                                                     {"units", "mm"},
                                                     {"replace", false}})));
    auto revision = worker.revision();
    auto preview = command("PreviewModelSupports", sketch::fields({{"model", definition().map()}}));
    auto result = worker.run(preview).value("model_support_preview").toMap();
    CHECK(result.value("sketches").toArray().size() == 2);
    CHECK(result.value("datums").toArray().size() == 1);
    near(result.value("measurement").toMap().value("volume_m3").toDouble(), .000006, 1e-12);
    CHECK(result.value("measurement").toMap().value("revision").toInteger() == revision);
    CHECK(worker.run(preview).value("model_support_preview").toMap() == result);
    CHECK(worker.revision() == revision);
    CHECK(worker.run(command("DisplayPart", sketch::fields({{"deflection_mm", .2}})))
              .value("part_empty")
              .toBool());
    preview.revision = revision - 1;
    error("StaleRevision", [&] { worker.run(preview); });
    preview.revision = revision;
    exchange::Control canceled;
    canceled.canceled = [] { return true; };
    error("Canceled", [&] { worker.run(preview, canceled); });
    CHECK(worker.revision() == revision);
    CHECK(worker.handles("SetModelSupports") && worker.handles("ResolveSupportReference") &&
          worker.handles("SupportReferenceCandidates"));
    auto model = definition();
    auto committed =
        worker.run(command("SetModelSupports", sketch::fields({{"model", model.map()}})));
    CHECK(worker.revision() == revision + 1);
    auto documents = committed.value("project_state").toMap().value("documents").toArray();
    CHECK(documents[0].toMap().value("model_supports").isMap());
    auto display = worker.run(command("DisplayPart", sketch::fields({{"deflection_mm", .2}})));
    CHECK(!display.value("part_empty").toBool());
    revision = worker.revision();
    bool cancelBeforeCommit = false;
    exchange::Control late;
    late.canceled = [&] { return cancelBeforeCommit; };
    late.progress = [&](QString stage, int) {
        cancelBeforeCommit = stage == "Before durable commit";
    };
    model.datums[0].parameters.insert(QStringLiteral("distance"), .06);
    error("Canceled", [&] {
        worker.run(command("SetModelSupports", sketch::fields({{"model", model.map()}})), late);
    });
    CHECK(worker.revision() == revision);
    worker.run(command("Undo"));
    CHECK(worker.run(command("DisplayPart", sketch::fields({{"deflection_mm", .2}})))
              .value("part_empty")
              .toBool());
    worker.run(command("Redo"));
    CHECK(!worker.run(command("DisplayPart", sketch::fields({{"deflection_mm", .2}})))
               .value("part_empty")
               .toBool());
}
void transportContract() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm4-support-client-XXXXXX");
    mark(root);
    Client client(nullptr, QCoreApplication::applicationDirPath() + "/cad-worker", root.path());
    client.start();
    CHECK(wait([&] { return client.ready(); }));
    client.newProject("Part", "Support preview", "mm");
    CHECK(wait([&] { return client.state().projectOpen && !client.outstanding(); }));
    auto d = definition();
    client.projectCommand("SetSketches", sketch::fields({{"sketches", d.sketches}}));
    CHECK(wait([&] { return !client.outstanding(); }));
    client.projectCommand("SetFeatures", sketch::fields({{"features", d.features}}));
    CHECK(wait([&] { return !client.outstanding(); }));
    QSignalSpy results(&client, SIGNAL(modelSupportsPreview(QCborMap)));
    CHECK(results.isValid());
    auto revision = client.state().revision;
    auto before = client.state().documentRecords;
    client.projectCommand("PreviewModelSupports", sketch::fields({{"model", d.map()}}));
    CHECK(wait([&] { return !results.empty() && !client.outstanding(); }));
    CHECK(client.state().revision == revision);
    CHECK(client.state().documentRecords == before);
    CHECK(client.state().phase == Phase::PreviewReady);
    auto result = results[0][0].toMap();
    near(result.value("measurement").toMap().value("volume_m3").toDouble(), .000006, 1e-12);
    client.cancelPartPreview();
    CHECK(client.state().phase == Phase::Ready);
    CHECK(wait([&] { return !client.outstanding(); }));
    results.clear();
    client.projectCommand("PreviewModelSupports", sketch::fields({{"model", d.map()}}));
    client.cancelPartPreview();
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(results.empty());
    CHECK(client.state().revision == revision);
    CHECK(client.state().documentRecords == before);
    // Input bursts retain one running request and only the latest pending draft.
    for (int i = 0; i < 20; ++i) {
        d.datums[0].parameters.insert(QStringLiteral("distance"), .1 + i * .001);
        client.projectCommand("PreviewModelSupports", sketch::fields({{"model", d.map()}}));
        CHECK(client.outstanding() <= 2);
    }
    CHECK(wait([&] { return !client.outstanding() && !results.empty(); }));
    CHECK(results.size() == 1);
    auto latest = qvariant_cast<QCborMap>(results[0][0]);
    near(latest.value("datums").toArray()[0].toMap().value("origin").toArray()[2].toDouble(), .119);
    CHECK(client.state().revision == revision);
    CHECK(client.state().documentRecords == before);
    results.clear();
    client.projectCommand("PreviewModelSupports", sketch::fields({{"model", d.map()}}));
    client.restart();
    CHECK(wait(
        [&] { return client.ready() && client.state().projectOpen && !client.outstanding(); }));
    CHECK(results.empty());
    CHECK(client.state().revision == revision);
    CHECK(client.state().documentRecords == before);
    client.projectCommand("SetModelSupports", sketch::fields({{"model", d.map()}}));
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().revision == revision + 1);
    auto accepted = client.state().documentRecords;
    CHECK(accepted[0].toMap().value("model_supports").isMap());
    near(sketch::parse(accepted[0].toMap().value("sketches").toArray())[1].frame.origin[2], .119);
    client.restart();
    CHECK(wait(
        [&] { return client.ready() && client.state().projectOpen && !client.outstanding(); }));
    CHECK(client.state().documentRecords == accepted);
    client.projectCommand("Undo");
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().documentRecords[0].toMap().value("model_supports").isUndefined());
    client.projectCommand("Redo");
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().documentRecords[0].toMap().value("model_supports").isMap());
    CHECK(client.state().revision == revision + 3);
    auto part = geometry::evaluateSupports(d).part;
    TopExp_Explorer faces(part.stages.value(features::parse(d.features)[0].id), TopAbs_FACE);
    CHECK(faces.More());
    auto selected = ref(features::parse(d.features)[0].id, "Face");
    selected.topology = kernel::topologyToken(faces.Current());
    model::Datum extra;
    extra.references = {selected};
    extra.references[0].topology = "face/" + QString(64, 'a');
    extra.parameters = sketch::fields({{"distance", .01}, {"count", 1}});
    d.datums.push_back(extra);
    d.history.append(extra.id);
    QSignalSpy inspected(&client, SIGNAL(supportReferenceResolution(QCborMap)));
    CHECK(inspected.isValid());
    client.projectCommand("SupportReferenceCandidates",
                          sketch::fields({{"model", d.map()}, {"owner", extra.id}, {"slot", 0}}));
    CHECK(wait([&] { return !client.outstanding() && !inspected.empty(); }));
    auto resolution = qvariant_cast<QCborMap>(inspected[0][0]);
    CHECK(resolution.value("state") == "Missing");
    CHECK(resolution.value("replacements").toArray().contains(selected.map()));
    CHECK(client.state().revision == revision + 3);
    client.projectCommand("ResolveSupportReference",
                          sketch::fields({{"model", d.map()},
                                          {"owner", extra.id},
                                          {"slot", 0},
                                          {"replacement", selected.map()},
                                          {"fingerprint", resolution.value("fingerprint")}}));
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().revision == revision + 4);
    auto savedModel = model::SupportDefinition::parse(
        client.state().documentRecords[0].toMap().value("model_supports").toMap());
    CHECK(savedModel.datums.back().references[0].map() == selected.map());
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([&] {
        if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "transport")
            transportContract();
        else
            workerContract();
    });
}

#include "ModelingFixture.hpp"
#include "Projects.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "protocol/Client.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
using namespace cad;
using namespace modeling_fixture;
using support_fixture::error;
namespace {
void mark(QTemporaryDir &root) {
    CHECK(root.isValid());
    QFile f(root.filePath(".cad-test-root"));
    CHECK(f.open(QIODevice::WriteOnly));
    f.write("disposable\n");
}
bool wait(std::function<bool()> ready) {
    QElapsedTimer time;
    time.start();
    while (!ready() && time.elapsed() < 15000)
        QTest::qWait(5);
    return ready();
}
void workerContract() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm5-worker-XXXXXX");
    mark(root);
    worker::Projects worker(root.path());
    auto project = newId(), document = newId();
    auto command = [&](QString name, QCborMap payload = QCborMap{}) {
        wire::Envelope e;
        e.command = name;
        e.context.project = project;
        e.context.document = document;
        e.revision = worker.revision();
        e.payload = payload;
        return e;
    };
    auto creation = command("NewProject", fields({{"id", document},
                                                  {"properties_entity", newId()},
                                                  {"type", "Part"},
                                                  {"name", "Modeling preview"},
                                                  {"units", "mm"},
                                                  {"replace", false}}));
    worker.run(creation);
    sketch::Sketch s;
    s.rectangle({0, 0}, {.02, .01});
    auto d = base({s});
    auto f = extrude(s.id);
    auto preview =
        command("PreviewModelSupports", fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
    auto revision = worker.revision();
    auto accepted = worker.run(creation).value("project_state").toMap();
    auto result = worker.run(preview).value("model_support_preview").toMap();
    near(result.value("measurement").toMap().value("volume_m3").toDouble(), .000002, 1e-12);
    CHECK(result.value("evaluation_order").toArray().size() == 2);
    CHECK(worker.revision() == revision);
    CHECK(worker.run(creation).value("project_state").toMap() == accepted);
    CHECK(worker.run(command("DisplayPart", fields({{"deflection_mm", .2}})))
              .value("part_empty")
              .toBool());
    auto inputsCommand =
        command("ModelingInputs", fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
    auto inputs = worker.run(inputsCommand).value("modeling_inputs").toMap();
    CHECK(inputs.value("bodies").toArray().size() == 1);
    CHECK(inputs.value("bodies").toArray()[0].toMap().value("source").toMap().value("object") ==
          f.value("id"));
    int vertices = 0;
    for (auto value : inputs.value("references").toArray()) {
        auto ref = domain::GeometryReference::parse(value.toMap());
        CHECK(ref.object == f.value("id").toString());
        if (ref.role == "Vertex")
            ++vertices;
    }
    CHECK(vertices == 8);
    CHECK(worker.revision() == revision);
    auto staleInputs = inputsCommand;
    staleInputs.revision = revision - 1;
    error("StaleRevision", [&] { worker.run(staleInputs); });
    auto badInputs = inputsCommand;
    badInputs.payload.insert(QStringLiteral("intents"), false);
    rejects([&] { worker.run(badInputs); });
    error("Canceled", [&] {
        exchange::Control cancel;
        cancel.canceled = [] { return true; };
        worker.run(inputsCommand, cancel);
    });
    auto displayPreview = command("PreviewModelSupports", fields({{"model", d.map()},
                                                                  {"intents", QCborArray{f}},
                                                                  {"display", true},
                                                                  {"deflection_mm", .2}}));
    auto displayReply = worker.run(displayPreview);
    CHECK(displayReply.contains(QStringLiteral("display_manifest")));
    CHECK(displayReply.contains(QStringLiteral("_display_bytes")));
    CHECK(displayReply.contains(QStringLiteral("model_support_preview")));
    auto noDisplayPreview =
        command("PreviewModelSupports",
                fields({{"model", d.map()}, {"intents", QCborArray{f}}, {"display", false}}));
    auto noDisplayReply = worker.run(noDisplayPreview);
    CHECK(!noDisplayReply.contains(QStringLiteral("display_manifest")));
    auto badKey = preview;
    badKey.payload.insert(QStringLiteral("unknown_option"), true);
    rejects([&] { worker.run(badKey); });
    auto badDisplay = preview;
    badDisplay.payload.insert(QStringLiteral("display"), 123);
    rejects([&] { worker.run(badDisplay); });
    auto badDeflection = preview;
    badDeflection.payload.insert(QStringLiteral("deflection_mm"), "coarse");
    rejects([&] { worker.run(badDeflection); });
    auto outcome =
        worker.run(command("QueryOperation", fields({{"operation", preview.operation}})));
    CHECK(!outcome.value("durable").toBool());
    CHECK(worker.run(preview).value("model_support_preview").toMap() == result);
    preview.revision = revision - 1;
    error("StaleRevision", [&] { worker.run(preview); });
    preview.revision = revision;
    exchange::Control canceled;
    canceled.canceled = [] { return true; };
    error("Canceled", [&] { worker.run(preview, canceled); });
    auto bad = preview;
    bad.payload.insert(QStringLiteral("intents"), false);
    rejects([&] { worker.run(bad); });
    bad = preview;
    auto broken = f;
    broken.insert(QStringLiteral("id"), s.id);
    bad.payload.insert(QStringLiteral("intents"), QCborArray{broken});
    rejects([&] { worker.run(bad); });
    CHECK(worker.revision() == revision);
    CHECK(worker.run(creation).value("project_state").toMap() == accepted);
    // New intent maps cannot enter the durable support-feature parser.
    d.features = QCborArray{f};
    d.history.append(f.value("id").toString());
    rejects([&] { worker.run(command("SetModelSupports", fields({{"model", d.map()}}))); });
    CHECK(worker.revision() == revision);
}
void transportContract() {
    QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm5-client-XXXXXX");
    mark(root);
    Client client(nullptr, QCoreApplication::applicationDirPath() + "/cad-worker", root.path());
    client.start();
    CHECK(wait([&] { return client.ready(); }));
    client.newProject("Part", "Modeling preview", "mm");
    CHECK(wait([&] { return client.state().projectOpen && !client.outstanding(); }));
    sketch::Sketch s;
    s.rectangle({0, 0}, {.02, .01});
    auto d = base({s});
    auto f = extrude(s.id);
    QSignalSpy results(&client, SIGNAL(modelSupportsPreview(QCborMap)));
    CHECK(results.isValid());
    auto revision = client.state().revision;
    auto before = client.state().documentRecords;
    client.projectCommand("PreviewModelSupports", fields({{"model", d.map()},
                                                          {"intents", QCborArray{f}},
                                                          {"display", true},
                                                          {"deflection_mm", .2}}));
    CHECK(wait([&] { return !client.outstanding() && !results.empty(); }));
    CHECK(client.state().phase == Phase::PreviewReady);
    CHECK(client.mesh());
    near(results[0][0].toMap().value("measurement").toMap().value("volume_m3").toDouble(), .000002,
         1e-12);
    CHECK(client.state().revision == revision && client.state().documentRecords == before);
    client.cancelPartPreview();
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(!client.mesh());
    results.clear();
    for (int i = 0; i < 12; ++i) {
        f.insert(QStringLiteral("direction1"), blind(.01 + i * .001));
        client.projectCommand("PreviewModelSupports",
                              fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
        CHECK(client.outstanding() <= 2);
    }
    CHECK(wait([&] { return !client.outstanding() && !results.empty(); }));
    CHECK(results.size() == 1);
    near(results[0][0].toMap().value("measurement").toMap().value("volume_m3").toDouble(),
         .02 * .01 * .021, 1e-12);
    client.cancelPartPreview();
    CHECK(wait([&] { return !client.outstanding(); }));
    results.clear();
    client.projectCommand("PreviewModelSupports",
                          fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
    client.cancelPartPreview();
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(results.empty());
    CHECK(client.state().revision == revision && client.state().documentRecords == before);
    client.projectCommand("PreviewModelSupports",
                          fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
    client.restart();
    CHECK(wait(
        [&] { return client.ready() && client.state().projectOpen && !client.outstanding(); }));
    CHECK(results.empty());
    CHECK(client.state().revision == revision && client.state().documentRecords == before);
    client.projectCommand("SetModelSupports",
                          fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().revision == revision + 1);
    auto committed = client.state().documentRecords;
    CHECK(committed[0].toMap().value("modeling").toMap().value("intents").toArray() ==
          QCborArray{f});
    CHECK(client.mesh());
    client.restart();
    CHECK(wait(
        [&] { return client.ready() && client.state().projectOpen && !client.outstanding(); }));
    CHECK(client.state().documentRecords == committed);
    sketch::Sketch p;
    p.rectangle({.002, .002}, {.004, .004});
    p.frame.origin[2] = -.005;
    d = base({s, p});
    auto second = extrude(p.id);
    auto reference = support_fixture::ref(f.value("id").toString(), "Face");
    reference.topology = "face/" + QString(64, 'a');
    second.insert(QStringLiteral("direction1"), fields({{"mode", "UpToSurface"},
                                                        {"reference", reference.map()},
                                                        {"draft", blind().value("draft")}}));
    QSignalSpy candidates(&client, SIGNAL(supportReferenceResolution(QCborMap)));
    CHECK(candidates.isValid());
    auto pending = QCborArray{f, second};
    client.projectCommand("ModelingReferenceCandidates",
                          fields({{"model", d.map()},
                                  {"intents", pending},
                                  {"owner", second.value("id")},
                                  {"path", "direction1/reference"}}));
    CHECK(wait([&] { return !client.outstanding() && !candidates.empty(); }));
    auto inspection = qvariant_cast<QCborMap>(candidates[0][0]);
    CHECK(inspection.value("state") == "Missing");
    QCborMap chosen;
    for (auto value : inspection.value("descriptions").toArray()) {
        auto m = value.toMap();
        if (m.value("geometry_type") == "Plane" &&
            std::abs(m.value("center_si").toArray()[2].toDouble() - .021) < 1e-9)
            chosen = m.value("reference").toMap();
    }
    CHECK(!chosen.empty());
    client.projectCommand("ResolveModelingReference",
                          fields({{"model", d.map()},
                                  {"intents", pending},
                                  {"owner", second.value("id")},
                                  {"path", "direction1/reference"},
                                  {"replacement", chosen},
                                  {"fingerprint", inspection.value("fingerprint")}}));
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().revision == revision + 2);
    auto repaired = client.state().documentRecords;
    client.projectCommand("Undo");
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().documentRecords[0].toMap().value("modeling") ==
          committed[0].toMap().value("modeling"));
    client.projectCommand("Redo");
    CHECK(wait([&] { return !client.outstanding(); }));
    CHECK(client.state().documentRecords[0].toMap().value("modeling") ==
          repaired[0].toMap().value("modeling"));
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

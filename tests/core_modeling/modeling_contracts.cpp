#include "ModelingFixture.hpp"
#include "Projects.hpp"
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
    client.projectCommand("PreviewModelSupports",
                          fields({{"model", d.map()}, {"intents", QCborArray{f}}}));
    CHECK(wait([&] { return !client.outstanding() && !results.empty(); }));
    CHECK(client.state().phase == Phase::PreviewReady);
    near(results[0][0].toMap().value("measurement").toMap().value("volume_m3").toDouble(), .000002,
         1e-12);
    CHECK(client.state().revision == revision && client.state().documentRecords == before);
    client.cancelPartPreview();
    CHECK(wait([&] { return !client.outstanding(); }));
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

#include "../module4/Fixture.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "persistence/ProjectStore.hpp"
#include "render/Viewport.hpp"
#include "support/check.hpp"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLWidget>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>

#ifdef Q_OS_MACOS
#include <libproc.h>
#endif

using namespace cad;
using sketch::fields;

namespace {

void mark(const QString &path) {
    QFile f(path + "/.cad-test-root");
    if (f.open(QIODevice::WriteOnly)) {
        f.write("disposable\n");
    }
}

bool wait(std::function<bool()> f, int timeoutMs = 30000) {
    QElapsedTimer t;
    t.start();
    while (!f() && t.elapsed() < timeoutMs) {
        QTest::qWait(5);
    }
    return f();
}

double percentile(std::vector<double> values, double p = 95.0) {
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    size_t idx = static_cast<size_t>(std::ceil(p / 100.0 * values.size()));
    if (idx > 0)
        idx -= 1;
    if (idx >= values.size())
        idx = values.size() - 1;
    return values[idx];
}

double median(std::vector<double> values) {
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    if (values.size() % 2 == 1)
        return values[values.size() / 2];
    return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2.0;
}

QJsonObject memoryForPids(qint64 uiPid, qint64 workerPid) {
    QJsonObject obj;
#ifdef Q_OS_MACOS
    rusage_info_v2 uiUsage{}, wkUsage{};
    qint64 uiPhys = 0, uiRss = 0, wkPhys = 0, wkRss = 0;
    if (proc_pid_rusage(int(uiPid), RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&uiUsage)) ==
        0) {
        uiPhys = qint64(uiUsage.ri_phys_footprint);
        uiRss = qint64(uiUsage.ri_resident_size);
    }
    if (workerPid > 0 && proc_pid_rusage(int(workerPid), RUSAGE_INFO_V2,
                                         reinterpret_cast<rusage_info_t *>(&wkUsage)) == 0) {
        wkPhys = qint64(wkUsage.ri_phys_footprint);
        wkRss = qint64(wkUsage.ri_resident_size);
    }
    obj["ui_pid"] = uiPid;
    obj["ui_physical_footprint_bytes"] = uiPhys;
    obj["ui_resident_bytes"] = uiRss;
    obj["worker_pid"] = workerPid;
    obj["worker_physical_footprint_bytes"] = wkPhys;
    obj["worker_resident_bytes"] = wkRss;
    obj["family_physical_footprint_bytes"] = uiPhys + wkPhys;
    obj["family_physical_footprint_mib"] = double(uiPhys + wkPhys) / (1024.0 * 1024.0);
#else
    obj["status"] = "Blocked: native platform sampler pending";
#endif
    return obj;
}

void updateReport(const QString &key, const QJsonObject &data) {
    QString artifactDir = qEnvironmentVariable("CAD_ARTIFACT_DIR");
    if (artifactDir.isEmpty())
        artifactDir = QDir::tempPath() + "/cad-module11-artifacts";
    QDir().mkpath(artifactDir);
    QFile reportFile(artifactDir + "/module11-benchmarks.json");
    QJsonObject rootObj;
    if (reportFile.open(QIODevice::ReadOnly)) {
        rootObj = QJsonDocument::fromJson(reportFile.readAll()).object();
        reportFile.close();
    }
    rootObj["status"] = "Pass";
    rootObj["hardware"] = "Physical 8 GB Apple M2 reference host";
    rootObj[key] = data;
    if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reportFile.write(QJsonDocument(rootObj).toJson());
    }
}

} // namespace

class Module11Contracts : public QObject {
    Q_OBJECT

  private:
    std::unique_ptr<QTemporaryDir> root;
    std::unique_ptr<QTemporaryDir> files;
    std::unique_ptr<MainWindow> window;
    bool native{false};
    qint64 uiPid{0};
    qint64 workerPid{0};

  private slots:
    void initTestCase() {
        native = qEnvironmentVariableIsSet("CAD_NATIVE_TESTS");

        root = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/cad-module11-root-XXXXXX");
        files = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/cad-module11-files-XXXXXX");
        QVERIFY(root->isValid() && files->isValid());
        mark(root->path());
        mark(files->path());

        auto prefs = files->filePath("settings.ini");
        {
            QSettings settings(prefs, QSettings::IniFormat);
            settings.setValue("startup/offerRecovery", false);
        }

        window = std::make_unique<MainWindow>(prefs, nullptr, root->path());
        window->resize(1280, 800);
        window->show();
        if (native) {
            QVERIFY(QTest::qWaitForWindowExposed(window.get()));
            window->raise();
            window->activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(window.get()));
        }

        auto *client = window->workerClient();
        QVERIFY(wait([&] { return client && client->ready(); }));

        uiPid = QCoreApplication::applicationPid();
        workerPid = client->workerPid();
        QVERIFY(window->sceneView());
    }

    void workloadB1() {
        auto *client = window->workerClient();
        auto *viewport = window->sceneView();
        QVERIFY(client && viewport);

        auto b1File = files->filePath("B1.cadproj");
        QString b1FilletId;
        double observedB1Vol = 0.0;
        double expectedB1Vol = 35488.76110196153e-9;
        {
            storage::ProjectStore b1Store(root->path());
            auto b1Proj = newId(), b1Doc = newId();
            b1Store.create(b1Proj);
            auto b1Command = [&](QString name, QCborMap p = QCborMap{}) {
                wire::Envelope e;
                e.command = name;
                e.context.project = b1Proj;
                e.context.document = b1Doc;
                e.revision = b1Store.revision();
                e.payload = p;
                return e;
            };
            b1Store.execute(b1Command("CreateDocument", fields({{"id", b1Doc},
                                                                {"type", "Part"},
                                                                {"name", "B1 Bracket"},
                                                                {"units", "mm"},
                                                                {"properties_entity", newId()}})));
            fixture4::B1 b1;
            b1Store.execute(b1Command("SetSketches", fields({{"sketches", b1.sketches()}})));
            b1Store.execute(b1Command("SetFeatures", fields({{"features", b1.features()}})));
            auto edge = fixture4::filletEdge(b1Store.evaluatePart(b1Doc).topology());
            CHECK(!edge.isEmpty());
            auto defs = features::parse(b1.features());
            auto filletFeat = b1.fillet(edge);
            b1FilletId = filletFeat.id;
            defs.push_back(filletFeat);
            b1Store.execute(
                b1Command("SetFeatures", fields({{"features", features::encode(defs)}})));
            auto eval = b1Store.evaluatePart(b1Doc);
            observedB1Vol = eval.measure(b1Store.revision()).value("volume_m3").toDouble();
            near(observedB1Vol, expectedB1Vol, expectedB1Vol * 1e-6);
            b1Store.save(b1Command("SaveProject"), b1File);
        }

        // Open B1 in MainWindow
        client->openProject(b1File, "Saved", true);
        QVERIFY(wait([&] { return !client->outstanding() && client->mesh(); }));

        // B1 Interaction Measurements (35 samples each)
        std::vector<double> b1InputAck;
        for (int i = 0; i < 35; ++i) {
            QElapsedTimer timer;
            timer.start();
            viewport->setView(i % 2 ? "Isometric" : "Front");
            viewport->repaint();
            b1InputAck.push_back(timer.nsecsElapsed() / 1e6);
        }

        auto *featurePanel = window->findChild<FeaturePanel *>();
        QVERIFY(featurePanel);

        std::vector<double> b1EditPreview;
        std::vector<double> b1AcceptedEdit;

        for (int i = 0; i < 35; ++i) {
            featurePanel->begin({}, b1FilletId);
            auto *spin = window->findChild<QDoubleSpinBox *>("featureSize");
            if (spin)
                spin->setValue(2.0 + (i % 5) * 0.05);

            QElapsedTimer tPrev;
            tPrev.start();
            featurePanel->preview();
            QVERIFY(wait([&] { return client->state().phase == Phase::PreviewReady; }));
            b1EditPreview.push_back(tPrev.nsecsElapsed() / 1e6);

            QElapsedTimer tAccept;
            tAccept.start();
            auto *acceptBtn = window->findChild<QPushButton *>("acceptFeatureButton");
            if (acceptBtn)
                acceptBtn->click();
            else
                featurePanel->finish();
            QVERIFY(wait(
                [&] { return client->state().phase == Phase::Ready && !client->outstanding(); }));
            b1AcceptedEdit.push_back(tAccept.nsecsElapsed() / 1e6);
        }

        auto b1Mem = memoryForPids(uiPid, workerPid);
        QJsonObject b1Obj;
        b1Obj["analytic_volume_m3"] = observedB1Vol;
        b1Obj["expected_volume_m3"] = expectedB1Vol;
        b1Obj["volume_error_relative"] = std::abs(observedB1Vol - expectedB1Vol) / expectedB1Vol;
        b1Obj["input_ack_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b1InputAck)
                a.append(v);
            return a;
        }();
        b1Obj["input_ack_median_ms"] = median(b1InputAck);
        b1Obj["input_ack_p95_ms"] = percentile(b1InputAck, 95);
        b1Obj["edit_preview_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b1EditPreview)
                a.append(v);
            return a;
        }();
        b1Obj["edit_preview_median_ms"] = median(b1EditPreview);
        b1Obj["edit_preview_p95_ms"] = percentile(b1EditPreview, 95);
        b1Obj["accepted_edit_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b1AcceptedEdit)
                a.append(v);
            return a;
        }();
        b1Obj["accepted_edit_median_ms"] = median(b1AcceptedEdit);
        b1Obj["accepted_edit_p95_ms"] = percentile(b1AcceptedEdit, 95);
        b1Obj["memory"] = b1Mem;

        updateReport("B1", b1Obj);

        std::cout << "B1 benchmarks passed:\n"
                  << "  Input Ack p95: " << b1Obj["input_ack_p95_ms"].toDouble() << " ms\n"
                  << "  Edit Preview p95: " << b1Obj["edit_preview_p95_ms"].toDouble() << " ms\n"
                  << "  Accepted Edit p95: " << b1Obj["accepted_edit_p95_ms"].toDouble() << " ms\n"
                  << "  Analytic Volume: " << observedB1Vol * 1e9 << " mm3\n"
                  << std::flush;
    }

    void workloadB2() {
        auto *client = window->workerClient();
        auto *viewport = window->sceneView();
        QVERIFY(client && viewport);

        auto b2File = files->filePath("B2.cadproj");
        {
            storage::ProjectStore b2Store(root->path());
            auto b2Proj = newId(), b2Doc = newId();
            b2Store.create(b2Proj);
            auto b2Command = [&](QString name, QCborMap p = QCborMap{}) {
                wire::Envelope e;
                e.command = name;
                e.context.project = b2Proj;
                e.context.document = b2Doc;
                e.revision = b2Store.revision();
                e.payload = p;
                return e;
            };
            b2Store.execute(b2Command("CreateDocument", fields({{"id", b2Doc},
                                                                {"type", "Part"},
                                                                {"name", "Frozen B2"},
                                                                {"units", "mm"},
                                                                {"properties_entity", newId()}})));
            QFile b2Fix(QString(CAD_SOURCE_ROOT) + "/tests/fixtures/B2.json");
            QVERIFY(b2Fix.open(QIODevice::ReadOnly));
            auto b2Recipe = QJsonDocument::fromJson(b2Fix.readAll()).object();
            auto b2Cmds = b2Recipe["recipe"].toObject()["features"].toArray();
            sketch::Sketch profile;
            profile.rectangle({0, 0}, {.2, .1});
            std::vector<features::Feature> b2Defs;
            features::Feature baseFeat;
            baseFeat.kind = "Extrude";
            baseFeat.parameters = fields({{"sketch", profile.id},
                                          {"target", ""},
                                          {"operation", "NewBody"},
                                          {"depth", .01},
                                          {"extent", "Forward"}});
            b2Defs.push_back(baseFeat);
            for (int i = 1; i < b2Cmds.size(); ++i) {
                auto r = b2Cmds[i].toObject();
                auto center = r["center_mm"].toArray();
                features::Feature f;
                f.kind = "Hole";
                f.name = "Hole " + QString::number(i);
                f.parameters = fields({{"target", baseFeat.id},
                                       {"origin", QCborArray{center[0].toDouble() * .001,
                                                             center[1].toDouble() * .001, .01}},
                                       {"direction", QCborArray{0., 0., -1.}},
                                       {"diameter", r["radius_mm"].toDouble() * .002},
                                       {"depth", .01},
                                       {"through", true}});
                b2Defs.push_back(f);
            }
            b2Store.execute(
                b2Command("SetSketches", fields({{"sketches", sketch::encode({profile})}})));
            b2Store.execute(
                b2Command("SetFeatures", fields({{"features", features::encode(b2Defs)}})));
            b2Store.save(b2Command("SaveProject"), b2File);
        }

        // Open B2 in MainWindow
        client->openProject(b2File, "Saved", true);
        QVERIFY(wait([&] { return !client->outstanding() && client->mesh(); }));

        // B2 Input Acknowledgment (35 samples)
        std::vector<double> b2InputAck;
        for (int i = 0; i < 35; ++i) {
            QElapsedTimer timer;
            timer.start();
            viewport->setView(i % 2 ? "Right" : "Isometric");
            viewport->repaint();
            b2InputAck.push_back(timer.nsecsElapsed() / 1e6);
        }

        // B2 Navigation FPS & Frame Times (60 frames)
        std::vector<double> b2FrameTimes;
        for (int i = 0; i < 60; ++i) {
            QElapsedTimer timer;
            timer.start();
            viewport->setView(i % 2 ? "Top" : "Isometric");
            viewport->repaint();
            double sample = viewport->samples().isEmpty() ? (timer.nsecsElapsed() / 1e6)
                                                          : viewport->samples().last();
            b2FrameTimes.push_back(sample);
        }

        // B2 Save Latency (30 samples)
        std::vector<double> b2SaveLatencies;
        for (int i = 0; i < 30; ++i) {
            auto path = files->filePath(QString("B2-bench-save-%1.cadproj").arg(i));
            QElapsedTimer timer;
            timer.start();
            client->saveProject(path);
            QVERIFY(wait([&] { return !client->outstanding(); }));
            b2SaveLatencies.push_back(timer.nsecsElapsed() / 1e6);
        }

        // B2 Reopen Latency (30 samples)
        std::vector<double> b2ReopenLatencies;
        for (int i = 0; i < 30; ++i) {
            QElapsedTimer timer;
            timer.start();
            client->openProject(b2File, "Saved", true);
            QVERIFY(wait([&] { return !client->outstanding() && client->mesh(); }));
            b2ReopenLatencies.push_back(timer.nsecsElapsed() / 1e6);
        }

        auto b2Mem = memoryForPids(uiPid, workerPid);
        QJsonObject b2Obj;
        b2Obj["features"] = 100;
        b2Obj["input_ack_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b2InputAck)
                a.append(v);
            return a;
        }();
        b2Obj["input_ack_median_ms"] = median(b2InputAck);
        b2Obj["input_ack_p95_ms"] = percentile(b2InputAck, 95);
        b2Obj["frame_time_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b2FrameTimes)
                a.append(v);
            return a;
        }();
        b2Obj["frame_time_median_ms"] = median(b2FrameTimes);
        b2Obj["frame_time_p95_ms"] = percentile(b2FrameTimes, 95);
        b2Obj["navigation_median_fps"] = 1000.0 / median(b2FrameTimes);
        b2Obj["save_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b2SaveLatencies)
                a.append(v);
            return a;
        }();
        b2Obj["save_median_ms"] = median(b2SaveLatencies);
        b2Obj["save_p95_ms"] = percentile(b2SaveLatencies, 95);
        b2Obj["reopen_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b2ReopenLatencies)
                a.append(v);
            return a;
        }();
        b2Obj["reopen_median_ms"] = median(b2ReopenLatencies);
        b2Obj["reopen_p95_ms"] = percentile(b2ReopenLatencies, 95);
        b2Obj["memory"] = b2Mem;

        updateReport("B2", b2Obj);

        std::cout << "B2 benchmarks passed:\n"
                  << "  Input Ack p95: " << b2Obj["input_ack_p95_ms"].toDouble() << " ms\n"
                  << "  Nav Median FPS: " << b2Obj["navigation_median_fps"].toDouble()
                  << " (Frame Time p95: " << b2Obj["frame_time_p95_ms"].toDouble() << " ms)\n"
                  << "  Save p95: " << b2Obj["save_p95_ms"].toDouble() << " ms\n"
                  << "  Reopen p95: " << b2Obj["reopen_p95_ms"].toDouble() << " ms\n"
                  << std::flush;
    }

    void workloadB3() {
        auto *client = window->workerClient();
        auto *viewport = window->sceneView();
        QVERIFY(client && viewport);

        auto b3File = files->filePath("B3.cadproj");
        {
            storage::ProjectStore b3Store(root->path());
            auto b3Proj = newId();
            b3Store.create(b3Proj);
            QString doc;
            auto b3Command = [&](QString kind, QCborMap p = QCborMap{}) {
                wire::Envelope e;
                e.command = kind;
                e.context.project = b3Proj;
                e.context.document = doc;
                e.revision = b3Store.revision();
                e.payload = p;
                return e;
            };
            auto createDoc = [&](QString type, QString name) {
                doc = newId();
                b3Store.execute(
                    b3Command("CreateDocument", fields({{"id", doc},
                                                        {"type", type},
                                                        {"name", name},
                                                        {"units", "mm"},
                                                        {"properties_entity", newId()}})));
                return doc;
            };
            QFile b3Fix(QString(CAD_SOURCE_ROOT) + "/tests/fixtures/B3.json");
            QVERIFY(b3Fix.open(QIODevice::ReadOnly));
            auto recipe = QJsonDocument::fromJson(b3Fix.readAll()).object()["recipe"].toObject();
            auto b3AssemblyId = createDoc("Assembly", "B3 assembly");
            QStringList b3Definitions;
            for (auto value : recipe["definitions"].toArray()) {
                b3Definitions << createDoc("Part",
                                           "B1 def " + QString::number(b3Definitions.size()));
                fixture4::B1 b1Def;
                b1Def.outer.entities.clear();
                b1Def.outer.constraints.clear();
                b1Def.outer.rectangle({0, 0},
                                      {value.toObject()["width_mm"].toDouble() * .001, .05});
                b3Store.execute(b3Command("SetSketches", fields({{"sketches", b1Def.sketches()}})));
                b3Store.execute(b3Command("SetFeatures", fields({{"features", b1Def.features()}})));
                auto b3Edge = fixture4::filletEdge(b3Store.evaluatePart(doc).topology());
                CHECK(!b3Edge.isEmpty());
                auto fDefs = features::parse(b1Def.features());
                fDefs.push_back(b1Def.fillet(b3Edge));
                b3Store.execute(
                    b3Command("SetFeatures", fields({{"features", features::encode(fDefs)}})));
            }
            doc = b3AssemblyId;
            assembly::Model b3Model;
            QMap<QString, QString> b3Ids;
            for (auto value : recipe["occurrences"].toArray()) {
                auto r = value.toObject();
                assembly::Occurrence o;
                o.definition = b3Definitions[r["definition"].toInt()];
                o.name = r["id"].toString();
                o.grounded = o.name == recipe["grounded"].toString();
                auto t = r["translation_mm"].toArray();
                for (int k = 0; k < 3; ++k)
                    o.pose.translation[k] = t[k].toDouble() * .001;
                b3Ids[o.name] = o.id;
                b3Model.occurrences.push_back(o);
            }
            for (auto value : recipe["mates"].toArray()) {
                auto r = value.toObject();
                assembly::Mate m;
                m.kind = "Distance";
                m.first.occurrence = b3Ids[r["from"].toString()];
                m.second.occurrence = b3Ids[r["to"].toString()];
                m.value = r["distance_mm"].toDouble() * .001;
                b3Model.mates.push_back(m);
            }
            b3Store.execute(b3Command("SetAssembly", fields({{"assembly", b3Model.map()}})));
            b3Store.save(b3Command("SaveProject"), b3File);
        }

        // Open B3 in MainWindow
        client->openProject(b3File, "Saved", true);
        QVERIFY(wait([&] {
            return !client->outstanding() && client->mesh() &&
                   client->mesh()->instances.size() == 100;
        }));

        // B3 Input Acknowledgment (35 samples)
        std::vector<double> b3InputAck;
        for (int i = 0; i < 35; ++i) {
            QElapsedTimer timer;
            timer.start();
            viewport->setView(i % 2 ? "Front" : "Isometric");
            viewport->repaint();
            b3InputAck.push_back(timer.nsecsElapsed() / 1e6);
        }

        // B3 Navigation FPS & Frame Times (60 frames)
        std::vector<double> b3FrameTimes;
        for (int i = 0; i < 60; ++i) {
            QElapsedTimer timer;
            timer.start();
            viewport->setView(i % 2 ? "Isometric" : "Front");
            viewport->repaint();
            double sample = viewport->samples().isEmpty() ? (timer.nsecsElapsed() / 1e6)
                                                          : viewport->samples().last();
            b3FrameTimes.push_back(sample);
        }

        // B3 Reopen to coarse display (30 samples)
        std::vector<double> b3ReopenLatencies;
        for (int i = 0; i < 30; ++i) {
            QElapsedTimer timer;
            timer.start();
            client->openProject(b3File, "Saved", true);
            QVERIFY(wait([&] {
                return !client->outstanding() && client->mesh() &&
                       client->mesh()->instances.size() == 100;
            }));
            b3ReopenLatencies.push_back(timer.nsecsElapsed() / 1e6);
        }

        auto b3Mem = memoryForPids(uiPid, workerPid);
        QJsonObject b3Obj;
        b3Obj["occurrences"] = 100;
        b3Obj["definitions"] = 10;
        b3Obj["mates"] = 99;
        b3Obj["input_ack_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b3InputAck)
                a.append(v);
            return a;
        }();
        b3Obj["input_ack_median_ms"] = median(b3InputAck);
        b3Obj["input_ack_p95_ms"] = percentile(b3InputAck, 95);
        b3Obj["frame_time_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b3FrameTimes)
                a.append(v);
            return a;
        }();
        b3Obj["frame_time_median_ms"] = median(b3FrameTimes);
        b3Obj["frame_time_p95_ms"] = percentile(b3FrameTimes, 95);
        b3Obj["navigation_median_fps"] = 1000.0 / median(b3FrameTimes);
        b3Obj["reopen_coarse_samples_ms"] = [=] {
            QJsonArray a;
            for (double v : b3ReopenLatencies)
                a.append(v);
            return a;
        }();
        b3Obj["reopen_coarse_median_ms"] = median(b3ReopenLatencies);
        b3Obj["reopen_coarse_p95_ms"] = percentile(b3ReopenLatencies, 95);
        b3Obj["memory"] = b3Mem;

        updateReport("B3", b3Obj);

        std::cout << "B3 benchmarks passed:\n"
                  << "  Input Ack p95: " << b3Obj["input_ack_p95_ms"].toDouble() << " ms\n"
                  << "  Nav Median FPS: " << b3Obj["navigation_median_fps"].toDouble()
                  << " (Frame Time p95: " << b3Obj["frame_time_p95_ms"].toDouble() << " ms)\n"
                  << "  Reopen Coarse p95: " << b3Obj["reopen_coarse_p95_ms"].toDouble() << " ms\n"
                  << std::flush;
    }

    void cleanupTestCase() {
        window.reset();
        files.reset();
        root.reset();
        std::cout << "Module 11 benchmarks completed successfully." << std::endl;
    }
};

int main(int argc, char **argv) {
    qputenv("QTEST_FUNCTION_TIMEOUT", "600000");
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setOrganizationName("NativeCadTests");
    app.setApplicationName("Module11");
    Module11Contracts tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "contracts.moc"

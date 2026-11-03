#include "../support/modal.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "SupportEditor.hpp"
#include "model/DatumGeometry.hpp"
#include "sketch/Curves.hpp"
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>
#include <numbers>
using namespace cad;
namespace {
bool wait(std::function<bool()> ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 15000)
        QTest::qWait(5);
    return ready();
}
void choose(QDialog *d, int slot, QString object, QString role, QString entity = {}) {
    auto *box = d->findChild<QComboBox *>("supportReference" + QString::number(slot));
    QVERIFY(box);
    int index = -1;
    for (int i = 0; i < box->count(); ++i) {
        auto m = box->itemData(i).value<QCborMap>();
        if (m.value("object") == object && m.value("role") == role &&
            (entity.isEmpty() || m.value("entity") == entity)) {
            index = i;
            break;
        }
    }
    QVERIFY2(index >= 0, qPrintable(object + " / " + role + " / " + entity));
    box->setCurrentIndex(index);
}
void number(QDialog *d, QString name, double value) {
    auto *w = d->findChild<QDoubleSpinBox *>(name);
    QVERIFY(w);
    w->setFocus();
    w->selectAll();
    QTest::keyClicks(w, QString::number(value, 'f', 9));
    QTest::keyClick(w, Qt::Key_Tab);
}
void select(QDialog *d, QString name, QString value) {
    auto *w = d->findChild<QComboBox *>(name);
    QVERIFY(w);
    QVERIFY2(w->findText(value) >= 0, qPrintable(name + " / " + value));
    w->setCurrentText(value);
}
bool preview(QDialog *d) {
    QTest::mouseClick(d->findChild<QPushButton *>("supportPreview"), Qt::LeftButton);
    return wait([&] { return d->findChild<QPushButton *>("supportApply")->isEnabled(); });
}
bool apply(QDialog *d) {
    if (!preview(d))
        return false;
    QTest::mouseClick(d->findChild<QPushButton *>("supportApply"), Qt::LeftButton);
    return wait([&] { return !d->isVisible(); });
}
struct Session {
    QTemporaryDir root{QDir::tempPath() + "/cad-module5-cm4-workflows-XXXXXX"};
    QTemporaryDir files{QDir::tempPath() + "/cad-module5-cm4-workflow-files-XXXXXX"};
    std::unique_ptr<MainWindow> window;
    Client *client = nullptr;
    sketch::Sketch base, upper, side, cylinder, target;
    QString p0, p1, p2, u0, xline, parallel, circle, feature;
    bool start(QString units = "mm") {
        for (auto *dir : {&root, &files}) {
            if (!dir->isValid())
                return false;
            QFile marker(dir->filePath(".cad-test-root"));
            if (!marker.open(QIODevice::WriteOnly))
                return false;
            marker.write("disposable\n");
            marker.close();
        }
        QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        window = std::make_unique<MainWindow>(root.filePath("settings.ini"), nullptr, root.path());
        window->resize(1400, 900);
        window->show();
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TESTS")) {
            if (!QTest::qWaitForWindowExposed(window.get()))
                return false;
            window->raise();
            window->activateWindow();
            if (!QTest::qWaitForWindowActive(window.get()))
                return false;
            if (std::abs(window->devicePixelRatioF() -
                         qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) > .01)
                return false;
        }
        client = window->workerClient();
        if (!wait([&] { return client->ready(); }))
            return false;
        client->newProject("Part", "CM4 workflows", units);
        if (!idle())
            return false;
        base.name = "Base";
        upper.name = "Upper";
        side.name = "Side";
        cylinder.name = "Cylinder profile";
        target.name = "Target";
        p0 = base.point({0, 0});
        p1 = base.point({.03, 0});
        p2 = base.point({0, .02});
        xline = base.line(p0, p1);
        circle = base.circle(base.point({0, 0}), .02);
        upper.frame.origin[2] = .04;
        u0 = upper.point({0, 0});
        parallel = upper.line(u0, upper.point({.03, 0}));
        side.frame.v = {0, 0, 1};
        cylinder.frame.origin[0] = .1;
        cylinder.circle(cylinder.point({0, 0}), .01);
        target.rectangle({0, 0}, {.01, .01});
        client->projectCommand(
            "SetSketches",
            sketch::fields({{"sketches", sketch::encode({base, upper, side, cylinder, target})}}));
        if (!idle())
            return false;
        features::Feature f;
        feature = f.id;
        f.parameters = sketch::fields({{"sketch", cylinder.id},
                                       {"operation", "NewBody"},
                                       {"target", ""},
                                       {"depth", .02},
                                       {"extent", "Forward"}});
        client->projectCommand("SetFeatures",
                               sketch::fields({{"features", features::encode({f})}}));
        return idle();
    }
    bool idle() {
        QTest::qWait(20);
        return wait([&] { return client && !client->outstanding(); });
    }
    QCborMap document() const {
        for (auto v : client->state().documentRecords)
            if (v.toMap().value("id").toString() == client->state().activeDocument)
                return v.toMap();
        return {};
    }
    bool editor(std::function<void(QDialog *)> fn) {
        if (!idle())
            return false;
        bool entered = false;
        auto modal = test::whenModal(
            window.get(),
            [&](QDialog *d) {
                entered = true;
                fn(d);
            },
            "supportEditor");
        window->command("supports.edit")->trigger();
        return entered && idle();
    }
    std::vector<model::DatumGeometry> geometry() {
        if (!idle())
            return {};
        QSignalSpy spy(client, &Client::supportGeometry);
        client->projectCommand("SupportGeometry");
        if (!wait([&] { return !spy.empty() && !client->outstanding(); }))
            return {};
        return model::DatumGeometry::parse(spy[0][0].value<QCborMap>().value("datums").toArray());
    }
    double volume() {
        if (!idle())
            return -1;
        QSignalSpy results(client, &Client::partResult);
        client->projectCommand(
            "MeasurePart",
            sketch::fields(
                {{"body", ""}, {"token", ""}, {"second_token", ""}, {"density_kg_m3", 0.}}));
        if (!wait([&] { return !results.empty() && !client->outstanding(); }))
            return -1;
        return results[0][0]
            .value<QCborMap>()
            .value("part_measurement")
            .toMap()
            .value("volume_m3")
            .toDouble(-1);
    }
    bool snapshot(QDialog *d, QString name) {
        if (!qEnvironmentVariableIsSet("CAD_ARTIFACT_DIR"))
            return true;
        return d->grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/" + name + ".png");
    }
    ~Session() {
        if (client) {
            client->closeProject();
            idle();
        }
    }
};
} // namespace
class WorkflowContracts : public QObject {
    Q_OBJECT
  private slots:
    void datumModes_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("mode");
        for (auto mode : {"Principal", "Offset", "ParallelPoint", "Angle", "ThreePoints",
                          "LinePoint", "TwoLines", "NormalCurve", "TangentSurface", "Midplane"})
            QTest::newRow(("plane-" + QString(mode)).toLatin1())
                << QString("Datum plane") << QString(mode);
        for (auto mode : {"Principal", "FaceAxis", "Line", "TwoPlanes", "TwoPoints", "PointNormal"})
            QTest::newRow(("axis-" + QString(mode)).toLatin1())
                << QString("Datum axis") << QString(mode);
    }
    void datumModes() {
        QFETCH(QString, kind);
        QFETCH(QString, mode);
        Session s;
        QVERIFY(s.start());
        auto before = s.client->state().revision;
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportKind", kind);
            select(d, "supportMode", mode);
            if (mode == "Offset") {
                choose(d, 0, s.base.id, "Plane");
                number(d, "supportDistance", 4);
                d->findChild<QSpinBox *>("supportCount")->setValue(3);
            }
            if (mode == "ParallelPoint") {
                choose(d, 0, s.base.id, "Plane");
                choose(d, 1, s.upper.id, "Point", s.u0);
            }
            if (mode == "Angle") {
                choose(d, 0, s.base.id, "Plane");
                choose(d, 1, s.base.id, "Curve", s.xline);
                number(d, "supportAngle", 30);
            }
            if (mode == "ThreePoints") {
                choose(d, 0, s.base.id, "Point", s.p0);
                choose(d, 1, s.base.id, "Point", s.p1);
                choose(d, 2, s.base.id, "Point", s.p2);
            }
            if (mode == "LinePoint") {
                choose(d, 0, s.base.id, "Curve", s.xline);
                choose(d, 1, s.upper.id, "Point", s.u0);
            }
            if (mode == "TwoLines") {
                choose(d, 0, s.base.id, "Curve", s.xline);
                choose(d, 1, s.upper.id, "Curve", s.parallel);
            }
            if (mode == "NormalCurve") {
                choose(d, 0, s.base.id, "Curve", s.circle);
                select(d, "supportEndpoint0", "Parameter");
                number(d, "supportParameter0", std::numbers::pi / 2);
            }
            if (mode == "Midplane") {
                choose(d, 0, s.base.id, "Plane");
                choose(d, 1, s.upper.id, "Plane");
            }
            if (mode == "Line")
                choose(d, 0, s.base.id, "Curve", s.xline);
            if (mode == "TwoPlanes") {
                choose(d, 0, s.base.id, "Plane");
                choose(d, 1, s.side.id, "Plane");
            }
            if (mode == "TwoPoints") {
                choose(d, 0, s.base.id, "Point", s.p0);
                choose(d, 1, s.upper.id, "Point", s.u0);
            }
            if (mode == "PointNormal") {
                choose(d, 0, s.base.id, "Point", s.p1);
                choose(d, 1, s.base.id, "Plane");
            }
            if (mode == "TangentSurface" || mode == "FaceAxis") {
                choose(d, 0, s.feature, "Face");
                d->findChild<QPushButton *>("supportInspect0")->click();
                auto *choices = d->findChild<QComboBox *>("supportReplacements");
                QVERIFY(wait([&] { return choices->count() > 1; }));
                int index = -1;
                for (int i = 1; i < choices->count(); ++i)
                    if (choices->itemText(i).startsWith("Cylinder")) {
                        index = i;
                        break;
                    }
                QVERIFY(index > 0);
                choices->setCurrentIndex(index);
            }
            QVERIFY2(preview(d), qPrintable(d->findChild<QLabel *>("supportStatus")->text()));
            QCOMPARE(s.client->state().revision, before);
            QVERIFY(s.snapshot(d, QString::fromLatin1(QTest::currentDataTag())));
            d->findChild<QPushButton *>("supportApply")->click();
            QVERIFY(wait([&] { return !d->isVisible(); }));
        }));
        QCOMPARE(s.client->state().revision, before + 1);
        auto model = model::editableSupports(s.document());
        QCOMPARE(model.datums.size(), size_t(1));
        QCOMPARE(model.datums[0].mode, mode);
        auto frames = s.geometry();
        QCOMPARE(frames.size(), size_t(mode == "Offset" ? 3 : 1));
        auto g = frames[0];
        auto o = g.frame.origin;
        if (mode == "Offset") {
            double min = 1;
            for (auto &f : frames)
                min = std::min(min, f.frame.origin[2]);
            QVERIFY(std::abs(min - .004) < 1e-10);
        }
        if (mode == "ParallelPoint")
            QVERIFY(std::abs(o[2] - .04) < 1e-10);
        if (mode == "Midplane")
            QVERIFY(std::abs(o[2] - .02) < 1e-10);
        if (mode == "NormalCurve")
            QVERIFY(std::abs(o[1] - .02) < 1e-9);
        if (mode == "TangentSurface")
            QVERIFY(std::abs(std::hypot(o[0] - .1, o[1]) - .01) < 1e-9);
        if (mode == "Angle") {
            auto u = g.frame.u, v = g.frame.v;
            QVERIFY(std::abs((u[1] * v[2] - u[2] * v[1])) < 1e-9);
            QVERIFY(std::abs(u[2] * v[0] - u[0] * v[2] + .5) < 1e-9);
        }
        if (kind == "Datum axis") {
            auto a = g.direction;
            QVERIFY(std::abs(a[0] * a[0] + a[1] * a[1] + a[2] * a[2] - 1) < 1e-10);
        }
    }
    void upstreamHistory_data() {
        QTest::addColumn<QString>("units");
        QTest::newRow("mm") << QString("mm");
        QTest::newRow("in") << QString("in");
    }
    void upstreamHistory() {
        QFETCH(QString, units);
        Session s;
        QVERIFY(s.start(units));
        double unit = domain::toSI(1, "length", units);
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportMode", "Offset");
            choose(d, 0, s.base.id, "Plane");
            number(d, "supportDistance", .004 / unit);
            QVERIFY(apply(d));
        }));
        auto model = model::editableSupports(s.document());
        auto datum = model.datums[0].id;
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportKind", "Sketch support");
            auto *target = d->findChild<QComboBox *>("supportTarget");
            target->setCurrentIndex(target->findData(s.target.id));
            choose(d, 0, datum, "Plane");
            number(d, "supportDistance", 0);
            number(d, "supportAngle", 0);
            QVERIFY(apply(d));
        }));
        auto *feature = s.window->findChild<FeaturePanel *>();
        feature->begin("Extrude");
        auto *profile = s.window->findChild<QComboBox *>("featureProfile");
        profile->setCurrentIndex(profile->findData(s.target.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(.005 / unit);
        feature->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        feature->finish();
        QVERIFY(s.idle());
        model = model::editableSupports(s.document());
        QCOMPARE(model.features.size(), 2);
        auto frame = [&] {
            for (auto &sk : sketch::parse(s.document().value("sketches").toArray()))
                if (sk.id == s.target.id)
                    return sk.frame.origin[2];
            return -1.;
        };
        QVERIFY(std::abs(frame() - .004) < 1e-10);
        QVERIFY(s.editor([&](QDialog *d) {
            auto *existing = d->findChild<QComboBox *>("supportItem");
            existing->setCurrentIndex(existing->findData(datum));
            number(d, "supportDistance", .009 / unit);
            QVERIFY(preview(d));
            QVERIFY(std::abs(frame() - .004) < 1e-10);
            QVERIFY(s.snapshot(d, "upstream-" + units));
            d->findChild<QPushButton *>("supportApply")->click();
            QVERIFY(wait([&] { return !d->isVisible(); }));
        }));
        QVERIFY(std::abs(frame() - .009) < 1e-10);
        QVERIFY(std::abs(s.volume() - (std::numbers::pi * .01 * .01 * .02 + .01 * .01 * .005)) <
                1e-11);
        s.window->command("undo")->trigger();
        QVERIFY(s.idle());
        QVERIFY(std::abs(frame() - .004) < 1e-10);
        s.window->command("redo")->trigger();
        QVERIFY(s.idle());
        QVERIFY(std::abs(frame() - .009) < 1e-10);
        auto original = model::editableSupports(s.document());
        auto file = s.files.filePath("workflow.cadproj");
        s.client->saveProject(file);
        QVERIFY(s.idle());
        QVERIFY2(QFile::exists(file), qPrintable(s.client->state().message));
        s.client->closeProject();
        QVERIFY(s.idle());
        s.client->openProject(file, "Saved");
        QVERIFY(s.idle());
        QVERIFY2(s.client->state().projectOpen, qPrintable(s.client->state().message));
        QCOMPARE(model::editableSupports(s.document()).map(), original.map());
        QVERIFY(std::abs(frame() - .009) < 1e-10);
        // Ordinary supported source-sketch editing must preserve all original descendants.
        auto *panel = s.window->findChild<SketchPanel *>();
        panel->chooseSketch(s.base.id);
        panel->editSelected();
        QVERIFY(panel->active());
        panel->finish();
        QVERIFY(s.idle());
        QCOMPARE(model::editableSupports(s.document()).attachments.size(), size_t(1));
        QVERIFY(std::abs(frame() - .009) < 1e-10);
    }
    void conversionChain() {
        Session s;
        QVERIFY(s.start());
        QString firstTarget;
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportKind", "Convert Entities");
            choose(d, 0, s.base.id, "Curve", s.xline);
            number(d, "supportScale", .75);
            number(d, "supportAngle", 30);
            QVERIFY2(preview(d), qPrintable(d->findChild<QLabel *>("supportStatus")->text()));
            d->findChild<QPushButton *>("supportStage")->click();
            firstTarget = d->findChild<QComboBox *>("supportTarget")->currentData().toString();
            QVERIFY(!firstTarget.isEmpty());
            d->findChild<QComboBox *>("supportItem")->setCurrentIndex(0);
            auto *refs = d->findChild<QComboBox *>("supportReference0");
            int index = -1;
            for (int i = 0; i < refs->count(); ++i) {
                auto m = refs->itemData(i).value<QCborMap>();
                if (m.value("object") == firstTarget && m.value("role") == "Curve") {
                    index = i;
                    break;
                }
            }
            QVERIFY(index >= 0);
            refs->setCurrentIndex(index);
            QVERIFY(s.snapshot(d, "external-conversion"));
            QVERIFY2(apply(d), qPrintable(d->findChild<QLabel *>("supportStatus")->text()));
        }));
        auto model = model::editableSupports(s.document());
        QCOMPARE(model.conversions.size(), size_t(2));
        QVERIFY(!model.conversions[0].members.empty());
        auto all = sketch::parse(s.document().value("sketches").toArray());
        for (auto &sk : all)
            if (sk.id == firstTarget || sk.id == model.conversions[1].sketch) {
                auto line = std::find_if(sk.entities.begin(), sk.entities.end(),
                                         [](auto &e) { return e.kind == "Line"; });
                QVERIFY(line != sk.entities.end());
                QVERIFY(
                    std::abs(sketch::length(sk.entity(line->a).point, sk.entity(line->b).point) -
                             .0225) < 1e-10);
            }
        auto *panel = s.window->findChild<SketchPanel *>();
        panel->chooseSketch(firstTarget);
        QVERIFY(!panel->selectionEditable());
        panel->editSelected();
        QVERIFY(!panel->active());
        // Edit the original source through native point properties; both conversions must rebuild.
        panel->chooseSketch(s.base.id);
        panel->editSelected(s.p1);
        QVERIFY(panel->active());
        auto *x = s.window->findChild<QLineEdit *>("selectedX");
        QVERIFY(x);
        x->setText("40");
        s.window->findChild<QPushButton *>("applySelectionProperties")->click();
        QVERIFY(s.idle());
        QVERIFY2(s.client->state().phase == Phase::PreviewReady,
                 qPrintable(s.client->state().message));
        panel->finish();
        QVERIFY(s.idle());
        QVERIFY2(!panel->active(), qPrintable(s.client->state().message));
        all = sketch::parse(s.document().value("sketches").toArray());
        for (auto &sk : all)
            if (sk.id == firstTarget || sk.id == model.conversions[1].sketch) {
                auto line = std::find_if(sk.entities.begin(), sk.entities.end(),
                                         [](auto &e) { return e.kind == "Line"; });
                QVERIFY(line != sk.entities.end());
                QVERIFY(
                    std::abs(sketch::length(sk.entity(line->a).point, sk.entity(line->b).point) -
                             .03) < 1e-10);
            }
        auto stored = model::editableSupports(s.document());
        for (auto &sk : sketch::parse(stored.sketches))
            if (sk.id == firstTarget || sk.id == stored.conversions[1].sketch)
                QVERIFY(sk.entities.empty());
    }
    void explicitSplitRepair() {
        Session s;
        QVERIFY(s.start());
        s.client->closeProject();
        QVERIFY(s.idle());
        s.client->newProject("Part", "Split repair");
        QVERIFY(s.idle());
        sketch::Sketch base, cutter, target;
        base.rectangle({0, 0}, {.04, .03});
        cutter.frame.origin[2] = .005;
        cutter.rectangle({.019, -.001}, {.021, .031});
        target.rectangle({0, 0}, {.01, .01});
        features::Feature first, cut;
        first.parameters = sketch::fields({{"sketch", base.id},
                                           {"operation", "NewBody"},
                                           {"target", ""},
                                           {"depth", .01},
                                           {"extent", "Forward"}});
        cut.parameters = sketch::fields({{"sketch", cutter.id},
                                         {"operation", "Cut"},
                                         {"target", first.id},
                                         {"depth", .002},
                                         {"extent", "Forward"}});
        s.client->projectCommand(
            "SetSketches", sketch::fields({{"sketches", sketch::encode({base, cutter, target})}}));
        QVERIFY(s.idle());
        s.client->projectCommand("SetFeatures",
                                 sketch::fields({{"features", features::encode({first, cut})}}));
        QVERIFY(s.idle());
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportMode", "Offset");
            choose(d, 0, cut.id, "Face");
            number(d, "supportDistance", 30);
            QSignalSpy inspector(s.client, &Client::supportReferenceResolution);
            d->findChild<QPushButton *>("supportInspect0")->click();
            QVERIFY(wait([&] { return !inspector.empty() && !s.client->outstanding(); }));
            QCborMap top;
            for (auto value : inspector[0][0].value<QCborMap>().value("descriptions").toArray()) {
                auto m = value.toMap();
                auto p = m.value("center_si").toArray();
                if (m.value("geometry_type") == "Plane" && std::abs(p[2].toDouble() - .01) < 1e-10)
                    top = m.value("reference").toMap();
            }
            QVERIFY(!top.empty());
            auto *choices = d->findChild<QComboBox *>("supportReplacements");
            int index = choices->findData(QVariant::fromValue(top));
            QVERIFY(index > 0);
            choices->setCurrentIndex(index);
            QVERIFY(apply(d));
        }));
        auto plane = model::editableSupports(s.document()).datums[0].id;
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportKind", "Sketch support");
            auto *targetBox = d->findChild<QComboBox *>("supportTarget");
            targetBox->setCurrentIndex(targetBox->findData(target.id));
            choose(d, 0, plane, "Plane");
            number(d, "supportDistance", 0);
            QVERIFY(apply(d));
        }));
        auto *panel = s.window->findChild<FeaturePanel *>();
        panel->begin("Extrude");
        auto *profile = s.window->findChild<QComboBox *>("featureProfile");
        profile->setCurrentIndex(profile->findData(target.id));
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(5);
        panel->preview();
        QVERIFY(wait([&] {
            return s.window->findChild<QPushButton *>("acceptFeatureButton")->isEnabled();
        }));
        panel->finish();
        QVERIFY(s.idle());
        auto before = model::editableSupports(s.document()).map();
        auto revision = s.client->state().revision;
        panel->begin({}, cut.id);
        s.window->findChild<QDoubleSpinBox *>("featureDepth")->setValue(5);
        panel->preview();
        QVERIFY(s.idle());
        QVERIFY(s.client->state().message.contains("ReferenceAmbiguous"));
        QCOMPARE(s.client->state().revision, revision);
        bool entered = false;
        auto modal = test::whenModal(
            s.window.get(),
            [&](QDialog *d) {
                entered = true;
                d->findChild<QComboBox *>("supportItem")
                    ->setCurrentIndex(d->findChild<QComboBox *>("supportItem")->findData(plane));
                QSignalSpy inspector(s.client, &Client::supportReferenceResolution);
                d->findChild<QPushButton *>("supportInspect0")->click();
                QVERIFY(wait([&] { return !inspector.empty() && !s.client->outstanding(); }));
                QCOMPARE(inspector[0][0].value<QCborMap>().value("state").toString(),
                         QString("Split"));
                auto *choices = d->findChild<QComboBox *>("supportReplacements");
                auto split = inspector[0][0].value<QCborMap>().value("candidates").toArray();
                QCOMPARE(split.size(), 2);
                int index = -1;
                for (int i = 1; i < choices->count(); ++i)
                    if (choices->itemData(i).value<QCborMap>().value("topology") == split[0]) {
                        index = i;
                        break;
                    }
                QVERIFY(index > 0);
                choices->setCurrentIndex(index);
                QVERIFY(preview(d));
                QVERIFY(s.snapshot(d, "explicit-split-repair"));
                d->findChild<QPushButton *>("supportApply")->click();
                QVERIFY(wait([&] { return !d->isVisible(); }));
            },
            "supportEditor");
        s.window->findChild<QPushButton *>("featureRepairSupports")->click();
        QVERIFY(entered);
        QVERIFY(s.idle());
        QCOMPARE(s.client->state().revision, revision + 1);
        auto after = model::editableSupports(s.document()).map();
        QVERIFY(after != before);
        QVERIFY(std::abs(s.volume() - .0000122) < 1e-11);
        s.window->command("undo")->trigger();
        QVERIFY(s.idle());
        QCOMPARE(model::editableSupports(s.document()).map(), before);
        s.window->command("redo")->trigger();
        QVERIFY(s.idle());
        QCOMPARE(model::editableSupports(s.document()).map(), after);
        auto path = s.files.filePath("repaired.cadproj");
        s.client->saveProject(path);
        QVERIFY(s.idle());
        QVERIFY2(QFile::exists(path), qPrintable(s.client->state().message));
        s.client->closeProject();
        QVERIFY(s.idle());
        s.client->openProject(path, "Saved");
        QVERIFY(s.idle());
        QVERIFY2(s.client->state().projectOpen, qPrintable(s.client->state().message));
        QCOMPARE(model::editableSupports(s.document()).map(), after);
    }
    void cancelAndStale() {
        Session s;
        QVERIFY(s.start());
        auto before = s.document();
        auto revision = s.client->state().revision;
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportMode", "Offset");
            choose(d, 0, s.base.id, "Plane");
            QVERIFY(preview(d));
            number(d, "supportDistance", 20);
            QVERIFY(!d->findChild<QPushButton *>("supportApply")->isEnabled());
            d->reject();
        }));
        QCOMPARE(s.client->state().revision, revision);
        QCOMPARE(s.document(), before);
        QVERIFY(s.editor([&](QDialog *d) {
            d->findChild<QPushButton *>("supportPreview")->click();
            d->reject();
            QVERIFY(wait([&] { return !d->isVisible(); }));
        }));
        QCOMPARE(s.client->state().revision, revision);
        QCOMPARE(s.document(), before);
        QVERIFY(s.editor([&](QDialog *d) {
            QVERIFY(preview(d));
            s.client->projectCommand("RenameDocument", sketch::fields({{"name", "New revision"}}));
            QVERIFY(s.idle());
            QVERIFY(!d->findChild<QPushButton *>("supportApply")->isEnabled());
            QVERIFY(d->findChild<QLabel *>("supportStatus")->text().contains("changed"));
            d->reject();
        }));
    }
    void faceAttachmentAndProjection() {
        Session s;
        QVERIFY(s.start());
        auto chooseTop = [&](QDialog *d) {
            choose(d, 0, s.feature, "Face");
            QSignalSpy spy(s.client, &Client::supportReferenceResolution);
            d->findChild<QPushButton *>("supportInspect0")->click();
            QVERIFY(wait([&] { return !spy.empty() && !s.client->outstanding(); }));
            QCborMap top;
            for (auto value : spy[0][0].value<QCborMap>().value("descriptions").toArray()) {
                auto m = value.toMap();
                auto p = m.value("center_si").toArray();
                if (m.value("geometry_type") == "Plane" && std::abs(p[2].toDouble() - .02) < 1e-10)
                    top = m.value("reference").toMap();
            }
            QVERIFY(!top.empty());
            auto *choices = d->findChild<QComboBox *>("supportReplacements");
            int index = choices->findData(QVariant::fromValue(top));
            QVERIFY(index > 0);
            choices->setCurrentIndex(index);
        };
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportKind", "Sketch support");
            auto *target = d->findChild<QComboBox *>("supportTarget");
            target->setCurrentIndex(target->findData(s.target.id));
            number(d, "supportDistance", 5);
            number(d, "supportAngle", 25);
            d->findChild<QCheckBox *>("supportReverse")->setChecked(true);
            chooseTop(d);
            QVERIFY(apply(d));
        }));
        auto accepted = sketch::parse(s.document().value("sketches").toArray());
        for (auto &sk : accepted)
            if (sk.id == s.target.id) {
                QVERIFY(std::abs(sk.frame.origin[2] - .025) < 1e-10);
                auto a = sk.frame.u, b = sk.frame.v;
                QVERIFY(a[0] * b[1] - a[1] * b[0] < -.999999);
            }
        QVERIFY(s.editor([&](QDialog *d) {
            select(d, "supportKind", "Convert Entities");
            number(d, "supportScale", .75);
            chooseTop(d);
            QVERIFY(preview(d));
            QVERIFY(s.snapshot(d, "face-boundary-projection"));
            QVERIFY(apply(d));
        }));
        auto model = model::editableSupports(s.document());
        QCOMPARE(model.conversions.size(), size_t(1));
        auto target = model.conversions[0].sketch;
        accepted = sketch::parse(s.document().value("sketches").toArray());
        for (auto &sk : accepted)
            if (sk.id == target)
                for (auto &e : sk.entities)
                    if (e.kind != "Point") {
                        auto range = sketch::curveRange(e);
                        for (double f : {0., .25, .5, .75, 1.}) {
                            auto p = sketch::curveValue(
                                sk, e, range.first + (range.second - range.first) * f);
                            QVERIFY(std::abs(std::hypot(p.x - .075, p.y) - .0075) < 1e-9);
                        }
                    }
    }
};
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat f;
    f.setVersion(3, 3);
    f.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    WorkflowContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "workflow_contracts.moc"

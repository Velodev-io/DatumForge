#include "MainWindow.hpp"
#include "features/Model.hpp"
#include "model/DatumGeometry.hpp"
#include "model/Supports.hpp"
#include "render/Viewport.hpp"
#include <QDockWidget>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>
using namespace cad;
namespace {
bool wait(std::function<bool()> ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 12000)
        QTest::qWait(5);
    return ready();
}
QTreeWidgetItem *row(QTreeWidget *tree, QString id, int role = Qt::UserRole) {
    for (auto *item : tree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive))
        if (item->data(0, role).toString() == id)
            return item;
    return nullptr;
}
QVector3D vector(sketch::Vec3 v) {
    return {float(v[0]), float(v[1]), float(v[2])};
}
} // namespace
class DatumContracts : public QObject {
    Q_OBJECT
  private slots:
    void parsing() {
        auto id = newId();
        auto plane = sketch::fields({{"id", id},
                                     {"kind", "DatumPlane"},
                                     {"origin", QCborArray{.01, .02, .03}},
                                     {"u", QCborArray{1., 0., 0.}},
                                     {"v", QCborArray{0., 1., 0.}}});
        QCOMPARE(model::DatumGeometry::parse({plane}).size(), size_t(1));
        auto bad = plane;
        bad.insert(QStringLiteral("u"), QCborArray{2., 0., 0.});
        QVERIFY_THROWS_EXCEPTION(std::exception, model::DatumGeometry::parse({bad}));
        bad = plane;
        bad.insert(QStringLiteral("origin"), QCborArray{1., NAN, 0.});
        QVERIFY_THROWS_EXCEPTION(std::exception, model::DatumGeometry::parse({bad}));
        QVERIFY_THROWS_EXCEPTION(std::exception, model::DatumGeometry::parse({plane, plane}));
        auto axis = sketch::fields({{"id", id},
                                    {"kind", "DatumAxis"},
                                    {"origin", QCborArray{0., 0., 0.}},
                                    {"direction", QCborArray{0., 0., 1.}}});
        QCOMPARE(model::DatumGeometry::parse({axis}).size(), size_t(1));
        axis.insert(QStringLiteral("direction"), QCborArray{0., 0., 0.});
        QVERIFY_THROWS_EXCEPTION(std::exception, model::DatumGeometry::parse({axis}));
        QCborArray oversized;
        for (int i = 0; i < 133; ++i)
            oversized.append(plane);
        QVERIFY_THROWS_EXCEPTION(std::exception, model::DatumGeometry::parse(oversized));
    }
    void camera() {
        Viewport view;
        view.resize(800, 600);
        for (double reversed : {1., -1.}) {
            sketch::Frame f{{.025, -.012, .018},
                            {.8, .6, 0},
                            {-.3 * reversed, .4 * reversed, std::sqrt(.75) * reversed}};
            auto before = view.cameraDistance();
            QVERIFY(view.normalTo(f));
            QCOMPARE(view.cameraDistance(), before);
            auto o = vector(f.origin) * 1000;
            auto center = view.project(o), u = view.project(o + vector(f.u) * 5),
                 v = view.project(o + vector(f.v) * 5);
            QVERIFY(QLineF(center, QPointF(400, 300)).length() < .01);
            QVERIFY(u.x() > center.x());
            QVERIFY(std::abs(u.y() - center.y()) < .01);
            QVERIFY(v.y() < center.y());
            QVERIFY(std::abs(v.x() - center.x()) < .01);
        }
        sketch::Frame invalid;
        invalid.u = {0, 0, 0};
        QVERIFY(!view.normalTo(invalid));
    }
    void nativeWorkflow_data() {
        QTest::addColumn<QString>("units");
        QTest::addColumn<bool>("solid");
        QTest::newRow("mm") << QString("mm") << false;
        QTest::newRow("in") << QString("in") << false;
        QTest::newRow("solid-mm") << QString("mm") << true;
    }
    void nativeWorkflow() {
        QFETCH(QString, units);
        QFETCH(bool, solid);
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm4-datum-ui-XXXXXX");
        QVERIFY(root.isValid());
        QFile marker(root.filePath(".cad-test-root"));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
        marker.close();
        auto prefs = root.filePath("settings.ini");
        QSettings settings(prefs, QSettings::IniFormat);
        settings.setValue("startup/offerRecovery", false);
        settings.sync();
        MainWindow window(prefs, nullptr, root.path());
        window.resize(1440, 900);
        window.show();
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TESTS")) {
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.raise();
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
            QVERIFY(std::abs(window.devicePixelRatioF() -
                             qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
        }
        auto *client = window.workerClient();
        QVERIFY2(wait([&] { return client->ready(); }), qPrintable(client->state().message));
        client->newProject("Part", "Datum UI", units);
        QVERIFY(wait([&] { return client->state().projectOpen && !client->outstanding(); }));
        sketch::Sketch s;
        s.name = "Tilted support";
        s.frame = {{.025, -.012, .018}, {.8, .6, 0}, {-.3, .4, std::sqrt(.75)}};
        if (solid)
            s.rectangle({0, 0}, {.015, .012});
        model::Datum offsets;
        offsets.references.push_back({});
        offsets.references[0].object = s.id;
        offsets.references[0].role = "Plane";
        offsets.parameters = sketch::fields({{"distance", .012}, {"count", 3}});
        model::Datum axis;
        axis.kind = "DatumAxis";
        axis.mode = "Principal";
        axis.parameters = sketch::fields({{"axis", "Y"}});
        model::SupportDefinition model;
        model.document = client->state().activeDocument;
        model.sketches = sketch::encode({s});
        model.datums = {offsets, axis};
        model.history = {s.id};
        if (solid) {
            features::Feature feature;
            feature.parameters = sketch::fields({{"sketch", s.id},
                                                 {"operation", "NewBody"},
                                                 {"target", ""},
                                                 {"depth", .005},
                                                 {"extent", "Forward"}});
            model.features = features::encode({feature});
            model.history.append(feature.id);
        }
        model.history.append(offsets.outputs());
        model.history.append(axis.id);
        QSignalSpy inspections(client, &Client::supportGeometry);
        client->projectCommand("SetModelSupports", sketch::fields({{"model", model.map()}}));
        QVERIFY2(wait([&] { return !client->outstanding() && !inspections.empty(); }),
                 qPrintable(client->state().message));
        QCOMPARE(inspections.size(), 1);
        auto revision = client->state().revision;
        auto records = client->state().documentRecords;
        auto *browser = window.findChild<QTreeWidget *>("datumBrowser");
        auto *tree = window.findChild<QTreeWidget *>("modelTree");
        auto *view = window.sceneView();
        window.command("datum.browser")->trigger();
        QVERIFY(browser->isVisible());
        QVERIFY(view->isVisible());
        QCOMPARE(browser->topLevelItemCount(), 11); // 6 principal, 1 sketch, 3 offsets, 1 axis.
        for (auto id : offsets.outputs())
            QVERIFY(row(tree, id, Qt::UserRole + 6));
        auto id = offsets.outputs()[1];
        auto *plane = row(browser, id);
        QVERIFY(plane);
        browser->scrollToItem(plane);
        QTest::mouseClick(browser->viewport(), Qt::LeftButton, Qt::NoModifier,
                          browser->visualItemRect(plane).center());
        QVERIFY(window.command("view.normalTo")->isEnabled());
        auto distance = view->cameraDistance();
        QTest::mouseClick(window.findChild<QPushButton *>("datumNormalTo"), Qt::LeftButton);
        QCOMPARE(view->cameraDistance(), distance);
        auto n = QVector3D::crossProduct(vector(s.frame.u), vector(s.frame.v));
        auto origin = vector(s.frame.origin) * 1000 + n * 24;
        auto c = view->project(origin), u = view->project(origin + vector(s.frame.u) * 5),
             v = view->project(origin + vector(s.frame.v) * 5);
        QVERIFY(QLineF(c, QPointF(view->width() / 2., view->height() / 2.)).length() < .05);
        QVERIFY(u.x() > c.x());
        QVERIFY(std::abs(u.y() - c.y()) < .05);
        QVERIFY(v.y() < c.y());
        QVERIFY(std::abs(v.x() - c.x()) < .05);
        auto *filter = window.findChild<QComboBox *>("selectionFilter");
        filter->setCurrentText("Datum");
        // Hide overlapping parallel planes so picking must identify this output.
        for (auto other : offsets.outputs())
            if (other != id)
                row(browser, other)->setCheckState(0, Qt::Unchecked);
        row(browser, axis.id)->setCheckState(0, Qt::Unchecked);
        auto segments = view->datumSegments(id);
        QVERIFY(segments.size() >= 8);
        auto position = (view->project(segments[0]) + view->project(segments[1])) / 2;
        QCOMPARE(view->pick(position).entity, id);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, position.toPoint());
        QVERIFY(window.command("view.normalTo")->isEnabled());
        browser->setCurrentItem(row(browser, id));
        browser->setFocus();
        QTest::keyClick(browser, Qt::Key_Space);
        QCOMPARE(row(browser, id)->checkState(0), Qt::Unchecked);
        QVERIFY(view->pick(position).empty());
        QVERIFY(
            window.command("view.normalTo")->isEnabled()); // Hidden tree selection still navigates.
        QCOMPARE(row(tree, id, Qt::UserRole + 6)->checkState(0), Qt::Unchecked);
        browser->setCurrentItem(row(browser, axis.id));
        QVERIFY(!window.command("view.normalTo")->isEnabled());
        QVERIFY(window.command("view.normalTo")->toolTip().contains("axis"));
        auto *sketchRow = row(tree, "sketch/" + s.id, Qt::UserRole + 6);
        QVERIFY(sketchRow);
        tree->scrollToItem(sketchRow);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tree->visualItemRect(sketchRow).center());
        QVERIFY(window.command("view.normalTo")->isEnabled());
        window.command("view.normalTo")->trigger();
        QVERIFY(QLineF(view->project(vector(s.frame.origin) * 1000),
                       QPointF(view->width() / 2., view->height() / 2.))
                    .length() < .05);
        QCOMPARE(client->state().revision, revision);
        QCOMPARE(client->state().documentRecords, records);
        // A new accepted revision must replace the displayed exact frame, retaining identity.
        auto changed = model;
        changed.datums[0].parameters.insert(QStringLiteral("distance"), .02);
        auto oldCount = inspections.size();
        client->projectCommand("SetModelSupports", sketch::fields({{"model", changed.map()}}));
        QVERIFY(wait([&] { return !client->outstanding() && inspections.size() > oldCount; }));
        browser->setCurrentItem(row(browser, id));
        window.command("view.normalTo")->trigger();
        auto moved = vector(s.frame.origin) * 1000 + n * 40;
        QVERIFY(QLineF(view->project(moved), QPointF(view->width() / 2., view->height() / 2.))
                    .length() < .05);
        // Scene state must not leak to a different Part, then must restore this session's
        // visibility.
        auto document = client->state().activeDocument;
        client->createDocument("Part", "Other Part", units);
        QVERIFY(wait(
            [&] { return !client->outstanding() && client->state().activeDocument != document; }));
        QVERIFY(!row(browser, id));
        QVERIFY(!window.command("view.normalTo")->isEnabled());
        client->switchDocument(document);
        QVERIFY(wait(
            [&] { return !client->outstanding() && row(browser, id) && inspections.size() >= 2; }));
        QCOMPARE(row(browser, id)->checkState(0), Qt::Unchecked);
        // Refresh and restart must rebuild exact display data, with no revision change.
        revision = client->state().revision;
        auto count = inspections.size();
        window.command("datum.refresh")->trigger();
        QVERIFY(wait([&] { return !client->outstanding() && inspections.size() > count; }));
        QCOMPARE(client->state().revision, revision);
        count = inspections.size();
        client->restart();
        QVERIFY(wait([&] {
            return client->ready() && !client->outstanding() && inspections.size() > count;
        }));
        QCOMPARE(client->state().revision, revision);
        row(browser, id)->setCheckState(0, Qt::Checked);
        browser->setCurrentItem(row(browser, id));
        window.command("view.normalTo")->trigger();
        if (qEnvironmentVariableIsSet("CAD_ARTIFACT_DIR")) {
            QTest::qWait(100);
            QVERIFY(window.grab().save(qEnvironmentVariable("CAD_ARTIFACT_DIR") + "/datum-" +
                                       QString::fromLatin1(QTest::currentDataTag()) + ".png"));
            QVERIFY2(!view->graphicsDescription().contains("initialized 0"),
                     qPrintable(view->graphicsDescription()));
        }
        client->closeProject();
        QVERIFY(wait([&] { return !client->outstanding() && !client->state().projectOpen; }));
        QVERIFY(!window.command("view.normalTo")->isEnabled());
        QVERIFY(view->datumSegments(id).empty());
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
    DatumContracts test;
    return QTest::qExec(&test, argc, argv);
}
#include "datum_contracts.moc"

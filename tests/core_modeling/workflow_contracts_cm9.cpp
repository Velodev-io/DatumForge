#include "../support/modal.hpp"
#include "EvaluationUI.hpp"
#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "ModelingFixture.hpp"
#include "SketchPanel.hpp"
#include "features/Catalogs.hpp"
#include "features/ModelingIntent.hpp"
#include "features/ModelingRecords.hpp"
#include "geometry/occt/ModelingFeatures.hpp"
#include "geometry/occt/SupportReferences.hpp"
#include "model/SupportEditing.hpp"
#include "persistence/ProjectStore.hpp"
#include "render/Viewport.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <numbers>

using namespace cad;
using namespace cad::geometry;
using namespace modeling_fixture;
using sketch::fields;

namespace {
bool waitFor(std::function<bool()> ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 20000)
        QTest::qWait(5);
    return ready();
}

struct NativeSession {
    QTemporaryDir root{QDir::tempPath() + "/cad-module9-review-native-XXXXXX"};
    QTemporaryDir files{QDir::tempPath() + "/cad-module9-review-native-files-XXXXXX"};
    std::unique_ptr<MainWindow> window;
    Client *client = nullptr;

    bool start(QString units) {
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
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            window->raise();
            window->activateWindow();
            if (!QTest::qWaitForWindowExposed(window.get()))
                return false;
        }
        client = window->workerClient();
        if (!waitFor([&] { return client->ready(); }))
            return false;
        client->newProject("Part", "CM9 native review", units);
        return idle();
    }

    bool idle() {
        QTest::qWait(20);
        return waitFor([&] { return !client->outstanding(); });
    }

    QCborMap document() const {
        for (auto value : client->state().documentRecords)
            if (value.toMap().value("id") == client->state().activeDocument)
                return value.toMap();
        return {};
    }
};

inline void checkNear(double actual, double expected, double tolerance) {
    QVERIFY2(std::abs(actual - expected) <= tolerance,
             qPrintable(QString("Expected %1, got %2 (diff %3 > %4)")
                            .arg(expected, 0, 'g', 12)
                            .arg(actual, 0, 'g', 12)
                            .arg(std::abs(actual - expected), 0, 'g', 12)
                            .arg(tolerance, 0, 'g', 12)));
}

QCborMap makeExtrude(QString sketch, double depth = 0.05) {
    auto m = extrude(sketch);
    m.insert(QStringLiteral("direction1"), blind(depth));
    return m;
}

domain::GeometryReference findFace(const TopoDS_Shape &shape, QString object, double zMin = -1e9,
                                   double zMax = 1e9) {
    domain::GeometryReference r = support_fixture::ref(object, "Face");
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface surf(TopoDS::Face(ex.Current()));
        if (surf.GetType() == GeomAbs_Plane) {
            double z = surf.Plane().Location().Z();
            if (z >= zMin && z <= zMax) {
                r.topology = supportToken(ex.Current());
                return r;
            }
        }
    }
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        r.topology = supportToken(ex.Current());
        return r;
    }
    return r;
}

} // namespace

class WorkflowContractsCM9 : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST"))
            QVERIFY(QGuiApplication::platformName() == "cocoa");
    }

    void nativeEffectiveScales_data() {
        QTest::addColumn<QString>("units");
        QTest::newRow("mm") << QString("mm");
        QTest::newRow("inch") << QString("in");
    }

    void nativeEffectiveScales() {
        QFETCH(QString, units);
        NativeSession session;
        QVERIFY(session.start(units));
        auto &window = *session.window;
        auto *client = session.client;
        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            QCOMPARE(QGuiApplication::platformName(), QString("cocoa"));
            QVERIFY(std::abs(window.devicePixelRatioF() -
                             qEnvironmentVariable("CAD_EXPECTED_DPR").toDouble()) < .01);
        }

        double unit = units == "in" ? 0.0254 : 0.001;
        auto *featurePanel = window.findChild<FeaturePanel *>();
        QVERIFY(featurePanel != nullptr);
        auto *viewport = window.sceneView();
        QVERIFY(viewport != nullptr);

        // 1. Initial Solid Setup: 100x100x50 mm box
        sketch::Sketch sBase;
        sBase.name = "BaseBoxSketch";
        sBase.rectangle({0, 0}, {0.100, 0.100});
        auto baseExt = makeExtrude(sBase.id, 0.050);
        auto baseId = baseExt.value("id").toString();

        // Sketch for hole placement with center point on top face (z = 50 mm)
        sketch::Sketch sHole;
        sHole.name = "HolePointSketch";
        sHole.frame.origin = {0, 0, 0.050};
        auto firstPoint = sHole.point({0.050, 0.050});

        auto modelWithBase = model::editSupportedSketches(
            session.document(), sketch::encode(std::vector<sketch::Sketch>{sBase, sHole}));
        client->projectCommand("SetModelSupports", fields({{"model", modelWithBase.map()},
                                                           {"intents", QCborArray{baseExt}}}));
        QVERIFY(session.idle());

        auto docAfterBase = session.document();
        QVERIFY(docAfterBase.contains(QStringLiteral("modeling")));
        auto baseIntents = docAfterBase.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(baseIntents.size(), 1);

        auto evaluated = evaluateModeling(modelWithBase, {baseExt});
        auto face = findFace(evaluated.part.bodies[0].shape, baseId, 49, 51);

        // ====================================================================
        // 2. Hole Catalog Workflow
        // ====================================================================
        featurePanel->begin("Hole");
        QVERIFY(session.idle());
        QVERIFY(featurePanel->active());

        auto *holeDs = window.findChild<QComboBox *>("holeDataset");
        auto *holeStd = window.findChild<QComboBox *>("holeStandard");
        auto *holeType = window.findChild<QComboBox *>("holeType");
        auto *holeSize = window.findChild<QComboBox *>("holeSize");
        auto *holeFit = window.findChild<QComboBox *>("holeFit");
        auto *holeProv = window.findChild<QLabel *>("holeProvenance");
        auto *holeCustom = window.findChild<QCheckBox *>("holeCustomSizing");
        auto *holeSearch = window.findChild<QLineEdit *>("holeCatalogSearch");
        auto *holeDia = window.findChild<QDoubleSpinBox *>("holeDiameter");
        auto *holePoints = window.findChild<QListWidget *>("holePoints");
        auto *profileCombo = window.findChild<QComboBox *>("featureProfile");
        auto *holePlacementFace = window.findChild<QComboBox *>("holePlacementFace");

        QVERIFY(holeDs != nullptr);
        QVERIFY(holeStd != nullptr);
        QVERIFY(holeType != nullptr);
        QVERIFY(holeSize != nullptr);
        QVERIFY(holeFit != nullptr);
        QVERIFY(holeProv != nullptr);
        QVERIFY(holeCustom != nullptr);
        QVERIFY(holeSearch != nullptr);
        QVERIFY(holeDia != nullptr);
        QVERIFY(holePlacementFace != nullptr);

        // Select placement sketch
        int sIdx = profileCombo->findData(sHole.id);
        if (sIdx >= 0)
            profileCombo->setCurrentIndex(sIdx);
        QVERIFY(session.idle());

        auto selectReference = [&](const char *id, QCborMap ref) {
            auto *box = window.findChild<QComboBox *>(id);
            if (!box)
                return false;
            for (int i = 0; i < box->count(); ++i)
                if (box->itemData(i).value<QCborMap>() == ref) {
                    box->setCurrentIndex(i);
                    return true;
                }
            return false;
        };
        QVERIFY(selectReference("holePlacementFace", face.map()));
        QVERIFY(session.idle());

        if (holePoints->count() > 0) {
            for (int i = 0; i < holePoints->count(); ++i)
                holePoints->item(i)->setCheckState(
                    holePoints->item(i)->data(Qt::UserRole).toString() == firstPoint
                        ? Qt::Checked
                        : Qt::Unchecked);
        }

        // Test dataset selection
        int dsIdx = holeDs->findText("ansi_metric");
        QVERIFY(dsIdx >= 0);
        holeDs->setCurrentIndex(dsIdx);

        int stdIdx = holeStd->findText("B18.3.4M");
        QVERIFY(stdIdx >= 0);
        holeStd->setCurrentIndex(stdIdx);

        int typeIdx = holeType->findText("SocketButtonHeadCapScrew");
        QVERIFY(typeIdx >= 0);
        holeType->setCurrentIndex(typeIdx);

        int sizeIdx = holeSize->findText("M8");
        QVERIFY(sizeIdx >= 0);
        holeSize->setCurrentIndex(sizeIdx);

        int fitIdx = holeFit->findText("Normal");
        QVERIFY(fitIdx >= 0);
        holeFit->setCurrentIndex(fitIdx);

        // Sizing is in catalog mode (not custom)
        holeCustom->setChecked(false);
        checkNear(holeDia->value() * unit, 0.0090, 1e-4); // Normal fit for M8 = 9.0 mm
        QVERIFY(holeProv->text().contains("B18.3.4M"));
        QVERIFY(holeProv->text().contains("M8"));

        // Test search filter
        holeSearch->setText("M6");
        QCOMPARE(holeSize->currentText(), QString("M6"));
        checkNear(holeDia->value() * unit, 0.0064, 1e-4); // Close fit for M6 = 6.4 mm

        // Switch back to M8 with Normal fit
        holeSize->setCurrentText("M8");
        holeFit->setCurrentText("Normal");

        // Accept the standard hole
        auto *acceptFeatureBtn = window.findChild<QPushButton *>("acceptFeatureButton");
        QVERIFY(acceptFeatureBtn != nullptr);
        featurePanel->preview();
        QVERIFY(session.idle());
        QVERIFY(acceptFeatureBtn->isEnabled());
        featurePanel->finish();
        QVERIFY(session.idle());
        QVERIFY(!featurePanel->active());

        // Verify persisted hole intent has Catalog mode
        auto docWithHole = session.document();
        auto holeIntent =
            docWithHole.value("modeling").toMap().value("intents").toArray()[1].toMap();
        auto holeSizing = holeIntent.value("sizing").toMap();
        QCOMPARE(holeSizing.value("mode").toString(), QString("Catalog"));
        QCOMPARE(holeSizing.value("standard").toString(), QString("B18.3.4M"));
        QCOMPARE(holeSizing.value("size").toString(), QString("M8"));
        checkNear(holeSizing.value("diameter").toDouble(), 0.0090, 1e-4);

        // ====================================================================
        // 3. Materials Workflow
        // ====================================================================
        auto *matAction = window.command("material.manage");
        QVERIFY(matAction != nullptr);
        QVERIFY(matAction->isEnabled());
        matAction->trigger();

        auto *matDialog = window.findChild<MaterialDialog *>("materialDialog");
        QVERIFY(matDialog != nullptr);
        QVERIFY(matDialog->isVisible());

        auto *matSearch = matDialog->findChild<QLineEdit *>("materialSearch");
        auto *matTree = matDialog->findChild<QTreeWidget *>("materialTree");
        auto *matDensityLabel = matDialog->findChild<QLabel *>("materialDensityLabel");
        auto *matModulusLabel = matDialog->findChild<QLabel *>("materialModulusLabel");
        auto *matFavoriteBtn = matDialog->findChild<QPushButton *>("materialFavoriteButton");
        auto *matApplyBtn = matDialog->findChild<QPushButton *>("materialApplyButton");

        QVERIFY(matSearch != nullptr);
        QVERIFY(matTree != nullptr);
        QVERIFY(matDensityLabel != nullptr);
        QVERIFY(matApplyBtn != nullptr);

        // Search for Aluminum
        matSearch->setText("6061");
        matDialog->selectMaterial("al_6061_t6");
        QVERIFY(matDensityLabel->text().contains("2700"));
        QVERIFY(matModulusLabel->text().contains("68.9"));

        // Toggle favorite
        matFavoriteBtn->click();

        // Apply material to Part
        matApplyBtn->click();
        QVERIFY(session.idle());
        QVERIFY(!matDialog->isVisible());

        // Verify document received material
        auto docWithMat = session.document();
        auto supMapWithMat = docWithMat.contains(QStringLiteral("model_supports"))
                                 ? docWithMat.value("model_supports").toMap()
                                 : docWithMat.value("supports").toMap();
        QVERIFY(supMapWithMat.contains(QStringLiteral("material")));
        auto matObj = supMapWithMat.value("material").toMap();
        QCOMPARE(matObj.value("id").toString(), QString("al_6061_t6"));

        // ====================================================================
        // 3b. Appearance Workflow
        // ====================================================================
        auto *appAction = window.command("appearance.manage");
        QVERIFY(appAction != nullptr);
        QVERIFY(appAction->isEnabled());
        appAction->trigger();

        auto *appDialog = window.findChild<AppearanceDialog *>("appearanceDialog");
        QVERIFY(appDialog != nullptr);
        QVERIFY(appDialog->isVisible());

        auto *appScope = appDialog->findChild<QComboBox *>("appearanceScope");
        auto *appApply = appDialog->findChild<QPushButton *>("appearanceApplyButton");
        QVERIFY(appScope != nullptr);
        QVERIFY(appApply != nullptr);

        // 1. Part-level appearance: Green (0, 1, 0, 1)
        appDialog->setScope("Entire Part");
        appDialog->setColor(QVector4D(0.0f, 1.0f, 0.0f, 1.0f));
        appDialog->updatePreview();

        auto greenCol = viewport->resolveAppearance("any_face", QVector4D(0.5f, 0.5f, 0.5f, 1.0f));
        checkNear(greenCol.x(), 0.0, 1e-3);
        checkNear(greenCol.y(), 1.0, 1e-3);
        checkNear(greenCol.z(), 0.0, 1e-3);

        appApply->click();
        QVERIFY(session.idle());
        QVERIFY(!appDialog->isVisible());

        // 2. Face-level override: Red (1, 0, 0, 1) on top face
        QVERIFY(client->mesh() != nullptr);
        QVERIFY(!client->mesh()->faceIds.isEmpty());
        QString meshFace = client->mesh()->faceIds.first();
        Selection faceSelection{"Face", meshFace, "part"};
        client->select(faceSelection);
        appAction->trigger();
        QVERIFY(appDialog->isVisible());
        appDialog->refreshTarget();
        QCOMPARE(appScope->currentText(), QString("Face"));

        appDialog->setColor(QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
        appDialog->updatePreview();

        // Verify precedence: face override takes precedence over part appearance
        auto resolvedFaceCol =
            viewport->resolveAppearance(meshFace, QVector4D(0.5f, 0.5f, 0.5f, 1.0f));
        checkNear(resolvedFaceCol.x(), 1.0, 1e-3);
        checkNear(resolvedFaceCol.y(), 0.0, 1e-3);

        auto resolvedOtherCol =
            viewport->resolveAppearance("other_face", QVector4D(0.5f, 0.5f, 0.5f, 1.0f));
        checkNear(resolvedOtherCol.x(), 0.0, 1e-3);
        checkNear(resolvedOtherCol.y(), 1.0, 1e-3);

        appApply->click();
        QVERIFY(session.idle());
        QVERIFY(!appDialog->isVisible());

        auto docWithApp = session.document();
        auto supWithApp = docWithApp.contains(QStringLiteral("model_supports"))
                              ? docWithApp.value("model_supports").toMap()
                              : docWithApp.value("supports").toMap();
        QVERIFY(supWithApp.contains(QStringLiteral("appearance_overrides")));
        QCOMPARE(supWithApp.value("appearance_overrides").toArray().size(), 2);

        // ====================================================================
        // 4. Evaluate: Mass Properties & Section Properties
        // ====================================================================
        auto *massAction = window.command("massProperties");
        QVERIFY(massAction != nullptr);
        QVERIFY(massAction->isEnabled());
        massAction->trigger();

        auto *massDialog = window.findChild<MassPropertiesDialog *>("massPropertiesDialog");
        QVERIFY(massDialog != nullptr);
        QVERIFY(massDialog->isVisible());

        auto *massVal = massDialog->findChild<QLabel *>("massValueLabel");
        auto *volVal = massDialog->findChild<QLabel *>("volumeValueLabel");
        auto *areaVal = massDialog->findChild<QLabel *>("surfaceAreaValueLabel");
        auto *massStatus = massDialog->findChild<QLabel *>("massStatusLabel");
        auto *massRecalc = massDialog->findChild<QPushButton *>("massRecalculateButton");

        QVERIFY(massVal != nullptr);
        QVERIFY(volVal != nullptr);
        QVERIFY(areaVal != nullptr);
        QVERIFY(massStatus != nullptr);
        QVERIFY(massRecalc != nullptr);

        massRecalc->click();
        QVERIFY(session.idle());
        QVERIFY(massStatus->text().contains("successfully") ||
                massStatus->text().contains("Evaluated"));
        QVERIFY(massVal->text().contains("kg"));
        auto *massScope = massDialog->findChild<QComboBox *>("massScopeCombo");
        QVERIFY(massScope);
        QCOMPARE(massScope->itemData(1).toString(), baseId);
        massScope->setCurrentIndex(1);
        massRecalc->click();
        QVERIFY(session.idle());
        QVERIFY(massStatus->text().contains("Evaluated"));

        massDialog->accept();
        QVERIFY(!massDialog->isVisible());

        // Section Properties
        auto *secAction = window.command("sectionProperties");
        QVERIFY(secAction != nullptr);
        secAction->trigger();

        auto *secDialog = window.findChild<SectionPropertiesDialog *>("sectionPropertiesDialog");
        QVERIFY(secDialog != nullptr);
        QVERIFY(secDialog->isVisible());

        // Select a face and evaluate
        Selection faceSel{"Face", meshFace, "part"};
        client->select(faceSel);

        auto *secRecalc = secDialog->findChild<QPushButton *>("sectionRecalculateButton");
        QVERIFY(secRecalc != nullptr);
        secRecalc->click();
        QVERIFY(session.idle());

        secDialog->accept();
        QVERIFY(!secDialog->isVisible());

        // Test Section Properties on a planar sketch as well (CM-241)
        Selection sketchSel{"Sketch", sHole.id};
        client->select(sketchSel);
        secAction->trigger();
        QVERIFY(secDialog->isVisible());
        secRecalc->click();
        QVERIFY(session.idle());
        secDialog->accept();
        QVERIFY(!secDialog->isVisible());

        // ====================================================================
        // 5. Named Section Views Workflow
        // ====================================================================
        auto *secViewAction = window.command("sectionView");
        QVERIFY(secViewAction != nullptr);
        secViewAction->trigger();

        auto *secViewDialog = window.findChild<SectionViewDialog *>("sectionViewDialog");
        QVERIFY(secViewDialog != nullptr);
        QVERIFY(secViewDialog->isVisible());

        auto *secViewName = secViewDialog->findChild<QLineEdit *>("sectionViewName");
        auto *secViewActive = secViewDialog->findChild<QCheckBox *>("sectionViewActive");
        auto *secViewOffset = secViewDialog->findChild<QDoubleSpinBox *>("sectionViewOffset");
        auto *secViewFlip = secViewDialog->findChild<QCheckBox *>("sectionViewFlip");
        auto *secViewApply = secViewDialog->findChild<QPushButton *>("sectionViewApplyButton");
        auto *secViewNew = secViewDialog->findChild<QPushButton *>("sectionViewNewButton");

        QVERIFY(secViewName != nullptr);
        QVERIFY(secViewActive != nullptr);
        QVERIFY(secViewOffset != nullptr);
        QVERIFY(secViewFlip != nullptr);
        QVERIFY(secViewApply != nullptr);
        QVERIFY(secViewNew != nullptr);

        auto *sectionSupport = secViewDialog->findChild<QComboBox *>("sectionViewSupport");
        QVERIFY(sectionSupport);
        sectionSupport->setCurrentIndex(sectionSupport->findData("principal/XZ"));
        secViewName->setText("Inspection Cut Plane");
        secViewActive->setChecked(true);
        secViewOffset->setValue(-15.0);
        secViewFlip->setChecked(true);

        // Verify live viewport clipping reflection
        QVERIFY(viewport->sectionClippingActive());
        checkNear(std::abs(viewport->sectionClippingNormal().y()), 1.0, 1e-6);
        checkNear(viewport->sectionClippingOffset(), -15.0, 1e-4);
        QVERIFY(viewport->sectionClippingFlipped());

        // Apply and verify document persistence
        secViewApply->click();
        QVERIFY(session.idle());
        QVERIFY(!secViewDialog->isVisible());

        // Create a second named section view to verify multiple views are preserved
        secViewAction->trigger();
        QVERIFY(secViewDialog->isVisible());
        secViewNew->click();
        secViewName->setText("Section Two");
        secViewOffset->setValue(10.0);
        secViewActive->setChecked(true);
        secViewApply->click();
        QVERIFY(session.idle());
        QVERIFY(!secViewDialog->isVisible());

        auto docWithSec = session.document();
        auto supWithSec = docWithSec.contains(QStringLiteral("model_supports"))
                              ? docWithSec.value("model_supports").toMap()
                              : docWithSec.value("supports").toMap();
        auto secViewsArr = supWithSec.value("section_views").toArray();
        QCOMPARE(secViewsArr.size(), 2);
        auto firstSv = secViewsArr[0].toMap();
        QCOMPARE(firstSv.value("name").toString(), QString("Inspection Cut Plane"));
        QVERIFY(firstSv.value("active").toBool());
        QVERIFY(firstSv.value("flip").toBool());
        checkNear(firstSv.value("offset").toDouble(), -0.015, 1e-4);

        // ====================================================================
        // 6. Viewport Display Modes and Shadows
        // ====================================================================
        auto *viewModeCombo = window.findChild<QComboBox *>("viewMode");
        QVERIFY(viewModeCombo != nullptr);
        int hlvIdx = viewModeCombo->findText("Hidden Lines Visible");
        QVERIFY(hlvIdx >= 0);
        viewModeCombo->setCurrentIndex(hlvIdx);

        auto *shadowsToggle = window.findChild<QCheckBox *>("viewShadows");
        QVERIFY(shadowsToggle != nullptr);
        shadowsToggle->setChecked(true);
        QVERIFY(viewport->shadows());

        // ====================================================================
        // 7. Project Save & Reopen Round-Trip
        // ====================================================================
        auto savePath = session.files.filePath("cm9_native_review.cadproj");
        client->saveProject(savePath, true);
        QVERIFY(session.idle());
        QVERIFY2(QFile::exists(savePath), qPrintable(client->state().message));

        client->closeProject();
        QVERIFY(session.idle());
        QVERIFY(!client->state().projectOpen);

        client->openProject(savePath, "Saved");
        QVERIFY(session.idle());
        QVERIFY(client->state().projectOpen);

        auto reopenedDoc = session.document();
        QVERIFY(reopenedDoc.contains(QStringLiteral("modeling")));
        auto reopenedIntents = reopenedDoc.value("modeling").toMap().value("intents").toArray();
        QCOMPARE(reopenedIntents.size(), 2); // Extrude + Hole

        auto reopenedSupports = reopenedDoc.contains(QStringLiteral("model_supports"))
                                    ? reopenedDoc.value("model_supports").toMap()
                                    : reopenedDoc.value("supports").toMap();
        QVERIFY(reopenedSupports.contains(QStringLiteral("material")));
        QVERIFY(reopenedSupports.contains(QStringLiteral("section_views")));
        QCOMPARE(reopenedSupports.value("section_views").toArray().size(), 2);
        QVERIFY(reopenedSupports.contains(QStringLiteral("appearance_overrides")));
        QCOMPARE(reopenedSupports.value("appearance_overrides").toArray().size(), 2);

        // Verify viewport appearance and section clipping synchronized on project reopen
        auto reopenedFaceCol =
            viewport->resolveAppearance(meshFace, QVector4D(0.5f, 0.5f, 0.5f, 1.0f));
        checkNear(reopenedFaceCol.x(), 1.0, 1e-3);
        checkNear(reopenedFaceCol.y(), 0.0, 1e-3);

        auto reopenedOtherCol =
            viewport->resolveAppearance("other_face", QVector4D(0.5f, 0.5f, 0.5f, 1.0f));
        checkNear(reopenedOtherCol.x(), 0.0, 1e-3);
        checkNear(reopenedOtherCol.y(), 1.0, 1e-3);

        QVERIFY2(waitFor([&] { return viewport->sectionClippingActive(); }),
                 qPrintable(client->state().message));
        checkNear(std::abs(viewport->sectionClippingNormal().y()), 1.0, 1e-6);
        checkNear(viewport->sectionClippingOffset(), -15.0, 1e-4);

        if (qEnvironmentVariableIsSet("CAD_NATIVE_TEST")) {
            auto directory = qEnvironmentVariable("CAD_ARTIFACT_DIR");
            if (!directory.isEmpty())
                QVERIFY(window.grab().save(directory + "/cm9-" + units + ".png"));
        }
    }
};

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    WorkflowContractsCM9 tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "workflow_contracts_cm9.moc"

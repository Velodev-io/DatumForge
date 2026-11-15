#pragma once
#include "features/Model.hpp"
#include "features/ModelingIntent.hpp"
#include "protocol/Client.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
namespace cad {
class FeaturePanel : public QWidget {
    Q_OBJECT
    Client *client;
    QCborMap document, result;
    features::Feature draft;
    QCborArray candidate;
    bool editing = false, validPreview = false, installing = false, commitPending = false;
    bool supportPreviewPending = false, previewAfterInputs = false;

    bool isModeling = false;
    QCborMap modelingDraft;
    QCborArray candidateIntents;
    QCborArray currentRegions, repairIntents, inputSketches, inputGeometry;
    QCborMap repairResolution;
    QComboBox *repairPath = nullptr;
    QPushButton *selectRegions = nullptr, *selectDraftFaces = nullptr;
    void chooseRegions();
    void installModelingInputs(QCborMap input);

    QFormLayout *form;
    QWidget *inputs, *evaluation;
    QLineEdit *name;
    QComboBox *profile, *operation, *target, *source, *extent, *distribution;
    QDoubleSpinBox *size, *depth, *angle, *spacing, *density;
    QSpinBox *count;
    QCheckBox *through;
    std::array<QDoubleSpinBox *, 3> origin, axis;
    QLabel *title, *diagnostic, *measurement;
    QPushButton *accept, *previewButton, *cancelButton;

    // CM5 Controls
    QComboBox *handleParameter = nullptr;
    QDoubleSpinBox *handleField = nullptr;
    bool angularHandle = false;
    QComboBox *profileMode = nullptr;
    QCheckBox *flipSide = nullptr;
    QComboBox *startMode = nullptr;
    QDoubleSpinBox *startOffset = nullptr;
    QComboBox *startReference = nullptr;
    QCheckBox *reverseDirection = nullptr;
    QDoubleSpinBox *surfaceOffset = nullptr, *surfaceOffset2 = nullptr;
    QCheckBox *offsetReverse = nullptr, *offsetReverse2 = nullptr;
    QComboBox *dir1Reference = nullptr;
    QCheckBox *draftEnable = nullptr;
    QDoubleSpinBox *draftAngle = nullptr;
    QCheckBox *draftOutward = nullptr;
    QCheckBox *direction2Enable = nullptr;
    QComboBox *extent2 = nullptr;
    QDoubleSpinBox *depth2 = nullptr;
    QComboBox *dir2Reference = nullptr;
    QCheckBox *draft2Enable = nullptr;
    QDoubleSpinBox *draft2Angle = nullptr;
    QCheckBox *draft2Outward = nullptr;
    QComboBox *thinMode = nullptr;
    QDoubleSpinBox *thickness1 = nullptr;
    QDoubleSpinBox *thickness2 = nullptr;
    QCheckBox *thinReverse = nullptr;
    QComboBox *revolveAxis = nullptr;
    QComboBox *revolveExtent = nullptr;
    QDoubleSpinBox *angle2 = nullptr;

    // Standalone Draft controls
    QComboBox *draftNeutral = nullptr;
    QComboBox *draftPull = nullptr;
    QComboBox *draftFaces = nullptr;
    QDoubleSpinBox *draftStandaloneAngle = nullptr;
    QCheckBox *draftStandaloneOutward = nullptr;

    // CM6 Controls
    // Hole Wizard
    QComboBox *holeFamily = nullptr;
    QComboBox *holePlacementFace = nullptr;
    QComboBox *holePlacementSketch = nullptr;
    QDoubleSpinBox *holeDiameter = nullptr;
    QDoubleSpinBox *holeDepth = nullptr;
    QDoubleSpinBox *holeCounterboreDia = nullptr;
    QDoubleSpinBox *holeCounterboreDepth = nullptr;
    QDoubleSpinBox *holeCountersinkDia = nullptr;
    QDoubleSpinBox *holeCountersinkAngle = nullptr;
    QDoubleSpinBox *holeDrillAngle = nullptr;
    QComboBox *holeTermination = nullptr;

    // Treatments: Fillet & Chamfer
    QComboBox *filletMethod = nullptr;
    QDoubleSpinBox *filletRadius = nullptr;
    QCheckBox *filletPropagateTangent = nullptr;
    QComboBox *filletEdges = nullptr;

    QComboBox *chamferMethod = nullptr;
    QDoubleSpinBox *chamferDistance = nullptr;
    QDoubleSpinBox *chamferDistance2 = nullptr;
    QDoubleSpinBox *chamferAngle = nullptr;
    QCheckBox *chamferFlip = nullptr;
    QComboBox *chamferEdges = nullptr;

    // Rib
    QDoubleSpinBox *ribThickness = nullptr;
    QComboBox *ribSide = nullptr;
    QComboBox *ribDirection = nullptr;
    QCheckBox *ribFlipMaterial = nullptr;

    // Repetition: LinearPattern, CircularPattern, Mirror
    QComboBox *patSeedKind = nullptr;
    QComboBox *patSeed = nullptr;
    QComboBox *linearDir1Ref = nullptr;
    QDoubleSpinBox *linearSpacing1 = nullptr;
    QSpinBox *linearCount1 = nullptr;
    QCheckBox *linearReverse1 = nullptr;
    QComboBox *linearDir2Ref = nullptr;
    QDoubleSpinBox *linearSpacing2 = nullptr;
    QSpinBox *linearCount2 = nullptr;
    QCheckBox *linearReverse2 = nullptr;

    QComboBox *circAxisRef = nullptr;
    QSpinBox *circCount = nullptr;
    QCheckBox *circEqualSpacing = nullptr;
    QDoubleSpinBox *circAngle = nullptr;
    QCheckBox *circReverse = nullptr;

    QComboBox *mirrorPlaneRef = nullptr;

    // Cosmetic Thread
    QComboBox *threadEdgeRef = nullptr;
    QLineEdit *threadDesignation = nullptr;
    QComboBox *threadStandard = nullptr;
    QComboBox *threadType = nullptr;
    QLineEdit *threadSize = nullptr;
    QCheckBox *threadInternal = nullptr;
    QDoubleSpinBox *threadMajorDia = nullptr;
    QDoubleSpinBox *threadMinorDia = nullptr;
    QDoubleSpinBox *threadPitch = nullptr;
    QComboBox *threadTermination = nullptr;
    QCheckBox *threadShadedDisplay = nullptr;

    // Repair controls
    QComboBox *repairCandidate = nullptr;
    QPushButton *repairButton = nullptr;
    QPushButton *applyRepairButton = nullptr;

    void fieldsForKind();
    QCborArray definitions() const;
    QCborMap definitionModeling() const;
    QCborArray definitionsModeling() const;
    double unit() const;
    void invalidate();
    bool isCoreModelingDocument() const;
    void updateManipulator();

  public:
    explicit FeaturePanel(Client *client);
    void install(QCborMap active, bool idle);
    void begin(QString kind, QString editId = {});
    void discard();
    bool active() const {
        return editing;
    }
    bool hasSolid() const {
        return !document.value("features").toArray().empty() ||
               (document.contains(QStringLiteral("modeling")) &&
                !document.value("modeling").toMap().value("intents").toArray().empty()) ||
               (document.contains(QStringLiteral("model_supports")) &&
                !document.value("model_supports").toMap().value("features").toArray().empty());
    }
    void preview();
    void finish();
    QCborMap latestResult() const {
        return result;
    }
    void setSelectedRegions(const QCborArray &regions) {
        currentRegions = regions;
        invalidate();
    }
    bool isModelingActive() const {
        return isModeling;
    }
    void setDepthFromManipulator(double depthMM);
    void setDepthFinishedFromManipulator(double depthMM);
    void setDraftAngleFromManipulator(double angleDeg);
    void setDraftAngleFinishedFromManipulator(double angleDeg);

  signals:
    void editingChanged();
    void requestManipulator(QVector3D origin, QVector3D direction, double depthMM, bool reverse);
    void requestDraftManipulator(QVector3D origin, QVector3D normal, double angleDeg,
                                 double maximum = 89.9);
    void clearManipulatorRequested();
};
} // namespace cad

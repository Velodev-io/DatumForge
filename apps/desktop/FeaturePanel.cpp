#include "FeaturePanel.hpp"
#include "SupportEditor.hpp"
#include "features/ModelingRecords.hpp"
#include "model/SupportEditing.hpp"
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <numbers>
namespace cad {
using sketch::fields;
FeaturePanel::FeaturePanel(Client *c) : client(c) {
    setObjectName("featureProperties");
    setMinimumWidth(0);
    auto *layout = new QVBoxLayout(this);
    title = new QLabel("Exact Part");
    title->setObjectName("featureTitle");
    layout->addWidget(title);
    auto *buttons = new QHBoxLayout;
    previewButton = new QPushButton("Preview");
    accept = new QPushButton("Accept feature");
    cancelButton = new QPushButton("Cancel");
    previewButton->setObjectName("previewFeatureButton");
    accept->setObjectName("acceptFeatureButton");
    cancelButton->setObjectName("cancelFeatureButton");
    buttons->addWidget(previewButton);
    buttons->addWidget(accept);
    buttons->addWidget(cancelButton);
    layout->addLayout(buttons);
    diagnostic = new QLabel;
    diagnostic->setObjectName("featureDiagnostic");
    diagnostic->setWordWrap(true);
    diagnostic->setTextFormat(Qt::PlainText);
    layout->addWidget(diagnostic);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    inputs = new QWidget;
    form = new QFormLayout(inputs);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    scroll->setWidget(inputs);
    name = new QLineEdit;
    name->setObjectName("featureName");
    form->addRow("Feature name", name);
    auto combo = [&](QString id, QString label) {
        auto *w = new QComboBox;
        w->setObjectName(id);
        form->addRow(label, w);
        connect(w, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
        return w;
    };
    profile = combo("featureProfile", "Closed sketch profile");
    operation = combo("featureOperation", "Operation");
    operation->addItems({"NewBody", "Add", "Cut"});
    target = combo("featureTarget", "Target body");
    source = combo("featureSource", "Source feature / edge result");
    extent = combo("featureExtent", "Extent");
    extent->addItems({"Forward", "Reverse", "Symmetric"});
    distribution = combo("featureDistribution", "Circular distribution");
    distribution->addItems({"Spacing", "Full"});
    auto spin = [&](QString id, QString label, double value, double low, double high) {
        auto *w = new QDoubleSpinBox;
        w->setObjectName(id);
        w->setDecimals(8);
        w->setRange(low, high);
        w->setValue(value);
        form->addRow(label, w);
        connect(w, &QDoubleSpinBox::valueChanged, this, [this] { invalidate(); });
        return w;
    };
    size = spin("featureSize", "Radius / equal distance / diameter", 2, 1e-6, 1e6);
    depth = spin("featureDepth", "Depth", 10, 1e-6, 1e6);
    angle = spin("featureAngle", "Angle (degrees)", 360, 1e-6, 360);
    spacing = spin("featureSpacing", "Spacing", 10, 1e-6, 1e6);
    count = new QSpinBox;
    count->setObjectName("featureCount");
    count->setRange(1, 32);
    count->setValue(2);
    form->addRow("Count (including seed)", count);
    connect(count, &QSpinBox::valueChanged, this, [this] { invalidate(); });
    through = new QCheckBox("Through all");
    through->setObjectName("featureThrough");
    through->setChecked(true);
    form->addRow(through);
    connect(through, &QCheckBox::toggled, this, [this] { invalidate(); });
    for (int i = 0; i < 3; ++i) {
        auto suffix = QString("XYZ")[i];
        origin[i] = spin("featureOrigin" + QString(suffix), "Origin / center " + QString(suffix), 0,
                         -1e6, 1e6);
        axis[i] = spin("featureAxis" + QString(suffix),
                       "Unit direction / normal " + QString(suffix), i == 2 ? 1 : 0, -1, 1);
    }
    auto check = [&](QString id, QString label) {
        auto *w = new QCheckBox(label);
        w->setObjectName(id);
        form->addRow(w);
        connect(w, &QCheckBox::toggled, this, [this] { invalidate(); });
        return w;
    };
    profileMode = combo("featureProfileMode", "Contour selection");
    profileMode->addItems({"AllClosed", "SelectedRegions", "OpenChains"});
    flipSide = check("featureFlipSide", "Flip side to cut");
    startMode = combo("featureStartMode", "Start condition");
    startMode->addItems({"SketchPlane", "Surface", "Vertex", "Offset"});
    startOffset = spin("featureStartOffset", "Start offset distance", 0, -1000, 1000);
    startReference = combo("featureStartReference", "Start reference");
    reverseDirection = check("featureReverse", "Reverse direction 1");
    surfaceOffset = spin("featureSurfaceOffset", "Surface offset distance", 0, 0, 1000);
    offsetReverse = check("featureOffsetReverse", "Reverse surface offset");
    dir1Reference = combo("featureDir1Reference", "Direction 1 reference");
    draftEnable = check("featureDraftEnable", "Draft On/Off");
    draftAngle = spin("featureDraftAngle", "Draft angle (degrees)", 0, 0, 89.99999999);
    draftOutward = check("featureDraftOutward", "Draft outward");
    direction2Enable = check("featureDirection2Enable", "Direction 2");
    extent2 = combo("featureExtent2", "Direction 2 end condition");
    extent2->addItems({"Blind", "UpToVertex", "UpToSurface", "OffsetFromSurface", "UpToBody",
                       "UpToNext", "ThroughAll"});
    depth2 = spin("featureDepth2", "Direction 2 depth", 10, 1e-6, 1e6);
    surfaceOffset2 = spin("featureSurfaceOffset2", "Direction 2 surface offset", 0, 0, 1000);
    offsetReverse2 = check("featureOffsetReverse2", "Reverse Direction 2 surface offset");
    dir2Reference = combo("featureDir2Reference", "Direction 2 reference");
    draft2Enable = check("featureDraft2Enable", "Direction 2 Draft On/Off");
    draft2Angle =
        spin("featureDraft2Angle", "Direction 2 draft angle (degrees)", 0, 0, 89.99999999);
    draft2Outward = check("featureDraft2Outward", "Direction 2 draft outward");
    thinMode = combo("featureThinMode", "Thin feature");
    thinMode->addItems({"Solid", "OneDirection", "Midplane", "TwoDirection"});
    thickness1 = spin("featureThickness1", "Wall thickness (T1)", 1, 1e-6, 1e6);
    thickness2 = spin("featureThickness2", "Wall thickness (T2)", 1, 1e-6, 1e6);
    thinReverse = check("featureThinReverse", "Reverse thin direction");
    revolveAxis = combo("featureRevolveAxis", "Axis of revolution");
    revolveExtent = combo("featureRevolveExtent", "Revolve direction");
    revolveExtent->addItems({"Blind", "Midplane", "TwoDirection"});
    angle2 = spin("featureAngle2", "Direction 2 angle (degrees)", 0, 0, 360);

    handleParameter = combo("featureHandleParameter", "Viewport handle");
    connect(handleParameter, &QComboBox::currentIndexChanged, this,
            &FeaturePanel::updateManipulator);

    // Standalone Draft controls
    draftNeutral = combo("draftNeutral", "Neutral plane / face");
    draftPull = combo("draftPull", "Pull direction axis / edge");
    draftFaces = combo("draftFaces", "Faces to draft");
    draftStandaloneAngle = spin("draftStandaloneAngle", "Draft angle (degrees)", 5, 0, 89.99999999);
    draftStandaloneOutward = check("draftStandaloneOutward", "Draft outward");

    auto ispin = [&](QString id, QString label, int value, int low, int high) {
        auto *w = new QSpinBox;
        w->setObjectName(id);
        w->setRange(low, high);
        w->setValue(value);
        form->addRow(label, w);
        connect(w, &QSpinBox::valueChanged, this, [this] { invalidate(); });
        return w;
    };

    // CM6 Controls
    // Hole Wizard
    holeFamily = combo("holeFamily", "Hole type");
    holeFamily->addItems({"Simple", "Counterbore", "Countersink", "Drilled", "Tapped", "PipeTap",
                          "LegacySimple", "LegacySimpleDrilled", "LegacyCounterboreDrill",
                          "LegacyCountersinkDrill", "LegacyTapered"});
    holePlacementFace = combo("holePlacementFace", "Placement face");
    holePoints = new QListWidget;
    holePoints->setObjectName("holePoints");
    holePoints->setMaximumHeight(130);
    form->addRow("Hole positions", holePoints);
    connect(holePoints, &QListWidget::itemChanged, this, [this] { invalidate(); });
    connect(profile, &QComboBox::currentIndexChanged, this, [this] {
        if (!installing && editing && draft.kind == "Hole")
            installHolePoints();
    });
    holeDepthReference = combo("holeDepthReference", "Depth measured to");
    holeDepthReference->addItems({"Shoulder", "Tip"});
    holeEndReference = combo("holeEndReference", "Termination reference");
    holeTapDiameter = spin("holeTapDiameter", "Tap drill diameter", 8.5, 1e-4, 1e4);
    holeTaperAngle = spin("holeTaperAngle", "Taper included angle (degrees)", 1.7899, .0001, 89);
    holeThreadDesignation = new QLineEdit("M10x1.5");
    holeThreadDesignation->setObjectName("holeThreadDesignation");
    form->addRow("Thread designation", holeThreadDesignation);
    connect(holeThreadDesignation, &QLineEdit::textChanged, this, [this] { invalidate(); });
    holeThreadPitch = spin("holeThreadPitch", "Thread pitch", 1.5, 1e-4, 1e4);
    holeThreadDepth = spin("holeThreadDepth", "Thread depth", 20, 1e-4, 1e4);
    holeCustomSizing = check("holeCustomSizing", "Override with custom sizing");
    holeCustomSizing->setChecked(true);
    holeCatalog = new QLabel;
    holeCatalog->setWordWrap(true);
    form->addRow("Accepted catalog sizing", holeCatalog);
    holeDiameter = spin("holeDiameter", "Hole diameter (mm)", 10.0, 1e-4, 1e4);
    holeDepth = spin("holeDepth", "Hole depth (mm)", 25.0, 1e-4, 1e4);
    holeCounterboreDia = spin("holeCounterboreDia", "Counterbore diameter (mm)", 14.0, 1e-4, 1e4);
    holeCounterboreDepth = spin("holeCounterboreDepth", "Counterbore depth (mm)", 4.2, 1e-4, 1e4);
    holeCountersinkDia = spin("holeCountersinkDia", "Countersink diameter (mm)", 14.0, 1e-4, 1e4);
    holeCountersinkAngle =
        spin("holeCountersinkAngle", "Countersink angle (deg)", 90.0, 1.0, 179.0);
    holeDrillAngle = spin("holeDrillAngle", "Drill tip angle (deg)", 118.0, 1.0, 179.0);
    holeTermination = combo("holeTermination", "Termination mode");
    holeTermination->addItems(
        {"ThroughAll", "Blind", "UpToNext", "UpToVertex", "UpToSurface", "OffsetFromSurface"});

    // Fillet
    filletMethod = combo("filletMethod", "Fillet type");
    filletMethod->addItems({"Constant", "MultipleRadius", "Variable", "Face", "FullRound"});
    filletRadius = spin("filletRadius", "Fillet radius (mm)", 5.0, 1e-4, 1e4);
    filletPropagateTangent = check("filletPropagateTangent", "Tangent propagation");
    filletPropagateTangent->setChecked(true);
    filletEdges = new QListWidget;
    filletEdges->setObjectName("filletEdges");
    filletEdges->setMaximumHeight(130);
    form->addRow("Edges to fillet", filletEdges);
    filletVariableEdge = combo("filletVariableEdge", "Variable radius edge");
    auto table = [&](QString id, QString label, QStringList headings) {
        auto *w = new QTableWidget(0, headings.size());
        w->setObjectName(id);
        w->setHorizontalHeaderLabels(headings);
        w->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        w->setMaximumHeight(170);
        form->addRow(label, w);
        connect(w, &QTableWidget::cellChanged, this, [this] { invalidate(); });
        return w;
    };
    filletRadii = table("filletRadii", "Radius per edge", {"Edge", "Radius"});
    filletControlPoints =
        table("filletControlPoints", "Radius along edge", {"Parameter (0–1)", "Radius"});
    filletTransition = combo("filletTransition", "Radius transition");
    filletTransition->addItems({"Linear", "Smooth"});
    filletTransition->setItemData(0, "Linear transition supports two endpoints", Qt::ToolTipRole);
    addFilletPoint = new QPushButton("Add radius point");
    removeFilletPoint = new QPushButton("Remove selected radius point");
    form->addRow(addFilletPoint);
    form->addRow(removeFilletPoint);
    connect(filletEdges, &QListWidget::itemChanged, this, [this] {
        if (!installing) {
            installFilletRadii();
            invalidate();
        }
    });
    connect(addFilletPoint, &QPushButton::clicked, this, [this] {
        int row = filletControlPoints->rowCount() - 1;
        if (row < 1 || row >= 31)
            return;
        bool a, b;
        double before = filletControlPoints->item(row - 1, 0)->text().toDouble(&a);
        double after = filletControlPoints->item(row, 0)->text().toDouble(&b);
        if (!a || !b || before >= after)
            return;
        auto radius = filletControlPoints->item(row - 1, 1)->text();
        filletControlPoints->insertRow(row);
        filletControlPoints->setItem(
            row, 0, new QTableWidgetItem(QString::number((before + after) / 2, 'g', 12)));
        filletControlPoints->setItem(row, 1, new QTableWidgetItem(radius));
        filletTransition->setCurrentText("Smooth");
    });
    connect(removeFilletPoint, &QPushButton::clicked, this, [this] {
        int row = filletControlPoints->currentRow();
        if (row > 0 && row < filletControlPoints->rowCount() - 1) {
            filletControlPoints->removeRow(row);
            fieldsForKind();
            invalidate();
        }
    });
    for (auto method : {"Face", "FullRound"}) {
        auto *item = qobject_cast<QStandardItemModel *>(filletMethod->model())
                         ->item(filletMethod->findText(method));
        item->setEnabled(false);
        item->setToolTip("Blocked: no verified face-set blend adapter (R1)");
    }
    filletPropagateTangent->setEnabled(false);
    filletPropagateTangent->setToolTip("This OCCT adapter requires tangent propagation");

    // Chamfer
    chamferMethod = combo("chamferMethod", "Chamfer type");
    chamferMethod->addItems({"AngleDistance", "DistanceDistance", "EqualDistance"});
    chamferDistance = spin("chamferDistance", "Distance 1 (mm)", 1.5, 1e-4, 1e4);
    chamferDistance2 = spin("chamferDistance2", "Distance 2 (mm)", 1.5, 1e-4, 1e4);
    chamferAngle = spin("chamferAngle", "Chamfer angle (deg)", 45.0, 1.0, 89.0);
    chamferFlip = check("chamferFlip", "Flip direction / face");
    chamferEdges = new QListWidget;
    chamferEdges->setObjectName("chamferEdges");
    chamferEdges->setMaximumHeight(130);
    form->addRow("Edges to chamfer", chamferEdges);
    connect(chamferEdges, &QListWidget::itemChanged, this, [this] { invalidate(); });
    chamferReferenceFace = combo("chamferReferenceFace", "Chamfer reference face");
    chamferPropagateTangent = check("chamferPropagateTangent", "Tangent propagation");
    chamferPropagateTangent->setChecked(true);
    chamferPropagateTangent->setEnabled(false);
    chamferPropagateTangent->setToolTip("This OCCT adapter requires tangent propagation");

    // Rib
    ribThickness = spin("ribThickness", "Rib thickness (mm)", 13.0, 1e-4, 1e4);
    ribSide = combo("ribSide", "Thickness side");
    ribSide->addItems({"Both", "First", "Second"});
    ribDirection = combo("ribDirection", "Extrusion direction");
    ribDirection->addItems({"Parallel", "Normal"});
    ribFlipMaterial = check("ribFlipMaterial", "Flip material side");

    // Repetition
    patSeedKind = combo("patSeedKind", "Pattern seed kind");
    patSeedKind->addItems({"Feature", "Body", "Face"});
    connect(patSeedKind, &QComboBox::currentIndexChanged, this, [this] {
        if (!installing)
            installPatternSeeds();
    });
    patSeed = combo("patSeed", "Seed object");
    patFaces = new QListWidget;
    patFaces->setObjectName("patFaces");
    patFaces->setMaximumHeight(130);
    patFaces->setToolTip("Select all faces enclosing one closed tool body");
    form->addRow("Closed seed faces", patFaces);
    connect(patFaces, &QListWidget::itemChanged, this, [this] { invalidate(); });
    linearDir1Ref = combo("linearDir1Ref", "Direction 1 reference");
    linearSpacing1 = spin("linearSpacing1", "Direction 1 spacing (mm)", 43.0, 1e-4, 1e4);
    linearCount1 = ispin("linearCount1", "Direction 1 count", 2, 1, 32);
    linearReverse1 = check("linearReverse1", "Reverse direction 1");
    linearDir2Ref = combo("linearDir2Ref", "Direction 2 reference");
    linearSpacing2 = spin("linearSpacing2", "Direction 2 spacing (mm)", 40.0, 1e-4, 1e4);
    linearCount2 = ispin("linearCount2", "Direction 2 count", 1, 1, 32);
    linearReverse2 = check("linearReverse2", "Reverse direction 2");

    circAxisRef = combo("circAxisRef", "Rotation axis");
    circCount = ispin("circCount", "Instance count", 4, 1, 360);
    circEqualSpacing = check("circEqualSpacing", "Equal spacing");
    circEqualSpacing->setChecked(true);
    circAngle = spin("circAngle", "Total angle (deg)", 360.0, 0.1, 360.0);
    circReverse = check("circReverse", "Reverse rotation");

    mirrorPlaneRef = combo("mirrorPlaneRef", "Mirror plane");

    // Cosmetic Thread
    threadEdgeRef = combo("threadEdgeRef", "Cylindrical edge");
    threadDesignation = new QLineEdit;
    threadDesignation->setObjectName("threadDesignation");
    threadDesignation->setText("M10x1.5");
    form->addRow("Thread designation", threadDesignation);
    connect(threadDesignation, &QLineEdit::textChanged, this, [this] { invalidate(); });
    threadStandard = combo("threadStandard", "Thread standard");
    threadStandard->addItems({"ANSI Metric", "ISO", "ANSI Inch"});
    threadType = combo("threadType", "Thread type");
    threadType->addItems({"Machine Thread", "Pipe Thread"});
    threadSize = new QLineEdit;
    threadSize->setObjectName("threadSize");
    threadSize->setText("M10x1.5");
    form->addRow("Thread size", threadSize);
    connect(threadSize, &QLineEdit::textChanged, this, [this] { invalidate(); });
    threadInternal = check("threadInternal", "Internal thread");
    threadInternal->setChecked(true);
    threadMajorDia = spin("threadMajorDia", "Major diameter (mm)", 10.0, 1e-4, 1e4);
    threadMinorDia = spin("threadMinorDia", "Minor diameter (mm)", 8.5, 1e-4, 1e4);
    threadPitch = spin("threadPitch", "Thread pitch (mm)", 1.5, 1e-4, 1e4);
    threadTermination = combo("threadTermination", "Thread termination");
    threadTermination->addItems({"UpToNext", "Blind", "ThroughAll"});
    threadDepth = spin("threadDepth", "Thread depth", 25, 1e-4, 1e4);

    // CM7 Derived Curve Controls
    projectedMode = combo("projectedMode", "Projected curve mode");
    projectedMode->addItems({"SketchOnSketch", "SketchOnFaces"});
    connect(projectedMode, &QComboBox::currentIndexChanged, this, [this] {
        fieldsForKind();
        invalidate();
    });

    projectedSketch1 = combo("projectedSketch1", "First sketch");
    connect(projectedSketch1, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    projectedSketch2 = combo("projectedSketch2", "Second sketch");
    connect(projectedSketch2, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });

    projectedFaceSketch = combo("projectedFaceSketch", "Source sketch");
    connect(projectedFaceSketch, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });

    projectedFaces = new QListWidget;
    projectedFaces->setObjectName("projectedFaces");
    projectedFaces->setMaximumHeight(100);
    form->addRow("Faces to project onto", projectedFaces);
    connect(projectedFaces, &QListWidget::itemChanged, this, [this] { invalidate(); });

    projectedVectorMode = combo("projectedVectorMode", "Projection direction mode");
    projectedVectorMode->addItems({"AlongVector", "NormalToFace"});
    connect(projectedVectorMode, &QComboBox::currentIndexChanged, this, [this] {
        fieldsForKind();
        invalidate();
    });

    projectedDirection = combo("projectedDirection", "Projection vector direction");
    connect(projectedDirection, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });

    projectedBranch = ispin("projectedBranch", "Branch index", 0, 0, 16);
    connect(projectedBranch, &QSpinBox::valueChanged, this, [this] { invalidate(); });

    projectedBranchLabel = new QLabel;
    projectedBranchLabel->setObjectName("projectedBranchLabel");
    form->addRow("Branch selection", projectedBranchLabel);

    projectedReversed = check("projectedReversed", "Reverse curve direction");
    projectedStartParam = spin("projectedStartParam", "Start parameter (0..1)", 0.0, 0.0, 1.0);
    projectedEndParam = spin("projectedEndParam", "End parameter (0..1)", 1.0, 0.0, 1.0);

    compositeMembers = new QListWidget;
    compositeMembers->setObjectName("compositeMembers");
    compositeMembers->setMaximumHeight(120);
    form->addRow("Composite members", compositeMembers);
    connect(compositeMembers, &QListWidget::itemChanged, this, [this] { invalidate(); });

    compositeAddMember = new QPushButton("Add member");
    compositeAddMember->setObjectName("compositeAddMember");
    form->addRow(compositeAddMember);

    compositeRemoveMember = new QPushButton("Remove member");
    compositeRemoveMember->setObjectName("compositeRemoveMember");
    form->addRow(compositeRemoveMember);

    compositeReverseMember = new QPushButton("Toggle reverse");
    compositeReverseMember->setObjectName("compositeReverseMember");
    form->addRow(compositeReverseMember);

    compositeClosed = check("compositeClosed", "Closed path");
    compositeDiagnostic = new QLabel;
    compositeDiagnostic->setObjectName("compositeDiagnostic");
    form->addRow("Path diagnostic", compositeDiagnostic);

    helixMode = combo("helixMode", "Helix / spiral mode");
    helixMode->addItems({"PitchAndRevolution", "HeightAndRevolution", "HeightAndPitch", "Spiral"});
    connect(helixMode, &QComboBox::currentIndexChanged, this, [this] {
        fieldsForKind();
        invalidate();
    });

    helixAxis = combo("helixAxis", "Helix axis");
    connect(helixAxis, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });

    helixCenter = combo("helixCenter", "Helix center (optional)");
    connect(helixCenter, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });

    helixRadius = spin("helixRadius", "Starting radius (mm)", 15.0, 1e-4, 1e4);
    helixPitch = spin("helixPitch", "Pitch (mm)", 5.0, 1e-4, 1e4);
    helixHeight = spin("helixHeight", "Height (mm)", 20.0, 0.0, 1e4);
    helixRevolutions = spin("helixRevolutions", "Revolutions", 4.0, 1e-4, 1000.0);
    helixStartAngle = spin("helixStartAngle", "Start angle (deg)", 0.0, 0.0, 360.0);
    helixTaperAngle = spin("helixTaperAngle", "Taper angle (deg)", 0.0, -89.0, 89.0);
    helixClockwise = check("helixClockwise", "Clockwise");
    helixClockwise->setChecked(true);

    // CM8 Shell
    shellThickness = spin("shellThickness", "Shell thickness (mm)", 5.0, 1e-4, 1e4);
    shellInward = check("shellInward", "Shell inward");
    shellInward->setChecked(true);
    shellRemoveFaces = new QListWidget;
    shellRemoveFaces->setObjectName("shellRemoveFaces");
    shellRemoveFaces->setMaximumHeight(130);
    form->addRow("Faces to remove", shellRemoveFaces);
    connect(shellRemoveFaces, &QListWidget::itemChanged, this, [this] { invalidate(); });

    shellOverrides = table("shellOverrides", "Multi-thickness face overrides", {"Face", "Thickness (mm)"});
    addShellOverride = new QPushButton("Add face override");
    addShellOverride->setObjectName("addShellOverride");
    removeShellOverride = new QPushButton("Remove selected face override");
    removeShellOverride->setObjectName("removeShellOverride");
    form->addRow(addShellOverride);
    form->addRow(removeShellOverride);
    connect(addShellOverride, &QPushButton::clicked, this, [this] {
        int r = shellOverrides->rowCount();
        shellOverrides->insertRow(r);
        auto *faceItem = new QTableWidgetItem("face/0");
        shellOverrides->setItem(r, 0, faceItem);
        auto *thickItem = new QTableWidgetItem("10.0");
        shellOverrides->setItem(r, 1, thickItem);
        invalidate();
    });
    connect(removeShellOverride, &QPushButton::clicked, this, [this] {
        int r = shellOverrides->currentRow();
        if (r >= 0) {
            shellOverrides->removeRow(r);
            invalidate();
        }
    });

    // CM8 Sweep
    sweepProfileType = combo("sweepProfileType", "Sweep profile type");
    sweepProfileType->addItems({"Circular", "Sketch"});
    connect(sweepProfileType, &QComboBox::currentIndexChanged, this, [this] {
        fieldsForKind();
        invalidate();
    });
    sweepDiameter = spin("sweepDiameter", "Circular diameter (mm)", 20.0, 1e-4, 1e4);
    sweepPath = combo("sweepPath", "Sweep path");
    sweepPathReverse = check("sweepPathReverse", "Reverse sweep path");
    sweepGuides = new QListWidget;
    sweepGuides->setObjectName("sweepGuides");
    sweepGuides->setMaximumHeight(120);
    form->addRow("Guide curves", sweepGuides);
    connect(sweepGuides, &QListWidget::itemChanged, this, [this] { invalidate(); });
    sweepCornerTransition = combo("sweepCornerTransition", "Corner transition");
    sweepCornerTransition->addItems({"Round", "Sharp"});
    sweepStartTangency = combo("sweepStartTangency", "Start tangency");
    sweepStartTangency->addItems({"None", "PathTangent"});
    sweepEndTangency = combo("sweepEndTangency", "End tangency");
    sweepEndTangency->addItems({"None", "PathTangent"});
    sweepPreviewSections = ispin("sweepPreviewSections", "Preview sections (0–32)", 0, 0, 32);

    // CM8 Loft
    loftOutput = combo("loftOutput", "Loft output");
    loftOutput->addItems({"Solid", "Surface"});
    connect(loftOutput, &QComboBox::currentIndexChanged, this, [this] {
        fieldsForKind();
        invalidate();
    });
    loftSections = new QListWidget;
    loftSections->setObjectName("loftSections");
    loftSections->setMaximumHeight(130);
    form->addRow("Loft sections (ordered)", loftSections);
    connect(loftSections, &QListWidget::itemChanged, this, [this] { invalidate(); });
    addLoftSection = new QPushButton("Add section to loft");
    addLoftSection->setObjectName("addLoftSection");
    removeLoftSection = new QPushButton("Remove selected section");
    removeLoftSection->setObjectName("removeLoftSection");
    moveUpLoftSection = new QPushButton("Move section up");
    moveUpLoftSection->setObjectName("moveUpLoftSection");
    moveDownLoftSection = new QPushButton("Move section down");
    moveDownLoftSection->setObjectName("moveDownLoftSection");
    form->addRow(addLoftSection);
    form->addRow(removeLoftSection);
    form->addRow(moveUpLoftSection);
    form->addRow(moveDownLoftSection);
    connect(addLoftSection, &QPushButton::clicked, this, [this] {
        if (profile->count() > 0) {
            auto *item = new QListWidgetItem(profile->currentText(), loftSections);
            item->setData(Qt::UserRole, profile->currentData());
            item->setCheckState(Qt::Checked);
            invalidate();
        }
    });
    connect(removeLoftSection, &QPushButton::clicked, this, [this] {
        int r = loftSections->currentRow();
        if (r >= 0) {
            delete loftSections->takeItem(r);
            invalidate();
        }
    });
    connect(moveUpLoftSection, &QPushButton::clicked, this, [this] {
        int r = loftSections->currentRow();
        if (r > 0) {
            auto *item = loftSections->takeItem(r);
            loftSections->insertItem(r - 1, item);
            loftSections->setCurrentRow(r - 1);
            invalidate();
        }
    });
    connect(moveDownLoftSection, &QPushButton::clicked, this, [this] {
        int r = loftSections->currentRow();
        if (r >= 0 && r < loftSections->count() - 1) {
            auto *item = loftSections->takeItem(r);
            loftSections->insertItem(r + 1, item);
            loftSections->setCurrentRow(r + 1);
            invalidate();
        }
    });
    loftCenterline = combo("loftCenterline", "Centerline curve");
    loftCenterlineReverse = check("loftCenterlineReverse", "Reverse centerline");
    loftGuides = new QListWidget;
    loftGuides->setObjectName("loftGuides");
    loftGuides->setMaximumHeight(120);
    form->addRow("Loft guide curves", loftGuides);
    connect(loftGuides, &QListWidget::itemChanged, this, [this] { invalidate(); });
    loftConnectors = table("loftConnectors", "Connector correspondence", {"Connector ID", "Points (parameters)"});
    addLoftConnector = new QPushButton("Add connector handle");
    addLoftConnector->setObjectName("addLoftConnector");
    removeLoftConnector = new QPushButton("Remove selected connector");
    removeLoftConnector->setObjectName("removeLoftConnector");
    form->addRow(addLoftConnector);
    form->addRow(removeLoftConnector);
    connect(addLoftConnector, &QPushButton::clicked, this, [this] {
        int r = loftConnectors->rowCount();
        loftConnectors->insertRow(r);
        auto *idItem = new QTableWidgetItem(QUuid::createUuid().toString(QUuid::WithoutBraces));
        loftConnectors->setItem(r, 0, idItem);
        auto *ptsItem = new QTableWidgetItem("0.0, 0.0");
        loftConnectors->setItem(r, 1, ptsItem);
        invalidate();
    });
    connect(removeLoftConnector, &QPushButton::clicked, this, [this] {
        int r = loftConnectors->currentRow();
        if (r >= 0) {
            loftConnectors->removeRow(r);
            invalidate();
        }
    });
    loftStartCondition = combo("loftStartCondition", "Start condition");
    loftStartCondition->addItems({"None", "NormalToSection"});
    loftEndCondition = combo("loftEndCondition", "End condition");
    loftEndCondition->addItems({"None", "NormalToSection"});

    selectDraftFaces = new QPushButton("Select faces to draft…");
    selectDraftFaces->setObjectName("featureSelectDraftFaces");
    form->addRow(selectDraftFaces);
    connect(selectDraftFaces, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setObjectName("featureDraftFacesDialog");
        dialog.setWindowTitle("Faces to draft");
        auto *layout = new QVBoxLayout(&dialog);
        auto *list = new QListWidget;
        list->setObjectName("featureDraftFaceList");
        layout->addWidget(list);
        auto selected = definitionModeling().value("faces").toArray();
        for (int i = 0; i < draftFaces->count(); ++i) {
            auto ref = draftFaces->itemData(i).value<QCborMap>();
            if (ref.value("role") != "Face")
                continue;
            auto *item = new QListWidgetItem(draftFaces->itemText(i), list);
            item->setData(Qt::UserRole, QVariant::fromValue(ref));
            item->setCheckState(selected.contains(ref) ? Qt::Checked : Qt::Unchecked);
        }
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        QCborArray faces;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked)
                faces.append(list->item(i)->data(Qt::UserRole).value<QCborMap>());
        draftFaces->addItem(QString("Selected faces (%1)").arg(faces.size()),
                            QVariant::fromValue(faces));
        draftFaces->setCurrentIndex(draftFaces->count() - 1);
    });
    selectRegions = new QPushButton("Select contour boundaries…");
    selectRegions->setObjectName("featureSelectRegions");
    form->addRow(selectRegions);
    connect(selectRegions, &QPushButton::clicked, this, &FeaturePanel::chooseRegions);
    connect(client, &Client::modelingInputs, this, &FeaturePanel::installModelingInputs);
    // Repair controls
    repairPath = combo("featureRepairPath", "Reference to repair");
    repairPath->addItems({"direction1/reference", "start/reference", "direction2/reference", "axis",
                          "neutral", "pull", "target/source", "placement/face", "edges/0", "edge",
                          "radii/0/edge", "plane", "reference", "reference_face"});
    for (int i = 0; i < 32; ++i) {
        repairPath->addItem(QString("faces/%1").arg(i));
        repairPath->addItem(QString("seed/faces/%1").arg(i));
    }
    repairCandidate = combo("featureRepairCandidate", "Repair candidate");
    applyRepairButton = new QPushButton("Apply repair");
    applyRepairButton->setObjectName("featureApplyRepair");
    form->addRow(applyRepairButton);
    form->setRowVisible(repairCandidate, false);
    form->setRowVisible(applyRepairButton, false);
    connect(applyRepairButton, &QPushButton::clicked, this, [this] {
        if (!editing || repairCandidate->count() == 0)
            return;
        auto chosen = repairCandidate->currentData().value<QCborMap>();
        if (chosen.isEmpty())
            return;
        try {
            domain::check(
                repairResolution.value("revision").toInteger() == client->state().revision &&
                    repairIntents == definitionsModeling() &&
                    repairPath->currentText() == repairResolution.value("path").toString() &&
                    repairResolution.value("replacements").toArray().contains(chosen),
                "StaleRevision", "Inspect the current draft again before repair");
            auto replaced = features::replaceIntentReference(
                repairIntents, draft.id, repairResolution.value("path").toString(),
                domain::GeometryReference::parse(chosen));
            auto metadata = document.value("modeling").toMap();
            metadata.insert(QStringLiteral("intents"), replaced);
            document.insert(QStringLiteral("modeling"), metadata);
            auto owner = draft.id;
            editing = false;
            previewAfterInputs = true;
            begin({}, owner);
        } catch (const std::exception &e) {
            diagnostic->setText(e.what());
        }
    });
    for (auto *c : {profileMode, startMode, extent, extent2, thinMode, operation, revolveExtent,
                    holeFamily, holeTermination, filletMethod, chamferMethod, ribSide, ribDirection,
                    patSeedKind, threadStandard, threadType, threadTermination, filletTransition})
        connect(c, &QComboBox::currentIndexChanged, this, [this] { fieldsForKind(); });
    for (auto *k :
         {draftEnable, direction2Enable, draft2Enable, flipSide, thinReverse,
          filletPropagateTangent, chamferFlip, ribFlipMaterial, linearReverse1, linearReverse2,
          circEqualSpacing, circReverse, threadInternal, holeCustomSizing})
        connect(k, &QCheckBox::toggled, this, [this] { fieldsForKind(); });
    connect(name, &QLineEdit::textChanged, this, [this] { invalidate(); });
    evaluation = new QWidget;
    auto *evaluationLayout = new QVBoxLayout(evaluation);
    evaluationLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(evaluation);
    measurement = new QLabel;
    measurement->setObjectName("partMeasurement");
    measurement->setWordWrap(true);
    measurement->setTextFormat(Qt::PlainText);
    evaluationLayout->addWidget(measurement);
    auto *angleEdge = new QComboBox;
    angleEdge->setObjectName("measurementSecondEdge");
    angleEdge->addItem("Second straight edge for angle (optional)", QString());
    evaluationLayout->addWidget(angleEdge);
    auto *measureButton = new QPushButton("Measure selected geometry");
    measureButton->setObjectName("measurePartButton");
    evaluationLayout->addWidget(measureButton);
    connect(measureButton, &QPushButton::clicked, this, [this, angleEdge] {
        if (editing)
            return;
        auto s = client->state().selection;
        if (angleEdge->currentData().toString().isEmpty()) {
            client->measure();
            return;
        }
        if (s.kind != "Edge") {
            diagnostic->setText("Select the first straight edge in the viewport.");
            return;
        }
        client->projectCommand("MeasurePart",
                               fields({{"body", s.entity.left(36)},
                                       {"token", s.entity.mid(37).section("/segment/", 0, 0)},
                                       {"second_token", angleEdge->currentData().toString()},
                                       {"density_kg_m3", 0.}}));
    });
    connect(client, &Client::partResult, this, [angleEdge](QCborMap info) {
        if (!info.contains(QStringLiteral("topology")))
            return;
        auto selected = angleEdge->currentData();
        angleEdge->clear();
        angleEdge->addItem("Second straight edge for angle (optional)", QString());
        for (auto value : info.value("topology").toArray()) {
            auto m = value.toMap();
            auto token = m.value("token").toString();
            if (token.startsWith("edge/")) {
                auto c = m.value("center_si").toArray();
                angleEdge->addItem(QString("Edge center (%1, %2, %3) m")
                                       .arg(c[0].toDouble(), 0, 'g', 4)
                                       .arg(c[1].toDouble(), 0, 'g', 4)
                                       .arg(c[2].toDouble(), 0, 'g', 4),
                                   token);
            }
        }
        auto index = angleEdge->findData(selected);
        if (index >= 0)
            angleEdge->setCurrentIndex(index);
    });
    density = new QDoubleSpinBox;
    density->setObjectName("materialDensity");
    density->setRange(0, 1e6);
    density->setSuffix(" kg/m³");
    density->setValue(7800);
    evaluationLayout->addWidget(density);
    auto *assign = new QPushButton("Assign material density");
    assign->setObjectName("assignDensityButton");
    evaluationLayout->addWidget(assign);
    threadShadedDisplay = new QCheckBox("Show cosmetic thread overlay");
    threadShadedDisplay->setObjectName("threadShadedDisplay");
    threadShadedDisplay->setChecked(true);
    evaluationLayout->addWidget(threadShadedDisplay);
    connect(threadShadedDisplay, &QCheckBox::toggled, this, [this](bool value) {
        if (!installing && !editing && client->ready() && !client->outstanding())
            client->projectCommand("SetParameter", fields({{"name", "DisplayCosmeticThreads"},
                                                           {"dimension", "scalar"},
                                                           {"value_si", value ? 1.0 : 0.0}}));
    });
    connect(assign, &QPushButton::clicked, this, [this] {
        if (!editing && density->value() > 0)
            client->projectCommand("SetParameter", fields({{"name", "MaterialDensityKgM3"},
                                                           {"dimension", "scalar"},
                                                           {"value_si", density->value()}}));
    });
    connect(previewButton, &QPushButton::clicked, this, &FeaturePanel::preview);
    connect(client, &Client::modelSupportsPreview, this, [this](QCborMap preview) {
        if (!editing || !supportPreviewPending)
            return;
        supportPreviewPending = false;
        validPreview = true;
        auto m = preview.value("measurement").toMap();
        result.insert(QStringLiteral("part_measurement"), m);
        diagnostic->setText("Exact preview ready · review or click Accept feature");
        measurement->setText(QString("Exact preview revision %1\nVolume: %2 m³")
                                 .arg(m.value("revision").toInteger())
                                 .arg(m.value("volume_m3").toDouble(), 0, 'g', 12));
        accept->setEnabled(!client->outstanding());
        updateManipulator();
    });
    connect(accept, &QPushButton::clicked, this, &FeaturePanel::finish);
    connect(cancelButton, &QPushButton::clicked, this, &FeaturePanel::discard);
    auto *repairSupports = new QPushButton("Repair supports in this feature draft");
    repairSupports->setObjectName("featureRepairSupports");
    repairButton = repairSupports;
    layout->addWidget(repairSupports);
    repairSupports->hide();
    connect(client, &Client::changed, this, [this, repairSupports] {
        repairSupports->setVisible(editing &&
                                   (document.contains(QStringLiteral("model_supports")) ||
                                    document.contains(QStringLiteral("modeling"))));
        repairSupports->setEnabled(editing && !client->outstanding() && !commitPending);
    });
    connect(client, &Client::supportReferenceResolution, this, [this](QCborMap resolution) {
        if (!editing)
            return;
        repairCandidate->clear();
        repairResolution = resolution;
        QCborArray list = resolution.contains(QStringLiteral("replacements"))
                              ? resolution.value(QStringLiteral("replacements")).toArray()
                              : resolution.value(QStringLiteral("candidates")).toArray();
        for (auto val : list) {
            if (val.isMap()) {
                auto ref = val.toMap();
                auto role = ref.value("role").toString();
                auto topo = ref.value("topology").toString();
                auto obj = ref.value("object").toString();
                QString label = QString("%1: %2").arg(role, topo.isEmpty() ? obj : topo);
                repairCandidate->addItem(label, QVariant::fromValue(ref));
            } else if (val.isString()) {
                QString token = val.toString();
                domain::GeometryReference r;
                r.object = draft.id;
                r.role = token.startsWith("vertex/") ? "Vertex" : "Face";
                r.topology = token;
                repairCandidate->addItem(QString("%1: %2").arg(r.role, token),
                                         QVariant::fromValue(r.map()));
            }
        }
        if (repairCandidate->count() > 0) {
            form->setRowVisible(repairCandidate, true);
            form->setRowVisible(applyRepairButton, true);
            diagnostic->setText("Select replacement candidate and click Apply repair.");
        }
    });
    connect(repairSupports, &QPushButton::clicked, this, [this] {
        if (isModeling) {
            try {
                auto model = model::editableSupports(document);
                auto cand = definitionModeling();
                QString path = repairPath->currentText();
                repairIntents = definitionsModeling();
                client->projectCommand("ModelingReferenceCandidates",
                                       fields({{"model", model.map()},
                                               {"intents", repairIntents},
                                               {"owner", cand.value("id").toString()},
                                               {"path", path}}));
            } catch (const std::exception &e) {
                diagnostic->setText(e.what());
            }
            return;
        }
        try {
            auto model = model::editSupportedFeatures(document, definitions());
            SupportEditor dialog(client, document, this, {}, model);
            if (dialog.exec() == QDialog::Accepted) {
                editing = false;
                inputs->hide();
                accept->hide();
                previewButton->hide();
                cancelButton->hide();
                emit editingChanged();
            } else
                invalidate();
        } catch (const std::exception &e) {
            diagnostic->setText(e.what());
        }
    });
    auto *timingGroup = new QGroupBox("Feature timings");
    timingGroup->setCheckable(true);
    timingGroup->setChecked(false);
    auto *timingLayout = new QVBoxLayout(timingGroup);
    auto *timings = new QLabel;
    timings->setTextFormat(Qt::PlainText);
    timings->setWordWrap(true);
    timingLayout->addWidget(timings);
    timings->hide();
    evaluationLayout->addWidget(timingGroup);
    connect(timingGroup, &QGroupBox::toggled, timings, &QWidget::setVisible);
    connect(client, &Client::partResult, this, [this, timings](QCborMap info) {
        if (info.contains(QStringLiteral("topology")))
            result = info;
        else
            result.insert(QStringLiteral("part_measurement"), info.value("part_measurement"));
        if (editing && client->state().phase == Phase::PreviewReady) {
            validPreview = true;
            diagnostic->setText(
                "Preview · not committed. Accept records one recoverable transaction.");
        }
        auto m = info.value("part_measurement").toMap();
        QStringList lines;
        auto unitName = document.value("units").toString();
        double u = unit();
        for (auto key : {"volume_m3", "area_m2", "length_m", "radius_m", "angle_rad", "mass_kg"})
            if (m.contains(QString(key))) {
                double value = m.value(QString(key)).toDouble();
                QString suffix;
                if (QString(key) == "mass_kg")
                    suffix = "kg";
                else if (QString(key) == "angle_rad")
                    suffix = "rad";
                else {
                    int power = QString(key).endsWith("m3")   ? 3
                                : QString(key).endsWith("m2") ? 2
                                                              : 1;
                    value /= std::pow(u, power);
                    suffix = unitName + (power == 3 ? "³" : power == 2 ? "²" : "");
                }
                auto label = QString(key) == "volume_m3"   ? "Volume"
                             : QString(key) == "area_m2"   ? "Area"
                             : QString(key) == "length_m"  ? "Length"
                             : QString(key) == "radius_m"  ? "Radius"
                             : QString(key) == "angle_rad" ? "Angle"
                                                           : "Mass";
                lines.append(QString(label) + ": " + QString::number(value, 'g', 12) + " " +
                             suffix);
            }
        QStringList timingLines;
        for (auto d : info.value("feature_diagnostics").toArray()) {
            auto m = d.toMap();
            QString featureName = "Feature";
            for (auto value : document.value("features").toArray())
                if (value.toMap().value("id") == m.value("id"))
                    featureName = value.toMap().value("name").toString();
            if (m.value("id").toString() == draft.id)
                featureName = draft.name;
            timingLines.append(featureName + ": " +
                               QString::number(m.value("milliseconds").toDouble(), 'f', 2) + " ms");
        }
        timings->setText(timingLines.join('\n'));
        measurement->setText(QString("Exact result revision %1%2\n")
                                 .arg(m.value("revision").toInteger())
                                 .arg(editing ? " · Preview" : "") +
                             lines.join('\n'));
        accept->setEnabled(editing && validPreview && client->outstanding() == 0);
    });
    connect(client, &Client::projectChanged, this, [this] {
        if (commitPending) {
            commitPending = false;
            editing = false;
            inputs->hide();
            accept->hide();
            previewButton->hide();
            cancelButton->hide();
            diagnostic->setText("Accepted · recoverable semantic revision");
            emit clearManipulatorRequested();
            emit editingChanged();
        }
    });
    connect(client, &Client::changed, this, [this] {
        if (editing) {
            bool idle = !client->outstanding();
            accept->setEnabled(idle && validPreview &&
                               (isDerivedCurve || client->state().phase == Phase::PreviewReady));
            previewButton->setEnabled(idle);
            if (client->state().phase == Phase::Failed) {
                supportPreviewPending = false;
                validPreview = false;
                commitPending = false;
                diagnostic->setText(client->state().message);
            }
        }
    });
    inputs->hide();
    accept->hide();
    previewButton->hide();
    cancelButton->hide();
}
double FeaturePanel::unit() const {
    auto u = document.value("units").toString();
    return u.isEmpty() ? .001 : domain::toSI(1, "length", u);
}
void FeaturePanel::invalidate() {
    if (installing || !editing)
        return;
    validPreview = false;
    accept->setEnabled(false);
    diagnostic->setText("Inputs changed · preview required");
    updateManipulator();
}
void FeaturePanel::install(QCborMap active, bool idle) {
    bool showThreads = true;
    for (auto value : active.value("parameters").toArray())
        if (value.toMap().value("name") == "DisplayCosmeticThreads")
            showThreads = value.toMap().value("value_si").toDouble() != 0;
    {
        QSignalBlocker block(threadShadedDisplay);
        threadShadedDisplay->setChecked(showThreads);
    }
    threadShadedDisplay->setEnabled(idle && !editing && !active.empty());
    if (editing) {
        inputs->setEnabled(idle);
        cancelButton->setEnabled(!commitPending);
        return;
    }
    document = active;
    evaluation->setVisible(!editing);
    setEnabled(!active.empty() && idle);
    title->setText("Exact Part");
}
static domain::GeometryReference makeRef(QString object, QString role, QString entity = {},
                                         QString topo = {}) {
    domain::GeometryReference r;
    r.object = object;
    r.role = role;
    r.entity = entity;
    r.topology = topo;
    return r;
}

void FeaturePanel::fieldsForKind() {
    form->setRowVisible(surfaceOffset2, isModeling && direction2Enable->isChecked() &&
                                            extent2->currentText() == "OffsetFromSurface");
    form->setRowVisible(offsetReverse2, isModeling && direction2Enable->isChecked() &&
                                            extent2->currentText() == "OffsetFromSurface");
    form->setRowVisible(selectDraftFaces, isModeling && draft.kind == "Draft");
    form->setRowVisible(selectRegions, isModeling &&
                                           (draft.kind == "Extrude" || draft.kind == "Revolve" ||
                                            draft.kind == "Rib") &&
                                           profileMode->currentText() != "AllClosed");
    form->setRowVisible(repairPath, isModeling);
    form->setRowVisible(handleParameter, isModeling);
    auto kind = draft.kind;
    bool isExtrude = (kind == "Extrude");
    bool isRevolve = (kind == "Revolve");
    bool isDraft = (kind == "Draft");
    bool isSketch = isExtrude || isRevolve;
    bool isFillet = (kind == "Fillet");
    bool isChamfer = (kind == "Chamfer");
    bool isHole = (kind == "Hole");
    bool isRib = (kind == "Rib");
    bool isLinearPattern = (kind == "LinearPattern");
    bool isCircularPattern = (kind == "CircularPattern");
    bool isMirror = (kind == "Mirror");
    bool isCosmeticThread = (kind == "CosmeticThread");
    bool isShell = (kind == "Shell");
    bool isSweep = (kind == "Sweep");
    bool isLoft = (kind == "Loft");
    bool isTreatment = isFillet || isChamfer;
    bool isPattern = isLinearPattern || isCircularPattern || isMirror;

    auto show = [&](QWidget *w, bool yes) {
        if (w)
            form->setRowVisible(w, yes);
    };

    auto hideAll = [&]() {
        QWidget *widgets[] = {profile,
                              profileMode,
                              flipSide,
                              operation,
                              target,
                              startMode,
                              startOffset,
                              startReference,
                              extent,
                              depth,
                              angle,
                              reverseDirection,
                              surfaceOffset2,
                              offsetReverse2,
                              surfaceOffset,
                              offsetReverse,
                              dir1Reference,
                              draftEnable,
                              draftAngle,
                              draftOutward,
                              direction2Enable,
                              extent2,
                              depth2,
                              dir2Reference,
                              draft2Enable,
                              draft2Angle,
                              draft2Outward,
                              thinMode,
                              thickness1,
                              thickness2,
                              thinReverse,
                              revolveAxis,
                              revolveExtent,
                              angle2,
                              draftNeutral,
                              draftPull,
                              draftFaces,
                              draftStandaloneAngle,
                              draftStandaloneOutward,
                              holePlacementFace,
                              holeFamily,
                              holeDiameter,
                              holeDepth,
                              holeCounterboreDia,
                              holeCounterboreDepth,
                              holeCountersinkDia,
                              holeCountersinkAngle,
                              holeDrillAngle,
                              holeTermination,
                              filletMethod,
                              filletRadius,
                              filletPropagateTangent,
                              filletEdges,
                              chamferMethod,
                              chamferDistance,
                              chamferDistance2,
                              chamferAngle,
                              chamferFlip,
                              chamferEdges,
                              ribThickness,
                              ribSide,
                              ribDirection,
                              ribFlipMaterial,
                              patSeedKind,
                              patSeed,
                              patFaces,
                              linearDir1Ref,
                              linearSpacing1,
                              linearCount1,
                              linearReverse1,
                              linearDir2Ref,
                              linearSpacing2,
                              linearCount2,
                              linearReverse2,
                              circAxisRef,
                              circCount,
                              circEqualSpacing,
                              circAngle,
                              circReverse,
                              mirrorPlaneRef,
                              threadEdgeRef,
                              threadDesignation,
                              threadStandard,
                              threadType,
                              threadSize,
                              threadInternal,
                              threadMajorDia,
                              threadMinorDia,
                              threadPitch,
                              threadTermination,
                              threadDepth,
                              holePoints,
                              holeDepthReference,
                              holeEndReference,
                              holeTapDiameter,
                              holeTaperAngle,
                              holeThreadDesignation,
                              holeThreadPitch,
                              holeThreadDepth,
                              holeCustomSizing,
                              holeCatalog,
                              filletVariableEdge,
                              filletTransition,
                              filletRadii,
                              filletControlPoints,
                              addFilletPoint,
                              removeFilletPoint,
                              chamferReferenceFace,
                              chamferPropagateTangent,
                              projectedMode,
                              projectedSketch1,
                              projectedSketch2,
                              projectedFaceSketch,
                              projectedFaces,
                              projectedVectorMode,
                              projectedDirection,
                              projectedBranch,
                              projectedBranchLabel,
                              projectedReversed,
                              projectedStartParam,
                              projectedEndParam,
                              compositeMembers,
                              compositeAddMember,
                              compositeRemoveMember,
                              compositeReverseMember,
                              compositeClosed,
                              compositeDiagnostic,
                              helixMode,
                              helixAxis,
                              helixCenter,
                              helixRadius,
                              helixPitch,
                              helixHeight,
                              helixRevolutions,
                              helixStartAngle,
                              helixTaperAngle,
                              helixClockwise,
                              shellThickness,
                              shellInward,
                              shellRemoveFaces,
                              shellOverrides,
                              addShellOverride,
                              removeShellOverride,
                              sweepProfileType,
                              sweepDiameter,
                              sweepPath,
                              sweepPathReverse,
                              sweepGuides,
                              sweepCornerTransition,
                              sweepStartTangency,
                              sweepEndTangency,
                              sweepPreviewSections,
                              loftOutput,
                              loftSections,
                              addLoftSection,
                              removeLoftSection,
                              moveUpLoftSection,
                              moveDownLoftSection,
                              loftCenterline,
                              loftCenterlineReverse,
                              loftGuides,
                              loftConnectors,
                              addLoftConnector,
                              removeLoftConnector,
                              loftStartCondition,
                              loftEndCondition};
        for (QWidget *w : widgets)
            show(w, false);
    };

    if (isDerivedCurve) {
        hideAll();
        show(source, false);
        show(distribution, false);
        show(size, false);
        show(spacing, false);
        show(count, false);
        show(through, false);
        for (int i = 0; i < 3; ++i) {
            show(origin[i], false);
            show(axis[i], false);
        }
        if (draft.kind == "ProjectedCurve") {
            show(projectedMode, true);
            show(projectedBranch, true);
            show(projectedBranchLabel, true);
            show(projectedReversed, true);
            show(projectedStartParam, true);
            show(projectedEndParam, true);
            if (projectedMode->currentText() == "SketchOnSketch") {
                show(projectedSketch1, true);
                show(projectedSketch2, true);
            } else {
                show(projectedFaceSketch, true);
                show(projectedFaces, true);
                show(projectedVectorMode, true);
                show(projectedDirection, projectedVectorMode->currentText() == "AlongVector");
            }
        } else if (draft.kind == "CompositeCurve") {
            show(compositeMembers, true);
            show(compositeAddMember, true);
            show(compositeRemoveMember, true);
            show(compositeReverseMember, true);
            show(compositeClosed, true);
            show(compositeDiagnostic, true);
        } else if (draft.kind == "HelixSpiral") {
            show(helixMode, true);
            show(helixAxis, true);
            show(helixCenter, true);
            show(helixRadius, true);
            show(helixPitch, true);
            show(helixHeight, true);
            show(helixRevolutions, true);
            show(helixStartAngle, true);
            show(helixTaperAngle, true);
            show(helixClockwise, true);
        }
        return;
    }

    if (isModeling) {
        // Hide legacy controls
        show(source, false);
        show(distribution, false);
        show(size, false);
        show(spacing, false);
        show(count, false);
        show(through, false);
        for (int i = 0; i < 3; ++i) {
            show(origin[i], false);
            show(axis[i], false);
        }

        hideAll();

        if (isDraft) {
            show(target, true);
            show(draftNeutral, true);
            show(draftPull, true);
            show(draftFaces, true);
            show(draftStandaloneAngle, true);
            show(draftStandaloneOutward, true);
        } else if (isExtrude || isRevolve) {
            show(profile, true);
            show(profileMode, true);
            show(operation, true);
            show(target, operation->currentText() != "NewBody");

            bool isOpenCut =
                (profileMode->currentText() == "OpenChains" && thinMode->currentText() == "Solid" &&
                 isExtrude && operation->currentText() == "Cut");
            show(flipSide, isOpenCut);

            show(startMode, isExtrude);
            auto sMode = startMode->currentText();
            show(startOffset, isExtrude && sMode == "Offset");
            show(startReference, isExtrude && (sMode == "Surface" || sMode == "Vertex"));

            show(extent, isExtrude);
            show(depth, isExtrude && (extent->currentText() == "Blind" ||
                                      extent->currentText() == "Midplane"));
            show(angle, isRevolve);
            show(reverseDirection, true);

            auto ext1 = extent->currentText();
            bool dir1RefNeeded = isExtrude && (ext1 == "UpToVertex" || ext1 == "UpToSurface" ||
                                               ext1 == "OffsetFromSurface" || ext1 == "UpToBody");
            show(dir1Reference, dir1RefNeeded);
            show(surfaceOffset, isExtrude && ext1 == "OffsetFromSurface");
            show(offsetReverse, isExtrude && ext1 == "OffsetFromSurface");

            show(draftEnable, isExtrude);
            show(draftAngle, isExtrude && draftEnable->isChecked());
            show(draftOutward, isExtrude && draftEnable->isChecked());

            bool midplane1 = (isExtrude && ext1 == "Midplane") ||
                             (isRevolve && revolveExtent->currentText() == "Midplane");
            show(direction2Enable, !midplane1);
            bool dir2On = !midplane1 && direction2Enable->isChecked();
            if (isRevolve) {
                show(angle2, dir2On);
            } else {
                show(extent2, dir2On);
                auto ext2 = extent2->currentText();
                show(depth2, dir2On && ext2 == "Blind");
                bool dir2RefNeeded = dir2On && (ext2 == "UpToVertex" || ext2 == "UpToSurface" ||
                                                ext2 == "OffsetFromSurface" || ext2 == "UpToBody");
                show(dir2Reference, dir2RefNeeded);
                show(draft2Enable, dir2On);
                show(draft2Angle, dir2On && draft2Enable->isChecked());
                show(draft2Outward, dir2On && draft2Enable->isChecked());
            }

            show(thinMode, true);
            auto tm = thinMode->currentText();
            show(thickness1, tm != "Solid");
            show(thickness2, tm == "TwoDirection");
            show(thinReverse, tm == "OneDirection");

            show(revolveAxis, isRevolve);
            show(revolveExtent, isRevolve);
        } else if (isHole) {
            show(target, true);
            show(profile, true);
            show(holePlacementFace, true);
            show(holeFamily, true);
            show(holePoints, true);
            bool catalog = modelingDraft.value("sizing").toMap().value("mode") == "Catalog";
            show(holeCustomSizing, catalog);
            show(holeCatalog, catalog);
            bool editable = !catalog || holeCustomSizing->isChecked();
            for (auto *w : {holeDiameter, holeDepth, holeCounterboreDia, holeCounterboreDepth,
                            holeCountersinkDia, holeCountersinkAngle, holeDrillAngle,
                            holeTapDiameter, holeTaperAngle, holeThreadPitch, holeThreadDepth})
                w->setEnabled(editable);
            holeFamily->setEnabled(editable);
            holeThreadDesignation->setEnabled(editable);
            show(holeDiameter, true);
            show(holeDepth, holeTermination->currentText() == "Blind");
            auto hFam = holeFamily->currentText();
            show(holeCounterboreDia, hFam == "Counterbore" || hFam == "LegacyCounterboreDrill");
            show(holeCounterboreDepth, hFam == "Counterbore" || hFam == "LegacyCounterboreDrill");
            show(holeCountersinkDia, hFam == "Countersink" || hFam == "LegacyCountersinkDrill");
            show(holeCountersinkAngle, hFam == "Countersink" || hFam == "LegacyCountersinkDrill");
            bool drilled = hFam == "Drilled" || hFam == "LegacySimpleDrilled" ||
                           hFam == "LegacyCounterboreDrill" || hFam == "LegacyCountersinkDrill";
            bool tapered = hFam == "PipeTap" || hFam == "LegacyTapered";
            bool tapped = hFam == "Tapped" || hFam == "PipeTap";
            show(holeDrillAngle, drilled);
            show(holeDepthReference, drilled && holeTermination->currentText() == "Blind");
            show(holeTapDiameter, tapped);
            show(holeTaperAngle, tapered);
            show(holeThreadDesignation, tapped);
            show(holeThreadPitch, tapped);
            show(holeThreadDepth, tapped);
            auto terminationMode = holeTermination->currentText();
            show(holeEndReference, terminationMode == "UpToVertex" ||
                                       terminationMode == "UpToSurface" ||
                                       terminationMode == "OffsetFromSurface");
            show(surfaceOffset, terminationMode == "OffsetFromSurface");
            show(offsetReverse, terminationMode == "OffsetFromSurface");
            auto *terminationItems = qobject_cast<QStandardItemModel *>(holeTermination->model());
            for (int i = 0; i < holeTermination->count(); ++i) {
                auto mode = holeTermination->itemText(i);
                bool supported =
                    tapered ? mode == "Blind" : !drilled || mode == "Blind" || mode == "ThroughAll";
                terminationItems->item(i)->setEnabled(supported);
                terminationItems->item(i)->setToolTip(
                    supported ? QString()
                              : "This hole section does not yet support this termination");
            }
            show(holeTermination, true);
        } else if (isFillet) {
            show(target, true);
            show(filletMethod, true);
            auto fMeth = filletMethod->currentText();
            show(filletRadius, fMeth == "Constant");
            show(filletPropagateTangent, true);
            filletPropagateTangent->setEnabled(!filletPropagateTangent->isChecked());
            show(filletEdges, fMeth == "Constant" || fMeth == "MultipleRadius");
            show(filletRadii, fMeth == "MultipleRadius");
            show(filletVariableEdge, fMeth == "Variable");
            show(filletControlPoints, fMeth == "Variable");
            show(filletTransition, fMeth == "Variable");
            qobject_cast<QStandardItemModel *>(filletTransition->model())
                ->item(0)
                ->setEnabled(filletControlPoints->rowCount() == 2);
            show(addFilletPoint, fMeth == "Variable");
            show(removeFilletPoint, fMeth == "Variable");
        } else if (isChamfer) {
            show(target, true);
            show(chamferMethod, true);
            show(chamferDistance, true);
            auto cMeth = chamferMethod->currentText();
            show(chamferDistance2, cMeth == "DistanceDistance");
            show(chamferAngle, cMeth == "AngleDistance");
            show(chamferFlip, cMeth != "EqualDistance");
            show(chamferEdges, true);
            show(chamferReferenceFace, cMeth != "EqualDistance");
            show(chamferPropagateTangent, true);
            chamferPropagateTangent->setEnabled(!chamferPropagateTangent->isChecked());
        } else if (isRib) {
            show(target, true);
            show(profile, true);
            show(profileMode, true);
            show(ribThickness, true);
            show(ribSide, true);
            show(ribDirection, true);
            show(ribFlipMaterial, true);
        } else if (isLinearPattern) {
            show(operation, true);
            show(target, operation->currentText() != "NewBody");
            show(patSeedKind, true);
            show(patSeed, patSeedKind->currentText() != "Face");
            show(patFaces, patSeedKind->currentText() == "Face");
            show(linearDir1Ref, true);
            show(linearSpacing1, true);
            show(linearCount1, true);
            show(linearReverse1, true);
            show(linearDir2Ref, true);
            show(linearSpacing2, true);
            show(linearCount2, true);
            show(linearReverse2, true);
        } else if (isCircularPattern) {
            show(operation, true);
            show(target, operation->currentText() != "NewBody");
            show(patSeedKind, true);
            show(patSeed, patSeedKind->currentText() != "Face");
            show(patFaces, patSeedKind->currentText() == "Face");
            show(circAxisRef, true);
            show(circCount, true);
            show(circEqualSpacing, true);
            show(circAngle, true);
            show(circReverse, true);
        } else if (isMirror) {
            show(operation, true);
            show(target, operation->currentText() != "NewBody");
            show(patSeedKind, true);
            show(patSeed, patSeedKind->currentText() != "Face");
            show(patFaces, patSeedKind->currentText() == "Face");
            show(mirrorPlaneRef, true);
        } else if (isCosmeticThread) {
            show(target, true);
            show(threadEdgeRef, true);
            show(threadDesignation, true);
            show(threadStandard, true);
            show(threadType, true);
            show(threadSize, true);
            show(threadInternal, true);
            show(threadMajorDia, true);
            show(threadMinorDia, true);
            show(threadPitch, true);
            show(threadTermination, true);
            show(threadDepth, threadTermination->currentText() == "Blind");
        } else if (isShell) {
            show(target, true);
            show(shellThickness, true);
            show(shellInward, true);
            show(shellRemoveFaces, true);
            show(shellOverrides, true);
            show(addShellOverride, true);
            show(removeShellOverride, true);
        } else if (isSweep) {
            show(operation, true);
            show(target, operation->currentText() != "NewBody");
            show(sweepProfileType, true);
            show(profile, sweepProfileType->currentText() == "Sketch");
            show(sweepDiameter, sweepProfileType->currentText() == "Circular");
            show(sweepPath, true);
            show(sweepPathReverse, true);
            show(sweepGuides, true);
            show(sweepCornerTransition, true);
            show(sweepStartTangency, true);
            show(sweepEndTangency, true);
            show(thinMode, true);
            show(thickness1, thinMode->currentText() != "Solid");
            show(thickness2, thinMode->currentText() == "TwoDirection");
            show(thinReverse, thinMode->currentText() == "OneDirection");
            show(sweepPreviewSections, true);
        } else if (isLoft) {
            show(operation, loftOutput->currentText() == "Solid");
            show(target, loftOutput->currentText() == "Solid" && operation->currentText() != "NewBody");
            show(loftOutput, true);
            show(profile, true);
            show(loftSections, true);
            show(addLoftSection, true);
            show(removeLoftSection, true);
            show(moveUpLoftSection, true);
            show(moveDownLoftSection, true);
            show(loftCenterline, true);
            show(loftCenterlineReverse, true);
            show(loftGuides, true);
            show(loftConnectors, true);
            show(addLoftConnector, true);
            show(removeLoftConnector, true);
            show(loftStartCondition, true);
            show(loftEndCondition, true);
            show(thinMode, loftOutput->currentText() == "Solid");
            show(thickness1, loftOutput->currentText() == "Solid" && thinMode->currentText() != "Solid");
            show(thickness2, loftOutput->currentText() == "Solid" && thinMode->currentText() == "TwoDirection");
            show(thinReverse, loftOutput->currentText() == "Solid" && thinMode->currentText() == "OneDirection");
        }
    } else {
        hideAll();
        // Legacy feature UI
        show(profile, isSketch);
        show(operation, isSketch || (!isTreatment && !isHole));
        show(target, true);
        show(source, !isSketch && !isHole);
        show(extent, isExtrude || isRevolve);
        show(distribution, kind == "CircularPattern");
        show(size, isTreatment || isHole);
        show(depth, isExtrude || isHole);
        show(angle, isRevolve || kind == "CircularPattern");
        show(spacing, kind == "LinearPattern");
        show(count, isPattern && kind != "Mirror");
        show(through, isHole);
        bool vectors = isRevolve || isHole || kind == "Mirror" || kind == "CircularPattern";
        for (int i = 0; i < 3; ++i) {
            show(origin[i], vectors);
            show(axis[i], vectors || kind == "LinearPattern");
        }
    }

    if (auto *label = qobject_cast<QLabel *>(form->labelForField(size)))
        label->setText(isHole ? "Diameter" : kind == "Chamfer" ? "Equal distance" : "Radius");
    auto filterReferences = [&](QComboBox *box, QString mode) {
        if (!isModeling)
            return;
        auto *items = qobject_cast<QStandardItemModel *>(box->model());
        int first = -1;
        for (int i = 0; i < box->count(); ++i) {
            auto ref = box->itemData(i).value<QCborMap>();
            auto role = ref.value("role").toString();
            bool point = role == "Vertex" || role == "Point" ||
                         ((role == "Curve" || role == "Edge") &&
                          (ref.contains(QStringLiteral("endpoint")) ||
                           ref.contains(QStringLiteral("parameter"))));
            bool valid =
                mode == "UpToBody"                           ? role == "Object"
                : (mode == "Vertex" || mode == "UpToVertex") ? point
                : (mode == "Surface" || mode == "UpToSurface" || mode == "OffsetFromSurface")
                    ? (role == "Face" || role == "Plane")
                    : true;
            if (items && items->item(i))
                items->item(i)->setEnabled(valid);
            if (valid && first < 0)
                first = i;
        }
        if (!installing && items && items->item(box->currentIndex()) &&
            !items->item(box->currentIndex())->isEnabled())
            box->setCurrentIndex(first);
    };
    filterReferences(holeEndReference, holeTermination->currentText());
    filterReferences(startReference, startMode->currentText());
    filterReferences(dir1Reference, extent->currentText());
    filterReferences(dir2Reference, extent2->currentText());
    startOffset->setRange(-1000 / unit(), 1000 / unit());
    for (auto *offset : {surfaceOffset, surfaceOffset2})
        offset->setRange(0, 1000 / unit());
    auto units = document.value("units").toString();
    for (auto *w : {size,
                    depth,
                    depth2,
                    spacing,
                    thickness1,
                    thickness2,
                    startOffset,
                    surfaceOffset,
                    surfaceOffset2,
                    holeDiameter,
                    holeDepth,
                    holeCounterboreDia,
                    holeCounterboreDepth,
                    holeCountersinkDia,
                    filletRadius,
                    chamferDistance,
                    chamferDistance2,
                    ribThickness,
                    linearSpacing1,
                    linearSpacing2,
                    threadMajorDia,
                    threadMinorDia,
                    threadPitch,
                    threadDepth,
                    holeTapDiameter,
                    holeThreadPitch,
                    holeThreadDepth})
        if (w)
            w->setSuffix(" " + units);
    for (auto *w : origin)
        if (w)
            w->setSuffix(" " + units);
}

void FeaturePanel::begin(QString kind, QString editId) {
    if (editing || document.empty() || client->outstanding())
        return;
    bool advanced = kind == "Extrude" || kind == "Revolve" || kind == "Draft" || kind == "Hole" ||
                    kind == "Fillet" || kind == "Chamfer" || kind == "Rib" ||
                    kind == "LinearPattern" || kind == "CircularPattern" || kind == "Mirror" ||
                    kind == "CosmeticThread" || kind == "Shell" || kind == "Sweep" || kind == "Loft";
    if (editId.isEmpty() && !advanced &&
        !document.value("modeling").toMap().value("intents").toArray().empty()) {
        diagnostic->setText("This legacy operation cannot follow solid intents.");
        return;
    }
    installing = true;
    candidate = {};
    candidateIntents = {};
    currentRegions = {};
    inputSketches = {};
    inputGeometry = {};

    bool hasLegacyFeatures =
        !document.value("features").toArray().empty() ||
        (!document.value("model_supports").toMap().value("features").toArray().empty());
    bool hasModeling = document.contains(QStringLiteral("modeling")) &&
                       !document.value("modeling").toMap().value("intents").toArray().empty();

    isModeling = false;
    isDerivedCurve = false;
    modelingDraft = {};
    curveDraft = {};
    draft = features::Feature{};

    if (!editId.isEmpty()) {
        if (document.contains(QStringLiteral("modeling"))) {
            for (auto v : document.value("modeling").toMap().value("intents").toArray()) {
                if (v.toMap().value("id").toString() == editId) {
                    isModeling = true;
                    modelingDraft = v.toMap();
                    break;
                }
            }
        }
        if (!isModeling && document.contains(QStringLiteral("model_supports"))) {
            auto sup = model::SupportDefinition::parse(document.value("model_supports").toMap());
            for (const auto &c : model::parseDerivedCurves(sup.derived_curves)) {
                if (c.id == editId) {
                    isDerivedCurve = true;
                    curveDraft = c;
                    kind = c.kind == "HelixSpiral"      ? "HelixSpiral"
                           : c.kind == "CompositeCurve" ? "CompositeCurve"
                                                        : "ProjectedCurve";
                    break;
                }
            }
        }
        if (!isModeling && !isDerivedCurve) {
            auto definitions = document.value("features").toArray();
            for (auto f : definitions) {
                if (f.toMap().value("id").toString() == editId) {
                    draft = features::Feature::parse(f.toMap());
                    break;
                }
            }
        }
    } else {
        if (kind == "ProjectedCurve" || kind == "CompositeCurve" || kind == "HelixSpiral") {
            isDerivedCurve = true;
            isModeling = false;
        } else if (kind == "Draft" || kind == "Rib" || kind == "CosmeticThread" ||
                   kind == "Shell" || kind == "Sweep" || kind == "Loft") {
            isModeling = true;
        } else if (hasModeling) {
            isModeling = true;
        } else if (advanced && !hasLegacyFeatures) {
            isModeling = true;
        } else {
            isModeling = false;
        }
    }

    if (isDerivedCurve) {
        draft.id = editId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : editId;
        draft.kind = kind;
        draft.name = kind;
        name->setText(draft.name);
        title->setText(kind);

        projectedSketch1->clear();
        projectedSketch2->clear();
        projectedFaceSketch->clear();
        for (auto s : document.value("sketches").toArray()) {
            auto sm = s.toMap();
            auto sName = sm.value("name").toString();
            auto sId = sm.value("id").toString();
            projectedSketch1->addItem(sName, sId);
            projectedSketch2->addItem(sName, sId);
            projectedFaceSketch->addItem(sName, sId);
        }
        if (projectedSketch2->count() > 1)
            projectedSketch2->setCurrentIndex(1);

        helixAxis->clear();
        projectedDirection->clear();
        for (auto axName : {"X", "Y", "Z"}) {
            helixAxis->addItem(QString("Axis ") + axName, QString("axis/") + axName);
            projectedDirection->addItem(QString("Axis ") + axName, QString("axis/") + axName);
        }
        if (document.contains(QStringLiteral("model_supports"))) {
            auto sup = model::SupportDefinition::parse(document.value("model_supports").toMap());
            for (const auto &d : sup.datums) {
                if (d.kind == "DatumAxis") {
                    helixAxis->addItem(d.id, d.id);
                    projectedDirection->addItem(d.id, d.id);
                }
            }
        }
        helixAxis->setCurrentIndex(2);

        helixCenter->clear();
        helixCenter->addItem("None (origin)", QString());

        if (!editId.isEmpty()) {
            if (curveDraft.helixSpiral) {
                const auto &h = *curveDraft.helixSpiral;
                helixMode->setCurrentText(h.mode);
                helixRadius->setValue(h.radius / unit());
                helixPitch->setValue(h.pitch / unit());
                helixHeight->setValue(h.height / unit());
                helixRevolutions->setValue(h.revolutions);
                helixStartAngle->setValue(h.startAngle * 180.0 / std::numbers::pi);
                helixTaperAngle->setValue(h.taperAngle * 180.0 / std::numbers::pi);
                helixClockwise->setChecked(h.clockwise);
                int axIdx = helixAxis->findData(h.axis.object);
                if (axIdx >= 0)
                    helixAxis->setCurrentIndex(axIdx);
            } else if (curveDraft.sketchOnSketch) {
                const auto &sos = *curveDraft.sketchOnSketch;
                projectedMode->setCurrentText("SketchOnSketch");
                int s1Idx = projectedSketch1->findData(sos.sketch1.object);
                if (s1Idx >= 0)
                    projectedSketch1->setCurrentIndex(s1Idx);
                int s2Idx = projectedSketch2->findData(sos.sketch2.object);
                if (s2Idx >= 0)
                    projectedSketch2->setCurrentIndex(s2Idx);
                projectedBranch->setValue(sos.branch.index);
                projectedReversed->setChecked(sos.branch.reversed);
                projectedStartParam->setValue(sos.branch.startParam);
                projectedEndParam->setValue(sos.branch.endParam);
            } else if (curveDraft.sketchOnFaces) {
                const auto &sof = *curveDraft.sketchOnFaces;
                projectedMode->setCurrentText("SketchOnFaces");
                int sIdx = projectedFaceSketch->findData(sof.sketch.object);
                if (sIdx >= 0)
                    projectedFaceSketch->setCurrentIndex(sIdx);
                projectedVectorMode->setCurrentText(sof.mode);
                projectedBranch->setValue(sof.branch.index);
                projectedReversed->setChecked(sof.branch.reversed);
                projectedStartParam->setValue(sof.branch.startParam);
                projectedEndParam->setValue(sof.branch.endParam);
            } else if (curveDraft.compositeCurve) {
                compositeClosed->setChecked(curveDraft.compositeCurve->closed);
            }
        }
        editing = true;
        validPreview = false;
        installing = false;
        fieldsForKind();
        invalidate();
        emit editingChanged();
        return;
    }

    if (isModeling) {
        if (!editId.isEmpty() && !modelingDraft.isEmpty()) {
            draft.id = modelingDraft.value("id").toString();
            draft.kind = modelingDraft.value("kind").toString();
            kind = draft.kind;
            draft.name = modelingDraft.contains(QStringLiteral("name"))
                             ? modelingDraft.value("name").toString()
                             : kind;
        } else {
            draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            draft.kind = kind;
            draft.name = kind;
        }
    } else {
        if (editId.isEmpty()) {
            draft.kind = kind;
            draft.name = kind;
        } else {
            kind = draft.kind;
        }
    }

    extent->clear();
    if (kind == "Revolve") {
        extent->addItems({"Blind", "Midplane"});
    } else if (isModeling) {
        extent->addItems({"Blind", "Midplane", "UpToVertex", "UpToSurface", "OffsetFromSurface",
                          "UpToBody", "UpToNext", "ThroughAll"});
    } else {
        extent->addItems({"Forward", "Reverse", "Symmetric"});
    }

    name->setText(draft.name);
    profile->clear();
    source->clear();
    target->clear();
    target->addItem("None", QString());

    for (auto s : document.value("sketches").toArray()) {
        auto m = s.toMap();
        profile->addItem(m.value("name").toString(), m.value("id").toString());
    }
    for (auto value : client->state().documentRecords) {
        auto d = value.toMap();
        if (d.value("id") == document.value("id"))
            continue;
        for (auto s : d.value("sketches").toArray()) {
            auto m = s.toMap();
            profile->addItem(d.value("name").toString() + " / " + m.value("name").toString(),
                             m.value("id").toString());
        }
    }

    auto definitionsList = document.value("features").toArray();
    for (auto f : definitionsList) {
        auto m = f.toMap();
        if (m.value("id").toString() == editId)
            break;
        source->addItem(m.value("name").toString(), m.value("id").toString());
        auto p = m.value("parameters").toMap();
        if (p.value("operation").toString() == "NewBody")
            target->addItem(m.value("name").toString(), m.value("id").toString());
    }
    for (auto id : result.value("bodies").toArray())
        if (target->findData(id.toString()) < 0)
            target->addItem(QString("Body %1").arg(target->count()), id.toString());
    if (target->count() > 1)
        target->setCurrentIndex(1);
    source->setCurrentIndex(source->count() - 1);

    // Populate references for CM5
    startReference->clear();
    dir1Reference->clear();
    dir2Reference->clear();
    revolveAxis->clear();
    draftNeutral->clear();
    draftPull->clear();
    draftFaces->clear();

    auto addRef = [](QComboBox *box, const QString &text, const domain::GeometryReference &r) {
        try {
            box->addItem(text, QVariant::fromValue(r.map()));
        } catch (...) {
        }
    };

    for (auto s : document.value("sketches").toArray()) {
        auto sm = s.toMap();
        auto sName = sm.value("name").toString();
        auto sId = sm.value("id").toString();
        auto planeRef = makeRef(sId, "Plane");
        addRef(startReference, sName + " / Plane", planeRef);
        addRef(dir1Reference, sName + " / Plane", planeRef);
        addRef(dir2Reference, sName + " / Plane", planeRef);
        addRef(draftNeutral, sName + " / Plane", planeRef);

        for (auto entVal : sm.value("entities").toArray()) {
            auto ent = entVal.toMap();
            auto eKind = ent.value("kind").toString();
            auto eId = ent.value("id").toString();
            if (eKind == "Point") {
                auto ptRef = makeRef(sId, "Point", eId);
                addRef(startReference, sName + " / Point " + eId.right(6), ptRef);
                addRef(dir1Reference, sName + " / Point " + eId.right(6), ptRef);
                addRef(dir2Reference, sName + " / Point " + eId.right(6), ptRef);
            } else if (eKind == "Line") {
                auto lineRef = makeRef(sId, "Curve", eId);
                addRef(revolveAxis, sName + " / Line " + eId.right(6), lineRef);
                addRef(draftPull, sName + " / Line " + eId.right(6), lineRef);
            }
        }
    }

    if (document.contains(QStringLiteral("model_supports"))) {
        for (auto datumVal : document.value("model_supports").toMap().value("datums").toArray()) {
            auto dm = datumVal.toMap();
            auto dId = dm.value("id").toString();
            auto dKind = dm.value("kind").toString();
            if (dKind == "DatumPlane") {
                auto r = makeRef(dId, "Plane");
                addRef(startReference, dKind + " " + dId.right(6), r);
                addRef(dir1Reference, dKind + " " + dId.right(6), r);
                addRef(dir2Reference, dKind + " " + dId.right(6), r);
                addRef(draftNeutral, dKind + " " + dId.right(6), r);
            } else if (dKind == "DatumAxis") {
                auto r = makeRef(dId, "Axis");
                addRef(revolveAxis, dKind + " " + dId.right(6), r);
                addRef(draftPull, dKind + " " + dId.right(6), r);
            } else if (dKind == "DatumPoint") {
                auto r = makeRef(dId, "Point", dId);
                addRef(startReference, dKind + " " + dId.right(6), r);
                addRef(dir1Reference, dKind + " " + dId.right(6), r);
                addRef(dir2Reference, dKind + " " + dId.right(6), r);
            }
        }
    }

    // Default values
    operation->setCurrentText(
        (kind == "Extrude" || kind == "Revolve" || kind == "Mirror" || kind.endsWith("Pattern"))
            ? "NewBody"
            : "Cut");
    extent->setCurrentText(isModeling ? "Blind" : "Forward");
    depth->setValue(10);
    depth2->setValue(10);
    size->setValue(2);
    spacing->setValue(10);
    angle->setValue(360);
    angle2->setValue(0);
    count->setValue(2);
    through->setChecked(true);
    reverseDirection->setChecked(false);
    draftEnable->setChecked(false);
    draftAngle->setValue(0);
    draftOutward->setChecked(false);
    draft2Enable->setChecked(false);
    draft2Angle->setValue(0);
    draft2Outward->setChecked(false);
    direction2Enable->setChecked(false);
    thinMode->setCurrentText("Solid");
    thickness1->setValue(1);
    thickness2->setValue(1);
    thinReverse->setChecked(false);
    profileMode->setCurrentText("AllClosed");
    flipSide->setChecked(false);
    startMode->setCurrentText("SketchPlane");
    startOffset->setValue(0);
    surfaceOffset->setValue(0);
    surfaceOffset2->setValue(0);
    offsetReverse2->setChecked(false);
    offsetReverse->setChecked(false);

    for (int i = 0; i < 3; ++i) {
        origin[i]->setValue(0);
        axis[i]->setValue(i == 2 ? 1 : 0);
    }
    if (kind == "Revolve") {
        axis[1]->setValue(1);
        axis[2]->setValue(0);
    }
    if (kind == "LinearPattern") {
        axis[0]->setValue(1);
        axis[2]->setValue(0);
    }
    if (kind == "Hole") {
        axis[2]->setValue(-1);
        auto m = result.value("part_measurement").toMap().value("maximum_m").toArray();
        if (m.size() == 3)
            origin[2]->setValue(m[2].toDouble() / unit());
    }

    if (isModeling) {
        availableModelingInputs = {};
        patSeed->clear();
        patFaces->clear();
        patSeed->setProperty("seedKind", QString());
        for (auto *box :
             {holePlacementFace, holeEndReference, filletVariableEdge, chamferReferenceFace,
              threadEdgeRef, mirrorPlaneRef, circAxisRef, linearDir1Ref, linearDir2Ref})
            box->clear();
        filletEdges->clear();
        chamferEdges->clear();
        holePoints->clear();
        filletRadii->setRowCount(0);
        filletControlPoints->setRowCount(2);
        for (int row = 0; row < 2; ++row) {
            filletControlPoints->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
            filletControlPoints->setItem(
                row, 1, new QTableWidgetItem(QString::number(.005 / unit(), 'g', 12)));
        }
        holeFamily->setCurrentText("Simple");
        holeTermination->setCurrentText("ThroughAll");
        holeDepthReference->setCurrentText("Shoulder");
        holeCustomSizing->setChecked(true);
        filletMethod->setCurrentText("Constant");
        filletTransition->setCurrentText("Linear");
        filletPropagateTangent->setChecked(true);
        chamferPropagateTangent->setChecked(true);
        patSeedKind->setCurrentText("Feature");
        linearCount2->setValue(1);
        for (auto item : {std::pair{holeDiameter, .010},
                          {holeDepth, .025},
                          {holeTapDiameter, .0085},
                          {holeCounterboreDia, .014},
                          {holeCounterboreDepth, .0042},
                          {holeCountersinkDia, .014},
                          {holeThreadPitch, .0015},
                          {holeThreadDepth, .020},
                          {filletRadius, .005},
                          {ribThickness, .013},
                          {linearSpacing1, .043},
                          {linearSpacing2, .040},
                          {threadMajorDia, .010},
                          {threadMinorDia, .0085},
                          {threadPitch, .0015},
                          {threadDepth, .025}})
            item.first->setValue(item.second / unit());
        if (kind == "Rib")
            profileMode->setCurrentText("OpenChains");

        // CM8 resets
        shellRemoveFaces->clear();
        shellOverrides->setRowCount(0);
        shellThickness->setValue(2.0 / unit());
        shellInward->setChecked(true);

        sweepPath->clear();
        sweepGuides->clear();
        sweepProfileType->setCurrentText("Sketch");
        sweepDiameter->setValue(10.0 / unit());
        sweepPathReverse->setChecked(false);
        sweepCornerTransition->setCurrentText("Round");
        sweepStartTangency->setCurrentText("None");
        sweepEndTangency->setCurrentText("None");
        sweepPreviewSections->setValue(0);

        loftOutput->setCurrentText("Solid");
        loftSections->clear();
        loftCenterline->clear();
        loftCenterline->addItem("None", QVariant{});
        loftCenterlineReverse->setChecked(false);
        loftGuides->clear();
        loftConnectors->setRowCount(0);
        loftStartCondition->setCurrentText("None");
        loftEndCondition->setCurrentText("None");

        for (auto s : document.value("sketches").toArray()) {
            auto sm = s.toMap();
            auto sName = sm.value("name").toString();
            auto sId = sm.value("id").toString();
            auto cRef = makeRef(sId, "Curve");
            sweepPath->addItem(sName, QVariant::fromValue(cRef.map()));
            loftCenterline->addItem(sName, QVariant::fromValue(cRef.map()));
        }
        if (document.contains(QStringLiteral("model_supports"))) {
            auto sup = model::SupportDefinition::parse(document.value("model_supports").toMap());
            for (const auto &c : model::parseDerivedCurves(sup.derived_curves)) {
                auto cRef = makeRef(c.id, "Curve");
                sweepPath->addItem(c.name.isEmpty() ? c.id : c.name, QVariant::fromValue(cRef.map()));
                loftCenterline->addItem(c.name.isEmpty() ? c.id : c.name, QVariant::fromValue(cRef.map()));
            }
        }
    }

    auto matchRef = [](QComboBox *box, const QCborMap &targetRef) {
        if (targetRef.isEmpty() || !box)
            return;
        for (int i = 0; i < box->count(); ++i) {
            if (box->itemData(i).value<QCborMap>() == targetRef) {
                box->setCurrentIndex(i);
                return;
            }
        }
        box->addItem("Original reference (requires validation)", QVariant::fromValue(targetRef));
        box->setCurrentIndex(box->count() - 1);
    };

    if (isModeling && !modelingDraft.isEmpty()) {
        auto p = modelingDraft;
        if (p.contains(QStringLiteral("operation")))
            operation->setCurrentText(p.value("operation").toString());
        if (p.contains(QStringLiteral("reverse")))
            reverseDirection->setChecked(p.value("reverse").toBool());
        if (p.contains(QStringLiteral("flip_side")))
            flipSide->setChecked(p.value("flip_side").toBool());

        if (p.contains(QStringLiteral("target")) && !p.value("target").isNull()) {
            auto tgt = p.value("target").toMap();
            auto bId = tgt.value("body").toString();
            int idx = target->findData(bId);
            if (idx >= 0)
                target->setCurrentIndex(idx);
        }

        if (p.contains(QStringLiteral("profile"))) {
            auto prof = p.value("profile").toMap();
            auto sId = prof.value("sketch").toString();
            int idx = profile->findData(sId);
            if (idx >= 0)
                profile->setCurrentIndex(idx);
            profileMode->setCurrentText(prof.value("mode").toString());
            currentRegions = prof.value("regions").toArray();
        }

        if (p.contains(QStringLiteral("start"))) {
            auto st = p.value("start").toMap();
            startMode->setCurrentText(st.value("mode").toString());
            if (st.contains(QStringLiteral("distance")))
                startOffset->setValue(st.value("distance").toDouble() / unit());
            if (st.contains(QStringLiteral("reference")))
                matchRef(startReference, st.value("reference").toMap());
        }

        if (p.contains(QStringLiteral("direction1"))) {
            auto d1 = p.value("direction1").toMap();
            auto mode = d1.value("mode").toString();
            (kind == "Revolve" ? revolveExtent : extent)->setCurrentText(mode);
            surfaceOffset->setValue(d1.value("offset").toDouble() / unit());
            offsetReverse->setChecked(d1.value("offset_reverse").toBool());
            if (mode == "Blind" || mode == "Midplane") {
                if (kind == "Revolve")
                    angle->setValue(d1.value("value").toDouble() * 180.0 / std::numbers::pi);
                else
                    depth->setValue(d1.value("value").toDouble() / unit());
            }
            if (d1.contains(QStringLiteral("reference")))
                matchRef(dir1Reference, d1.value("reference").toMap());
            if (d1.contains(QStringLiteral("draft"))) {
                auto dr = d1.value("draft").toMap();
                double deg = dr.value("angle").toDouble() * 180.0 / std::numbers::pi;
                draftEnable->setChecked(deg > 0);
                draftAngle->setValue(deg);
                draftOutward->setChecked(dr.value("outward").toBool());
            }
        }

        if (p.contains(QStringLiteral("direction2")) && !p.value("direction2").isNull()) {
            auto d2 = p.value("direction2").toMap();
            direction2Enable->setChecked(true);
            auto mode = d2.value("mode").toString();
            extent2->setCurrentText(mode);
            surfaceOffset2->setValue(d2.value("offset").toDouble() / unit());
            offsetReverse2->setChecked(d2.value("offset_reverse").toBool());
            if (mode == "Blind") {
                if (kind == "Revolve")
                    angle2->setValue(d2.value("value").toDouble() * 180.0 / std::numbers::pi);
                else
                    depth2->setValue(d2.value("value").toDouble() / unit());
            }
            if (d2.contains(QStringLiteral("reference")))
                matchRef(dir2Reference, d2.value("reference").toMap());
            if (d2.contains(QStringLiteral("draft"))) {
                auto dr = d2.value("draft").toMap();
                double deg = dr.value("angle").toDouble() * 180.0 / std::numbers::pi;
                draft2Enable->setChecked(deg > 0);
                draft2Angle->setValue(deg);
                draft2Outward->setChecked(dr.value("outward").toBool());
            }
        }

        if (kind == "Revolve" && p.contains(QStringLiteral("axis"))) {
            matchRef(revolveAxis, p.value("axis").toMap());
        }

        if (p.contains(QStringLiteral("thin"))) {
            auto th = p.value("thin").toMap();
            thinMode->setCurrentText(th.value("mode").toString());
            if (th.contains(QStringLiteral("thickness")))
                thickness1->setValue(th.value("thickness").toDouble() / unit());
            if (th.contains(QStringLiteral("thickness1")))
                thickness1->setValue(th.value("thickness1").toDouble() / unit());
            if (th.contains(QStringLiteral("thickness2")))
                thickness2->setValue(th.value("thickness2").toDouble() / unit());
            if (th.contains(QStringLiteral("reverse")))
                thinReverse->setChecked(th.value("reverse").toBool());
        }

        if (kind == "Draft") {
            if (p.contains(QStringLiteral("angle")))
                draftStandaloneAngle->setValue(p.value("angle").toDouble() * 180.0 /
                                               std::numbers::pi);
            if (p.contains(QStringLiteral("outward")))
                draftStandaloneOutward->setChecked(p.value("outward").toBool());
            if (p.contains(QStringLiteral("neutral")))
                matchRef(draftNeutral, p.value("neutral").toMap());
            if (p.contains(QStringLiteral("pull")))
                matchRef(draftPull, p.value("pull").toMap());
            if (p.contains(QStringLiteral("faces"))) {
                auto faces = p.value("faces").toArray();
                if (!faces.isEmpty())
                    matchRef(draftFaces, faces[0].toMap());
            }
        }

        if (kind == "Hole") {
            auto placement = p.value("placement").toMap();
            auto index = profile->findData(placement.value("sketch").toString());
            if (index >= 0)
                profile->setCurrentIndex(index);
            matchRef(holePlacementFace, placement.value("face").toMap());
            auto termination = p.value("termination").toMap();
            holeDepthReference->setCurrentText(termination.value("depth_reference").toString());
            matchRef(holeEndReference, termination.value("reference").toMap());
            surfaceOffset->setValue(termination.value("offset").toDouble() / unit());
            offsetReverse->setChecked(termination.value("offset_reverse").toBool());
            if (p.contains(QStringLiteral("family")))
                holeFamily->setCurrentText(p.value("family").toString());
            if (p.contains(QStringLiteral("sizing"))) {
                auto sz = p.value("sizing").toMap();
                bool catalog = sz.value("mode") == "Catalog";
                holeCustomSizing->setChecked(!catalog);
                holeCatalog->setText(
                    catalog ? sz.value("dataset").toString() + " / " +
                                  sz.value("version").toString("Unversioned legacy") + " / " +
                                  sz.value("standard").toString() + " / " +
                                  sz.value("type").toString() + " / " + sz.value("size").toString()
                            : QString());
                if (sz.contains(QStringLiteral("tap_drill_diameter")))
                    holeTapDiameter->setValue(sz.value("tap_drill_diameter").toDouble() / unit());
                if (sz.contains(QStringLiteral("taper_angle")))
                    holeTaperAngle->setValue(sz.value("taper_angle").toDouble() * 180 /
                                             std::numbers::pi);
                if (sz.contains(QStringLiteral("thread"))) {
                    auto th = sz.value("thread").toMap();
                    holeThreadDesignation->setText(th.value("designation").toString());
                    holeThreadPitch->setValue(th.value("pitch").toDouble() / unit());
                    holeThreadDepth->setValue(th.value("depth").toDouble() / unit());
                }
                if (sz.contains(QStringLiteral("diameter")))
                    holeDiameter->setValue(sz.value("diameter").toDouble() / unit());
                if (sz.contains(QStringLiteral("depth")))
                    holeDepth->setValue(sz.value("depth").toDouble() / unit());
                if (sz.contains(QStringLiteral("counterbore_diameter")))
                    holeCounterboreDia->setValue(sz.value("counterbore_diameter").toDouble() /
                                                 unit());
                if (sz.contains(QStringLiteral("counterbore_depth")))
                    holeCounterboreDepth->setValue(sz.value("counterbore_depth").toDouble() /
                                                   unit());
                if (sz.contains(QStringLiteral("countersink_diameter")))
                    holeCountersinkDia->setValue(sz.value("countersink_diameter").toDouble() /
                                                 unit());
                if (sz.contains(QStringLiteral("countersink_angle")))
                    holeCountersinkAngle->setValue(sz.value("countersink_angle").toDouble() *
                                                   180.0 / std::numbers::pi);
                if (sz.contains(QStringLiteral("drill_angle")))
                    holeDrillAngle->setValue(sz.value("drill_angle").toDouble() * 180.0 /
                                             std::numbers::pi);
            }
            if (p.contains(QStringLiteral("termination")))
                holeTermination->setCurrentText(
                    p.value("termination").toMap().value("mode").toString());
        }

        if (kind == "Fillet") {
            matchRef(filletVariableEdge, p.value("edge").toMap());
            if (p.value("control_points").isArray()) {
                auto points = p.value("control_points").toArray();
                filletControlPoints->setRowCount(points.size());
                for (int row = 0; row < points.size(); ++row) {
                    auto point = points[row].toMap();
                    filletControlPoints->setItem(
                        row, 0,
                        new QTableWidgetItem(
                            QString::number(point.value("parameter").toDouble(), 'g', 12)));
                    filletControlPoints->setItem(
                        row, 1,
                        new QTableWidgetItem(
                            QString::number(point.value("radius").toDouble() / unit(), 'g', 12)));
                }
                filletTransition->setCurrentText(p.value("transition").toString());
            }
            if (p.contains(QStringLiteral("method")))
                filletMethod->setCurrentText(p.value("method").toString());
            if (p.contains(QStringLiteral("radius")))
                filletRadius->setValue(p.value("radius").toDouble() / unit());
            if (p.contains(QStringLiteral("propagate_tangent")))
                filletPropagateTangent->setChecked(p.value("propagate_tangent").toBool());
        }

        if (kind == "Chamfer") {
            matchRef(chamferReferenceFace, p.value("reference_face").toMap());
            chamferPropagateTangent->setChecked(p.value("propagate_tangent").toBool());
            if (p.contains(QStringLiteral("method")))
                chamferMethod->setCurrentText(p.value("method").toString());
            if (p.contains(QStringLiteral("distance")))
                chamferDistance->setValue(p.value("distance").toDouble() / unit());
            if (p.contains(QStringLiteral("distance1")))
                chamferDistance->setValue(p.value("distance1").toDouble() / unit());
            if (p.contains(QStringLiteral("distance2")))
                chamferDistance2->setValue(p.value("distance2").toDouble() / unit());
            if (p.contains(QStringLiteral("angle")))
                chamferAngle->setValue(p.value("angle").toDouble() * 180.0 / std::numbers::pi);
            if (p.contains(QStringLiteral("flip")))
                chamferFlip->setChecked(p.value("flip").toBool());
        }

        if (kind == "Rib") {
            if (p.contains(QStringLiteral("thickness")))
                ribThickness->setValue(p.value("thickness").toDouble() / unit());
            if (p.contains(QStringLiteral("side")))
                ribSide->setCurrentText(p.value("side").toString());
            if (p.contains(QStringLiteral("direction")))
                ribDirection->setCurrentText(p.value("direction").toString());
            if (p.contains(QStringLiteral("flip_material")))
                ribFlipMaterial->setChecked(p.value("flip_material").toBool());
        }

        if (kind == "LinearPattern") {
            matchRef(linearDir1Ref, p.value("direction1").toMap().value("reference").toMap());
            matchRef(linearDir2Ref, p.value("direction2").toMap().value("reference").toMap());
            if (p.contains(QStringLiteral("seed_kind")))
                patSeedKind->setCurrentText(p.value("seed_kind").toString());
            if (p.contains(QStringLiteral("direction1"))) {
                auto d1 = p.value("direction1").toMap();
                if (d1.contains(QStringLiteral("spacing")))
                    linearSpacing1->setValue(d1.value("spacing").toDouble() / unit());
                if (d1.contains(QStringLiteral("count")))
                    linearCount1->setValue(d1.value("count").toInteger());
                if (d1.contains(QStringLiteral("reverse")))
                    linearReverse1->setChecked(d1.value("reverse").toBool());
            }
            if (p.contains(QStringLiteral("direction2")) && !p.value("direction2").isNull()) {
                auto d2 = p.value("direction2").toMap();
                if (d2.contains(QStringLiteral("spacing")))
                    linearSpacing2->setValue(d2.value("spacing").toDouble() / unit());
                if (d2.contains(QStringLiteral("count")))
                    linearCount2->setValue(d2.value("count").toInteger());
                if (d2.contains(QStringLiteral("reverse")))
                    linearReverse2->setChecked(d2.value("reverse").toBool());
            }
        }

        if (kind == "CircularPattern") {
            matchRef(circAxisRef, p.value("axis").toMap());
            if (p.contains(QStringLiteral("seed_kind")))
                patSeedKind->setCurrentText(p.value("seed_kind").toString());
            if (p.contains(QStringLiteral("count")))
                circCount->setValue(p.value("count").toInteger());
            if (p.contains(QStringLiteral("equal_spacing")))
                circEqualSpacing->setChecked(p.value("equal_spacing").toBool());
            if (p.contains(QStringLiteral("angle")))
                circAngle->setValue(p.value("angle").toDouble() * 180.0 / std::numbers::pi);
            if (p.contains(QStringLiteral("reverse")))
                circReverse->setChecked(p.value("reverse").toBool());
        }

        if (kind == "Mirror") {
            matchRef(mirrorPlaneRef, p.value("plane").toMap());
            if (p.contains(QStringLiteral("seed_kind")))
                patSeedKind->setCurrentText(p.value("seed_kind").toString());
        }

        if (kind == "CosmeticThread") {
            matchRef(threadEdgeRef, p.value("reference").toMap());
            threadDepth->setValue(p.value("termination").toMap().value("depth").toDouble(.025) /
                                  unit());
            if (p.contains(QStringLiteral("designation")))
                threadDesignation->setText(p.value("designation").toString());
            if (p.contains(QStringLiteral("standard")))
                threadStandard->setCurrentText(p.value("standard").toString());
            if (p.contains(QStringLiteral("type")))
                threadType->setCurrentText(p.value("type").toString());
            if (p.contains(QStringLiteral("size")))
                threadSize->setText(p.value("size").toString());
            if (p.contains(QStringLiteral("thread_type")))
                threadInternal->setChecked(p.value("thread_type").toString() == "Internal");
            if (p.contains(QStringLiteral("major_diameter")))
                threadMajorDia->setValue(p.value("major_diameter").toDouble() / unit());
            if (p.contains(QStringLiteral("minor_diameter")))
                threadMinorDia->setValue(p.value("minor_diameter").toDouble() / unit());
            if (p.contains(QStringLiteral("pitch")))
                threadPitch->setValue(p.value("pitch").toDouble() / unit());
            if (p.contains(QStringLiteral("termination")))
                threadTermination->setCurrentText(
                    p.value("termination").toMap().value("mode").toString());
        }

        if (kind == "Shell") {
            if (p.contains(QStringLiteral("thickness")))
                shellThickness->setValue(p.value("thickness").toDouble() / unit());
            if (p.contains(QStringLiteral("inward")))
                shellInward->setChecked(p.value("inward").toBool());
            if (p.contains(QStringLiteral("overrides"))) {
                auto ovs = p.value("overrides").toArray();
                shellOverrides->setRowCount(ovs.size());
                for (int row = 0; row < ovs.size(); ++row) {
                    auto ov = ovs[row].toMap();
                    auto fRef = ov.value("face").toMap();
                    auto *fItem = new QTableWidgetItem(fRef.value("topology").toString().right(8));
                    fItem->setData(Qt::UserRole, QVariant::fromValue(fRef));
                    shellOverrides->setItem(row, 0, fItem);
                    auto *tItem = new QTableWidgetItem(
                        QString::number(ov.value("thickness").toDouble() / unit(), 'g', 6));
                    shellOverrides->setItem(row, 1, tItem);
                }
            }
        }

        if (kind == "Sweep") {
            if (p.contains(QStringLiteral("profile"))) {
                auto prof = p.value("profile").toMap();
                auto pType = prof.value("type").toString();
                sweepProfileType->setCurrentText(pType);
                if (pType == "Circular") {
                    sweepDiameter->setValue(prof.value("diameter").toDouble() / unit());
                } else {
                    auto sId = prof.value("sketch").toString();
                    int idx = profile->findData(sId);
                    if (idx >= 0)
                        profile->setCurrentIndex(idx);
                }
            }
            if (p.contains(QStringLiteral("path"))) {
                auto pathMap = p.value("path").toMap();
                matchRef(sweepPath, pathMap.value("reference").toMap());
                sweepPathReverse->setChecked(pathMap.value("reverse").toBool());
            }
            if (p.contains(QStringLiteral("corner_transition")))
                sweepCornerTransition->setCurrentText(p.value("corner_transition").toString());
            if (p.contains(QStringLiteral("start_tangency")))
                sweepStartTangency->setCurrentText(p.value("start_tangency").toString());
            if (p.contains(QStringLiteral("end_tangency")))
                sweepEndTangency->setCurrentText(p.value("end_tangency").toString());
            if (p.contains(QStringLiteral("preview_sections")))
                sweepPreviewSections->setValue(p.value("preview_sections").toInteger());
        }

        if (kind == "Loft") {
            if (p.contains(QStringLiteral("output")))
                loftOutput->setCurrentText(p.value("output").toString());
            if (p.contains(QStringLiteral("sections"))) {
                loftSections->clear();
                for (auto secVal : p.value("sections").toArray()) {
                    auto sec = secVal.toMap();
                    auto sId = sec.value("sketch").toString();
                    QString sName = sId;
                    for (int i = 0; i < profile->count(); ++i) {
                        if (profile->itemData(i).toString() == sId) {
                            sName = profile->itemText(i);
                            break;
                        }
                    }
                    auto *item = new QListWidgetItem(sName, loftSections);
                    item->setData(Qt::UserRole, sId);
                    item->setCheckState(Qt::Checked);
                }
            }
            if (p.contains(QStringLiteral("centerline")) && !p.value("centerline").isNull()) {
                auto clMap = p.value("centerline").toMap();
                matchRef(loftCenterline, clMap.value("reference").toMap());
                loftCenterlineReverse->setChecked(clMap.value("reverse").toBool());
            }
            if (p.contains(QStringLiteral("connectors"))) {
                auto conns = p.value("connectors").toArray();
                loftConnectors->setRowCount(conns.size());
                for (int row = 0; row < conns.size(); ++row) {
                    auto conn = conns[row].toMap();
                    loftConnectors->setItem(row, 0,
                                            new QTableWidgetItem(conn.value("id").toString()));
                    QStringList pts;
                    for (auto ptVal : conn.value("points").toArray())
                        pts.append(QString::number(ptVal.toDouble(), 'g', 6));
                    loftConnectors->setItem(row, 1, new QTableWidgetItem(pts.join(", ")));
                }
            }
            if (p.contains(QStringLiteral("start_condition")))
                loftStartCondition->setCurrentText(p.value("start_condition").toString());
            if (p.contains(QStringLiteral("end_condition")))
                loftEndCondition->setCurrentText(p.value("end_condition").toString());
        }
    } else if (!isModeling && !editId.isEmpty()) {
        auto p = draft.parameters;
        auto pick = [&](QComboBox *w, QString key) {
            int i = w->findData(p.value(key).toString());
            if (i >= 0)
                w->setCurrentIndex(i);
        };
        pick(profile, "sketch");
        pick(target, "target");
        pick(source, "source");
        operation->setCurrentText(p.value("operation").toString());
        extent->setCurrentText(
            p.value(QString(kind == "Revolve" ? "direction" : "extent")).toString());
        distribution->setCurrentText(p.value("distribution").toString());
        for (auto pair : {std::pair{depth, QString("depth")},
                          std::pair{size, kind == "Hole" ? QString("diameter") : QString("size")},
                          std::pair{spacing, QString("spacing")}})
            if (p.contains(pair.second))
                pair.first->setValue(p.value(pair.second).toDouble() / unit());
        if (p.contains(QStringLiteral("angle")))
            angle->setValue(p.value("angle").toDouble() * 180 / std::numbers::pi);
        if (p.contains(QStringLiteral("count")))
            count->setValue(int(p.value("count").toInteger()));
        if (p.contains(QStringLiteral("through")))
            through->setChecked(p.value("through").toBool());
        auto o = p.value(QString(kind == "Revolve" || kind == "CircularPattern" ? "axis_origin"
                                                                                : "origin"))
                     .toArray();
        auto a = p.value(QString(kind == "Mirror"                                 ? "normal"
                                 : kind == "Revolve" || kind == "CircularPattern" ? "axis_direction"
                                                                                  : "direction"))
                     .toArray();
        for (int i = 0; i < 3; ++i) {
            if (o.size() == 3)
                origin[i]->setValue(o[i].toDouble() / unit());
            if (a.size() == 3)
                axis[i]->setValue(a[i].toDouble());
        }
    }

    if (isModeling && kind == "Hole")
        installHolePoints();
    editing = true;
    evaluation->hide();
    inputs->setEnabled(true);
    previewButton->setEnabled(true);
    cancelButton->setEnabled(true);
    validPreview = false;
    installing = false;
    title->setText((editId.isEmpty() ? "Create " : "Edit ") + kind);
    inputs->show();
    accept->show();
    previewButton->show();
    cancelButton->show();
    fieldsForKind();
    if (repairButton) {
        repairButton->setVisible(editing && (document.contains(QStringLiteral("model_supports")) ||
                                             document.contains(QStringLiteral("modeling"))));
        repairButton->setEnabled(editing && !client->outstanding() && !commitPending);
    }
    invalidate();
    updateManipulator();
    if (isModeling) {
        if (kind == "Draft")
            repairPath->setCurrentText("faces/0");
        else if (kind == "Hole")
            repairPath->setCurrentText("placement/face");
        else if (kind == "Fillet" || kind == "Chamfer")
            repairPath->setCurrentText("edges/0");
        else if (kind == "Mirror")
            repairPath->setCurrentText("plane");
        else if (kind == "CosmeticThread")
            repairPath->setCurrentText("reference");
        else
            repairPath->setCurrentText("direction1/reference");
        QCborArray prefix;
        for (auto value : document.value("modeling").toMap().value("intents").toArray()) {
            if (value.toMap().value("id") == draft.id)
                break;
            prefix.append(value);
        }
        client->projectCommand(
            "ModelingInputs",
            fields({{"model", model::editableSupports(document).map()}, {"intents", prefix}}));
    }
    diagnostic->setText(kind == "Fillet" || kind == "Chamfer"
                            ? "Select an edge in the current 3D result, then Preview."
                            : "Lengths use " + document.value("units").toString() +
                                  "; angles use degrees. Select inputs, then Preview.");
    emit editingChanged();
}

QCborArray FeaturePanel::definitions() const {
    auto f = draft;
    f.name = name->text();
    auto kind = f.kind;
    double u = unit();
    QCborMap p;
    auto set = [&](QString k, QCborValue v) { p.insert(k, v); };
    if (kind == "Extrude" || kind == "Revolve") {
        set("sketch", profile->currentData().toString());
        set("operation", operation->currentText());
        set("target",
            operation->currentText() == "NewBody" ? QString() : target->currentData().toString());
        if (kind == "Extrude") {
            set("depth", depth->value() * u);
            set("extent", extent->currentText());
        }
    } else {
        set("target",
            (kind == "Mirror" || kind.endsWith("Pattern")) && operation->currentText() == "NewBody"
                ? QString()
                : target->currentData().toString());
        if (kind != "Hole")
            set("source", source->currentData().toString());
    }
    if (kind == "Fillet" || kind == "Chamfer") {
        auto selected = client->state().selection.entity;
        auto edge = selected.mid(selected.indexOf('/') + 1).section("/segment/", 0, 0);
        auto old = draft.parameters.value("edges").toArray();
        set("edges", selected.isEmpty() ? old : QCborArray{edge});
        set("size", size->value() * u);
    }
    QCborArray o, a;
    for (int i = 0; i < 3; ++i) {
        o.append(origin[i]->value() * u);
        a.append(axis[i]->value());
    }
    if (kind == "Revolve") {
        set("angle", angle->value() * std::numbers::pi / 180);
        set("direction", extent->currentText());
        set("axis_origin", o);
        set("axis_direction", a);
    }
    if (kind == "Hole") {
        set("origin", o);
        set("direction", a);
        set("diameter", size->value() * u);
        set("depth", depth->value() * u);
        set("through", through->isChecked());
    }
    if (kind == "LinearPattern" || kind == "CircularPattern" || kind == "Mirror") {
        set("operation", operation->currentText());
        if (kind == "Mirror") {
            set("origin", o);
            set("normal", a);
        } else {
            set("count", count->value());
            if (kind == "LinearPattern") {
                set("direction", a);
                set("spacing", spacing->value() * u);
            } else {
                set("axis_origin", o);
                set("axis_direction", a);
                set("angle", angle->value() * std::numbers::pi / 180);
                set("distribution", distribution->currentText());
            }
        }
    }
    f.parameters = p;
    features::Feature::parse(f.map());
    auto all = document.value("features").toArray();
    bool found = false;
    for (qsizetype i = 0; i < all.size(); ++i)
        if (all[i].toMap().value("id").toString() == f.id) {
            all[i] = f.map();
            found = true;
        }
    if (!found)
        all.append(f.map());
    return all;
}

void FeaturePanel::installHolePoints() {
    QSignalBlocker block(holePoints);
    holePoints->clear();
    auto selected = modelingDraft.value("placement").toMap();
    bool original = selected.value("sketch").toString() == profile->currentData().toString();
    for (auto value : document.value("sketches").toArray()) {
        auto sketch = value.toMap();
        if (sketch.value("id").toString() != profile->currentData().toString())
            continue;
        for (auto entry : sketch.value("entities").toArray()) {
            auto entity = entry.toMap();
            if (entity.value("kind") != "Point")
                continue;
            auto *item =
                new QListWidgetItem("Point " + entity.value("id").toString().right(8), holePoints);
            item->setData(Qt::UserRole, entity.value("id").toString());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(
                !original || selected.value("points").toArray().contains(entity.value("id"))
                    ? Qt::Checked
                    : Qt::Unchecked);
        }
    }
    if (original)
        for (auto point : selected.value("points").toArray()) {
            bool found = false;
            for (int i = 0; i < holePoints->count(); ++i)
                found =
                    found || holePoints->item(i)->data(Qt::UserRole).toString() == point.toString();
            if (!found) {
                auto *item = new QListWidgetItem("Missing point " + point.toString().right(8) +
                                                     " (choose a replacement)",
                                                 holePoints);
                item->setData(Qt::UserRole, point.toString());
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
            }
        }
}
void FeaturePanel::installFilletRadii() {
    QMap<QString, QString> values;
    for (auto value : modelingDraft.value("radii").toArray()) {
        auto radius = value.toMap();
        auto key = QString::fromLatin1(QCborValue(radius.value("edge")).toCbor().toHex());
        values[key] = QString::number(radius.value("radius").toDouble() / unit(), 'g', 12);
    }
    for (int row = 0; row < filletRadii->rowCount(); ++row)
        values[filletRadii->item(row, 0)->data(Qt::UserRole).toString()] =
            filletRadii->item(row, 1)->text();
    QSignalBlocker block(filletRadii);
    filletRadii->setRowCount(0);
    for (int i = 0; i < filletEdges->count(); ++i) {
        auto *edge = filletEdges->item(i);
        if (edge->checkState() != Qt::Checked)
            continue;
        auto ref = edge->data(Qt::UserRole).value<QCborMap>();
        auto key = QString::fromLatin1(QCborValue(ref).toCbor().toHex());
        int row = filletRadii->rowCount();
        filletRadii->insertRow(row);
        auto *label = new QTableWidgetItem(edge->text());
        label->setFlags(label->flags() & ~Qt::ItemIsEditable);
        label->setData(Qt::UserRole, key);
        label->setData(Qt::UserRole + 1, QVariant::fromValue(ref));
        filletRadii->setItem(row, 0, label);
        filletRadii->setItem(row, 1,
                             new QTableWidgetItem(values.value(
                                 key, QString::number(filletRadius->value(), 'g', 12))));
    }
}
void FeaturePanel::installPatternSeeds() {
    auto kind = patSeedKind->currentText();
    auto previous =
        patSeed->property("seedKind").toString() == kind ? patSeed->currentData() : QVariant{};
    if (kind != "Face" && !previous.isValid() && modelingDraft.value("seed_kind") == kind) {
        auto seed = modelingDraft.value("seed");
        previous = seed.isString() ? QVariant(seed.toString()) : QVariant::fromValue(seed.toMap());
    }
    QSignalBlocker block(patSeed);
    patSeed->clear();
    if (kind == "Feature") {
        for (auto value : document.value("modeling").toMap().value("intents").toArray()) {
            auto intent = value.toMap();
            if (intent.value("id").toString() == draft.id)
                break;
            auto type = intent.value("kind").toString();
            if (type == "Fillet" || type == "Chamfer" || type == "Draft" ||
                type == "CosmeticThread")
                continue;
            patSeed->addItem(type + " " + intent.value("id").toString().right(8),
                             intent.value("id").toString());
        }
    } else if (kind == "Body") {
        for (auto value : availableModelingInputs.value("bodies").toArray()) {
            auto body = value.toMap();
            patSeed->addItem("Body " + body.value("body").toString().right(8),
                             QVariant::fromValue(body));
        }
    }
    for (int i = 0; i < patSeed->count(); ++i)
        if (patSeed->itemData(i) == previous) {
            patSeed->setCurrentIndex(i);
            previous = {};
            break;
        }
    if (previous.isValid()) {
        patSeed->addItem("Original seed (requires validation)", previous);
        patSeed->setCurrentIndex(patSeed->count() - 1);
    }
    patSeed->setProperty("seedKind", kind);
}

void FeaturePanel::installModelingInputs(QCborMap input) {
    if (!editing || !isModeling)
        return;
    installing = true;
    availableModelingInputs = input;
    auto priorTarget = target->currentData();
    target->clear();
    target->addItem("None", QString());
    for (auto value : input.value("bodies").toArray()) {
        auto body = value.toMap();
        target->addItem("Body " + body.value("body").toString().right(8),
                        QVariant::fromValue(body));
    }
    auto wanted = priorTarget.canConvert<QCborMap>() ? priorTarget.value<QCborMap>()
                                                     : modelingDraft.value("target").toMap();
    int selected = -1;
    for (int i = 1; i < target->count(); ++i) {
        auto body = target->itemData(i).value<QCborMap>();
        if (body == wanted || body.value("body").toString() == priorTarget.toString())
            selected = i;
    }
    target->setCurrentIndex(selected >= 0 ? selected : target->count() > 1 ? 1 : 0);
    if (!wanted.empty() && selected < 0 && !modelingDraft.empty()) {
        target->addItem("Original target (requires validation)", QVariant::fromValue(wanted));
        target->setCurrentIndex(target->count() - 1);
    }
    for (auto *box : {startReference, dir1Reference, dir2Reference, revolveAxis, draftNeutral,
                      draftPull, draftFaces}) {
        auto previous = box->currentData().value<QCborMap>();
        for (auto value : input.value("references").toArray()) {
            auto ref = value.toMap();
            auto role = ref.value("role").toString();
            bool plane = box == startReference || box == dir1Reference || box == dir2Reference ||
                         box == draftNeutral;
            bool point = box == startReference || box == dir1Reference || box == dir2Reference;
            bool axis = box == revolveAxis || box == draftPull;
            if ((role == "Face" && (plane || box == draftFaces)) || (role == "Vertex" && point) ||
                (role == "Edge" && axis))
                box->addItem(role + " " + ref.value("topology").toString().right(8),
                             QVariant::fromValue(ref));
        }
        if (box == dir1Reference || box == dir2Reference)
            for (auto value : input.value("bodies").toArray()) {
                auto ref = value.toMap().value("source").toMap();
                box->addItem("Body " + value.toMap().value("body").toString().right(8),
                             QVariant::fromValue(ref));
                box->setItemData(box->count() - 1, value.toMap().value("body").toString(),
                                 Qt::UserRole + 1);
            }
        if (!previous.empty()) {
            QString wantedBody;
            if (previous.value("role") == "Object") {
                auto direction =
                    modelingDraft.value(box == dir1Reference ? "direction1" : "direction2").toMap();
                wantedBody = direction.value("body").toString();
            }
            for (int i = 0; i < box->count(); ++i)
                if (box->itemData(i).value<QCborMap>() == previous &&
                    (wantedBody.isEmpty() ||
                     box->itemData(i, Qt::UserRole + 1).toString() == wantedBody)) {
                    box->setCurrentIndex(i);
                    break;
                }
        }
    }

    for (auto *box : {holePlacementFace, filletVariableEdge, chamferReferenceFace, holeEndReference,
                      threadEdgeRef, mirrorPlaneRef, circAxisRef, linearDir1Ref, linearDir2Ref}) {
        if (!box)
            continue;
        auto previous = box->currentData().value<QCborMap>();
        box->clear();
        if (box == chamferReferenceFace)
            box->addItem("Automatic adjacent face", QVariant::fromValue(QCborMap{}));
        for (auto value : input.value("references").toArray()) {
            auto ref = value.toMap();
            auto role = ref.value("role").toString();
            bool match = false;
            if (box == holePlacementFace && role == "Face")
                match = true;
            else if ((box == filletVariableEdge && role == "Edge") ||
                     (box == threadEdgeRef && (role == "Edge" || role == "Face")))
                match = true;
            else if ((box == mirrorPlaneRef || box == chamferReferenceFace) && role == "Face")
                match = true;
            else if (box == holeEndReference && (role == "Face" || role == "Vertex"))
                match = true;
            else if (box == circAxisRef && (role == "Edge" || role == "Axis"))
                match = true;
            else if ((box == linearDir1Ref || box == linearDir2Ref) &&
                     (role == "Edge" || role == "Axis" || role == "Curve"))
                match = true;

            if (match)
                box->addItem(role + " " + ref.value("topology").toString().right(8),
                             QVariant::fromValue(ref));
        }

        if (box == mirrorPlaneRef || box == circAxisRef || box == linearDir1Ref ||
            box == linearDir2Ref || box == holeEndReference) {
            auto *supports = box == circAxisRef || box == linearDir1Ref || box == linearDir2Ref
                                 ? revolveAxis
                                 : draftNeutral;
            for (int i = 0; i < supports->count(); ++i) {
                auto ref = supports->itemData(i).value<QCborMap>();
                if (ref.value("role") == "Axis" || ref.value("role") == "Plane")
                    box->addItem(supports->itemText(i), QVariant::fromValue(ref));
            }
            for (auto s : document.value("sketches").toArray()) {
                auto sm = s.toMap();
                auto sName = sm.value("name").toString();
                auto sId = sm.value("id").toString();
                if (box == mirrorPlaneRef || box == holeEndReference) {
                    auto planeRef = makeRef(sId, "Plane");
                    box->addItem(sName + " / Plane", QVariant::fromValue(planeRef.map()));
                }
                for (auto entVal : sm.value("entities").toArray()) {
                    auto ent = entVal.toMap();
                    if (box == holeEndReference && ent.value("kind") == "Point")
                        box->addItem(sName + " / Point " + ent.value("id").toString().right(6),
                                     QVariant::fromValue(
                                         makeRef(sId, "Point", ent.value("id").toString()).map()));
                    if (ent.value("kind") == "Line") {
                        auto lineRef = makeRef(sId, "Curve", ent.value("id").toString());
                        if (box == circAxisRef || box == linearDir1Ref || box == linearDir2Ref)
                            box->addItem(sName + " / Line " + ent.value("id").toString().right(6),
                                         QVariant::fromValue(lineRef.map()));
                    }
                }
            }
        }

        if (!previous.empty()) {
            int selected = -1;
            for (int i = 0; i < box->count(); ++i)
                if (box->itemData(i).value<QCborMap>() == previous)
                    selected = i;
            if (selected < 0) {
                box->addItem("Original reference (requires validation)",
                             QVariant::fromValue(previous));
                selected = box->count() - 1;
            }
            box->setCurrentIndex(selected);
        } else if (!client->state().selection.entity.isEmpty()) {
            auto sel = client->state().selection.entity;
            for (int i = 0; i < box->count(); ++i) {
                auto ref = box->itemData(i).value<QCborMap>();
                if (ref.value("topology").toString() == sel ||
                    sel.contains(ref.value("topology").toString())) {
                    box->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    for (auto *list : {filletEdges, chamferEdges, patFaces}) {
        QSignalBlocker block(list);
        QCborArray selected;
        bool populated = list->count() > 0;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked)
                selected.append(list->item(i)->data(Qt::UserRole).value<QCborMap>());
        if (!populated) {
            selected = modelingDraft.value("edges").toArray();
            if (list == patFaces)
                selected = modelingDraft.value("seed_kind") == "Face"
                               ? modelingDraft.value("seed").toArray()
                               : QCborArray{};
            if (list == filletEdges && modelingDraft.value("method") == "MultipleRadius")
                for (auto radius : modelingDraft.value("radii").toArray())
                    selected.append(radius.toMap().value("edge"));
        }
        list->clear();
        auto add = [&](QCborMap ref) {
            auto *item = new QListWidgetItem(ref.value("role").toString() + " " +
                                                 ref.value("topology").toString().right(8),
                                             list);
            item->setData(Qt::UserRole, QVariant::fromValue(ref));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            bool checked = selected.contains(ref);
            if (!populated && modelingDraft.empty())
                checked =
                    client->state().selection.entity.contains(ref.value("topology").toString());
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        };
        for (auto ref : selected)
            add(ref.toMap());
        for (auto value : input.value("references").toArray()) {
            auto ref = value.toMap();
            if (ref.value("role") == (list == patFaces ? "Face" : "Edge") &&
                !selected.contains(ref))
                add(ref);
        }
    }
    {
        QSignalBlocker block(shellRemoveFaces);
        QCborArray selected;
        bool populated = shellRemoveFaces->count() > 0;
        for (int i = 0; i < shellRemoveFaces->count(); ++i)
            if (shellRemoveFaces->item(i)->checkState() == Qt::Checked)
                selected.append(shellRemoveFaces->item(i)->data(Qt::UserRole).value<QCborMap>());
        if (!populated && !modelingDraft.empty()) {
            selected = modelingDraft.value("remove_faces").toArray();
        }
        shellRemoveFaces->clear();
        auto add = [&](QCborMap ref) {
            auto *item = new QListWidgetItem("Face " + ref.value("topology").toString().right(8),
                                             shellRemoveFaces);
            item->setData(Qt::UserRole, QVariant::fromValue(ref));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            bool checked = selected.contains(ref);
            if (!populated && modelingDraft.empty())
                checked =
                    client->state().selection.entity.contains(ref.value("topology").toString());
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        };
        for (auto ref : selected)
            add(ref.toMap());
        for (auto value : input.value("references").toArray()) {
            auto ref = value.toMap();
            if (ref.value("role") == "Face" && !selected.contains(ref))
                add(ref);
        }
    }
    for (auto *list : {sweepGuides, loftGuides}) {
        QSignalBlocker block(list);
        QCborArray selected;
        bool populated = list->count() > 0;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked)
                selected.append(list->item(i)->data(Qt::UserRole).value<QCborMap>());
        if (!populated && !modelingDraft.empty()) {
            selected = modelingDraft.value("guides").toArray();
        }
        list->clear();
        auto add = [&](QCborMap ref) {
            auto *item = new QListWidgetItem(ref.value("role").toString() + " " +
                                                 ref.value("topology").toString().right(8),
                                             list);
            item->setData(Qt::UserRole, QVariant::fromValue(ref));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected.contains(ref) ? Qt::Checked : Qt::Unchecked);
        };
        for (auto ref : selected)
            add(ref.toMap());
        for (auto value : input.value("references").toArray()) {
            auto ref = value.toMap();
            if ((ref.value("role") == "Edge" || ref.value("role") == "Curve") &&
                !selected.contains(ref))
                add(ref);
        }
    }
    installFilletRadii();
    installPatternSeeds();
    inputSketches = input.value("sketches").toArray();
    inputGeometry = input.value("geometry").toArray();
    installing = false;
    fieldsForKind();
    invalidate();
    if (previewAfterInputs) {
        previewAfterInputs = false;
        preview();
    }
}

void FeaturePanel::chooseRegions() {
    if (!editing || !isModeling)
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Select exact contour boundaries");
    dialog.setObjectName("featureContourDialog");
    auto *layout = new QVBoxLayout(&dialog);
    auto *help =
        new QLabel("Add regions, their outer boundary first, then holes. Add curves in traversal "
                   "order. Exact Preview validates closure, intersections and containment.");
    help->setWordWrap(true);
    layout->addWidget(help);
    auto *tree = new QTreeWidget;
    tree->setObjectName("featureContourTree");
    tree->setHeaderLabels({"Region / boundary / curve", "From", "To", "Reverse"});
    layout->addWidget(tree);
    auto addUse = [&](QTreeWidgetItem *boundary, QCborMap use) {
        auto *row =
            new QTreeWidgetItem(boundary, {use.value("entity").toString(),
                                           QString::number(use.value("from").toDouble(), 'g', 16),
                                           QString::number(use.value("to").toDouble(), 'g', 16),
                                           use.value("reverse").toBool() ? "yes" : "no"});
        row->setData(0, Qt::UserRole, QVariant::fromValue(use));
    };
    for (auto region : currentRegions) {
        auto *r = new QTreeWidgetItem(QStringList{"Region"});
        tree->addTopLevelItem(r);
        for (auto boundary : region.toArray()) {
            auto *b =
                new QTreeWidgetItem(QStringList{r->childCount() == 0 ? "Outer boundary" : "Hole"});
            r->addChild(b);
            for (auto use : boundary.toArray())
                addUse(b, use.toMap());
        }
    }
    auto *curves = new QComboBox;
    curves->setObjectName("featureContourCurve");
    for (auto value : document.value("sketches").toArray()) {
        auto sketch = value.toMap();
        if (sketch.value("id").toString() != profile->currentData().toString())
            continue;
        for (auto entity : sketch.value("entities").toArray()) {
            auto e = entity.toMap();
            if (e.value("kind") != "Point" && !e.value("construction").toBool())
                curves->addItem(e.value("kind").toString() + " " +
                                    e.value("id").toString().right(8),
                                e.value("id").toString());
        }
    }
    layout->addWidget(curves);
    auto *from = new QDoubleSpinBox, *to = new QDoubleSpinBox;
    for (auto *spin : {from, to}) {
        spin->setDecimals(8);
        spin->setRange(0, 1);
        layout->addWidget(spin);
    }
    from->setPrefix("From ");
    to->setPrefix("To ");
    to->setValue(1);
    auto *reverse = new QCheckBox("Reverse traversal");
    layout->addWidget(reverse);
    auto addButton = [&](QString label, QString name, auto callback) {
        auto *button = new QPushButton(label);
        button->setObjectName(name);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, &dialog, callback);
    };
    addButton("Add region", "featureContourAddRegion", [&] {
        auto *r = new QTreeWidgetItem(QStringList{"Region"});
        tree->addTopLevelItem(r);
        auto *b = new QTreeWidgetItem(r, {"Outer boundary"});
        tree->setCurrentItem(b);
        tree->expandAll();
    });
    addButton("Add hole", "featureContourAddHole", [&] {
        auto *item = tree->currentItem();
        if (!item)
            return;
        while (item->parent())
            item = item->parent();
        auto *b = new QTreeWidgetItem(item, {"Hole"});
        tree->setCurrentItem(b);
        tree->expandAll();
    });
    addButton("Add selected curve", "featureContourAddCurve", [&] {
        auto *b = tree->currentItem();
        if (!b || curves->currentData().toString().isEmpty() || from->value() >= to->value())
            return;
        if (b->parent() && b->parent()->parent())
            b = b->parent();
        if (!b->parent()) {
            if (!b->childCount())
                return;
            b = b->child(0);
        }
        addUse(b, fields({{"entity", curves->currentData().toString()},
                          {"from", from->value()},
                          {"to", to->value()},
                          {"reverse", reverse->isChecked()}}));
        tree->expandAll();
    });
    addButton("Remove selected item", "featureContourRemove", [&] { delete tree->currentItem(); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    tree->expandAll();
    dialog.resize(620, 620);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QCborArray selected;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QCborArray region;
        auto *r = tree->topLevelItem(i);
        for (int j = 0; j < r->childCount(); ++j) {
            QCborArray boundary;
            auto *b = r->child(j);
            for (int k = 0; k < b->childCount(); ++k)
                boundary.append(b->child(k)->data(0, Qt::UserRole).value<QCborMap>());
            region.append(boundary);
        }
        selected.append(region);
    }
    setSelectedRegions(selected);
}

QCborMap FeaturePanel::definitionModeling() const {
    double u = unit();
    auto kind = draft.kind;
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("id"), draft.id);
    m.insert(QStringLiteral("kind"), kind);

    auto makeTarget = [&]() -> QCborMap {
        auto targetData = target->currentData();
        QCborMap targetMap;
        if (targetData.canConvert<QCborMap>()) {
            targetMap = targetData.value<QCborMap>();
        } else if (!targetData.toString().isEmpty()) {
            targetMap.insert(QStringLiteral("source"),
                             makeRef(targetData.toString(), "Object").map());
            targetMap.insert(QStringLiteral("body"), targetData.toString());
        } else if (!modelingDraft.isEmpty() && !modelingDraft.value("target").isNull()) {
            targetMap = modelingDraft.value("target").toMap();
        } else {
            for (auto idVal : result.value("bodies").toArray()) {
                auto bid = idVal.toString();
                targetMap.insert(QStringLiteral("source"), makeRef(bid, "Object").map());
                targetMap.insert(QStringLiteral("body"), bid);
                break;
            }
        }
        return targetMap;
    };

    if (kind == "Draft") {
        m.insert(QStringLiteral("target"), makeTarget());

        // neutral
        QCborMap neutralMap;
        if (draftNeutral->currentData().canConvert<QCborMap>())
            neutralMap = draftNeutral->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("neutral")))
            neutralMap = modelingDraft.value("neutral").toMap();
        m.insert(QStringLiteral("neutral"), neutralMap);

        // pull
        QCborMap pullMap;
        if (draftPull->currentData().canConvert<QCborMap>())
            pullMap = draftPull->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("pull")))
            pullMap = modelingDraft.value("pull").toMap();
        m.insert(QStringLiteral("pull"), pullMap);

        // faces
        QCborArray facesArray;
        if (draftFaces->currentData().canConvert<QCborArray>())
            facesArray = draftFaces->currentData().value<QCborArray>();
        else if (draftFaces->currentData().canConvert<QCborMap>()) {
            auto currentFace = draftFaces->currentData().value<QCborMap>();
            if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("faces"))) {
                auto existingFaces = modelingDraft.value("faces").toArray();
                if (!existingFaces.isEmpty() && existingFaces[0].toMap() == currentFace)
                    facesArray = existingFaces;
                else
                    facesArray = QCborArray{currentFace};
            } else {
                facesArray = QCborArray{currentFace};
            }
        } else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("faces")))
            facesArray = modelingDraft.value("faces").toArray();
        m.insert(QStringLiteral("faces"), facesArray);

        m.insert(QStringLiteral("angle"), draftStandaloneAngle->value() * std::numbers::pi / 180.0);
        m.insert(QStringLiteral("outward"), draftStandaloneOutward->isChecked());
        return m;
    }

    if (kind == "Hole") {
        m.insert(QStringLiteral("operation"), QStringLiteral("Cut"));
        m.insert(QStringLiteral("target"), makeTarget());
        m.insert(QStringLiteral("family"), holeFamily->currentText());

        QCborMap placement;
        auto profSketchId = profile->currentData().toString();
        placement.insert(QStringLiteral("sketch"), profSketchId);
        QCborArray pts;
        for (int i = 0; i < holePoints->count(); ++i)
            if (holePoints->item(i)->checkState() == Qt::Checked)
                pts.append(holePoints->item(i)->data(Qt::UserRole).toString());
        QCborArray ordered;
        auto originalPlacement = modelingDraft.value("placement").toMap();
        if (originalPlacement.value("sketch").toString() == profSketchId)
            for (auto point : originalPlacement.value("points").toArray())
                if (pts.contains(point))
                    ordered.append(point);
        for (auto point : pts)
            if (!ordered.contains(point))
                ordered.append(point);
        placement.insert(QStringLiteral("points"), ordered);
        QCborMap faceRef;
        if (holePlacementFace->currentData().canConvert<QCborMap>())
            faceRef = holePlacementFace->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() &&
                 modelingDraft.value("placement").toMap().contains(QStringLiteral("face")))
            faceRef = modelingDraft.value("placement").toMap().value("face").toMap();
        placement.insert(QStringLiteral("face"), faceRef);
        m.insert(QStringLiteral("placement"), placement);

        QCborMap sizing;
        sizing.insert(QStringLiteral("mode"), QStringLiteral("Custom"));
        double dia = holeDiameter->value() * u;
        double dep = holeDepth->value() * u;
        sizing.insert(QStringLiteral("diameter"), dia);
        sizing.insert(QStringLiteral("depth"), dep);
        auto fam = holeFamily->currentText();
        if (fam == "Counterbore" || fam == "LegacyCounterboreDrill") {
            double cbDia = holeCounterboreDia->value() * u;
            double cbDep = holeCounterboreDepth->value() * u;
            sizing.insert(QStringLiteral("counterbore_diameter"), cbDia);
            sizing.insert(QStringLiteral("counterbore_depth"), cbDep);
        }
        if (fam == "Countersink" || fam == "LegacyCountersinkDrill") {
            double csDia = holeCountersinkDia->value() * u;
            sizing.insert(QStringLiteral("countersink_diameter"), csDia);
            sizing.insert(QStringLiteral("countersink_angle"),
                          holeCountersinkAngle->value() * std::numbers::pi / 180.0);
        }
        if (fam == "Drilled" || fam == "LegacySimpleDrilled" || fam == "LegacyCounterboreDrill" ||
            fam == "LegacyCountersinkDrill") {
            sizing.insert(QStringLiteral("drill_angle"),
                          holeDrillAngle->value() * std::numbers::pi / 180.0);
        }
        if (fam == "LegacyTapered" || fam == "PipeTap")
            sizing.insert(QStringLiteral("taper_angle"),
                          holeTaperAngle->value() * std::numbers::pi / 180);
        if (fam == "Tapped" || fam == "PipeTap") {
            sizing.insert(QStringLiteral("tap_drill_diameter"), holeTapDiameter->value() * u);
            bool cosmetic = modelingDraft.value("sizing")
                                .toMap()
                                .value("thread")
                                .toMap()
                                .value("cosmetic")
                                .toBool(true);
            sizing.insert(QStringLiteral("thread"),
                          fields({{"cosmetic", cosmetic},
                                  {"designation", holeThreadDesignation->text()},
                                  {"pitch", holeThreadPitch->value() * u},
                                  {"depth", holeThreadDepth->value() * u}}));
        }
        if (!holeCustomSizing->isChecked() &&
            modelingDraft.value("sizing").toMap().value("mode") == "Catalog")
            sizing = modelingDraft.value("sizing").toMap();
        m.insert(QStringLiteral("sizing"), sizing);

        QCborMap term;
        term.insert(QStringLiteral("mode"), holeTermination->currentText());
        term.insert(QStringLiteral("depth_reference"), holeDepthReference->currentText());
        auto mode = holeTermination->currentText();
        if (mode == "UpToSurface" || mode == "UpToVertex" || mode == "OffsetFromSurface")
            term.insert(QStringLiteral("reference"),
                        holeEndReference->currentData().value<QCborMap>());
        if (mode == "OffsetFromSurface") {
            term.insert(QStringLiteral("offset"), surfaceOffset->value() * u);
            term.insert(QStringLiteral("offset_reverse"), offsetReverse->isChecked());
        }
        m.insert(QStringLiteral("termination"), term);
        return m;
    }

    auto checkedEdges = [](QListWidget *list) {
        QCborArray refs;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked)
                refs.append(list->item(i)->data(Qt::UserRole).value<QCborMap>());
        return refs;
    };
    auto number = [](QTableWidget *table, int row, int column) {
        auto *item = table->item(row, column);
        bool ok = false;
        double value = item ? item->text().toDouble(&ok) : 0;
        domain::check(ok && std::isfinite(value), "InvalidInput",
                      "Enter finite numeric radius table values");
        return value;
    };
    if (kind == "Fillet") {
        m.insert(QStringLiteral("target"), makeTarget());
        auto method = filletMethod->currentText();
        m.insert(QStringLiteral("method"), method);
        m.insert(QStringLiteral("propagate_tangent"), filletPropagateTangent->isChecked());
        if (method == "Constant") {
            m.insert(QStringLiteral("radius"), filletRadius->value() * u);
            m.insert(QStringLiteral("edges"), checkedEdges(filletEdges));
        } else if (method == "MultipleRadius") {
            QCborArray radii;
            for (int row = 0; row < filletRadii->rowCount(); ++row)
                radii.append(fields(
                    {{"edge", filletRadii->item(row, 0)->data(Qt::UserRole + 1).value<QCborMap>()},
                     {"radius", number(filletRadii, row, 1) * u}}));
            m.insert(QStringLiteral("radii"), radii);
        } else if (method == "Variable") {
            m.insert(QStringLiteral("edge"), filletVariableEdge->currentData().value<QCborMap>());
            QCborArray points;
            for (int row = 0; row < filletControlPoints->rowCount(); ++row)
                points.append(fields({{"parameter", number(filletControlPoints, row, 0)},
                                      {"radius", number(filletControlPoints, row, 1) * u}}));
            m.insert(QStringLiteral("control_points"), points);
            m.insert(QStringLiteral("transition"), filletTransition->currentText());
        } else {
            domain::check(false, "UnsupportedOperation", "Face-set fillets remain blocked (R1)");
        }
        return m;
    }

    if (kind == "Chamfer") {
        m.insert(QStringLiteral("target"), makeTarget());
        auto method = chamferMethod->currentText();
        m.insert(QStringLiteral("method"), method);
        m.insert(QStringLiteral("propagate_tangent"), true);
        m.insert(QStringLiteral("edges"), checkedEdges(chamferEdges));
        m.insert(QStringLiteral("propagate_tangent"), chamferPropagateTangent->isChecked());
        auto referenceFace = chamferReferenceFace->currentData().value<QCborMap>();
        if (method != "EqualDistance" && !referenceFace.empty())
            m.insert(QStringLiteral("reference_face"), referenceFace);
        if (method == "EqualDistance") {
            m.insert(QStringLiteral("distance"), chamferDistance->value() * u);
        } else if (method == "DistanceDistance") {
            m.insert(QStringLiteral("distance1"), chamferDistance->value() * u);
            m.insert(QStringLiteral("distance2"), chamferDistance2->value() * u);
            m.insert(QStringLiteral("flip"), chamferFlip->isChecked());
        } else if (method == "AngleDistance") {
            m.insert(QStringLiteral("distance"), chamferDistance->value() * u);
            m.insert(QStringLiteral("angle"), chamferAngle->value() * std::numbers::pi / 180.0);
            m.insert(QStringLiteral("flip"), chamferFlip->isChecked());
        }
        return m;
    }

    if (kind == "Rib") {
        m.insert(QStringLiteral("operation"), QStringLiteral("Add"));
        m.insert(QStringLiteral("target"), makeTarget());

        QCborMap prof;
        auto profSketchId = profile->currentData().toString();
        prof.insert(QStringLiteral("sketch"), profSketchId);
        prof.insert(QStringLiteral("mode"), profileMode->currentText());
        QCborArray uses;
        for (auto skVal : document.value("sketches").toArray()) {
            auto sk = skVal.toMap();
            if (sk.value("id").toString() == profSketchId) {
                for (auto entVal : sk.value("entities").toArray()) {
                    auto ent = entVal.toMap();
                    if (!ent.value("construction").toBool()) {
                        QString ek = ent.value("kind").toString();
                        if (ek == "Line" || ek == "Arc" || ek == "Circle" || ek == "Spline") {
                            QCborMap uMap;
                            uMap.insert(QStringLiteral("entity"), ent.value("id").toString());
                            uMap.insert(QStringLiteral("from"), 0.0);
                            uMap.insert(QStringLiteral("to"), 1.0);
                            uMap.insert(QStringLiteral("reverse"), false);
                            uses.append(uMap);
                        }
                    }
                }
                break;
            }
        }
        prof.insert(QStringLiteral("regions"),
                    profileMode->currentText() == "AllClosed" ? QCborArray{}
                    : currentRegions.empty()                  ? QCborArray{QCborArray{uses}}
                                                              : currentRegions);
        m.insert(QStringLiteral("profile"), prof);

        m.insert(QStringLiteral("thickness"), ribThickness->value() * u);
        m.insert(QStringLiteral("side"), ribSide->currentText());
        m.insert(QStringLiteral("direction"), ribDirection->currentText());
        m.insert(QStringLiteral("flip_material"), ribFlipMaterial->isChecked());
        return m;
    }

    if (kind == "LinearPattern") {
        auto op = operation->currentText();
        m.insert(QStringLiteral("operation"), op);
        m.insert(QStringLiteral("target"), op == "NewBody" ? QCborValue(nullptr) : makeTarget());
        auto skind = patSeedKind->currentText();
        m.insert(QStringLiteral("seed_kind"), skind);
        if (skind == "Feature") {
            m.insert(QStringLiteral("seed"), patSeed->currentData().toString());
        } else if (skind == "Body") {
            m.insert(QStringLiteral("seed"), patSeed->currentData().value<QCborMap>());
        } else {
            m.insert(QStringLiteral("seed"), checkedEdges(patFaces));
        }

        QCborMap d1;
        QCborMap ref1;
        if (linearDir1Ref->currentData().canConvert<QCborMap>())
            ref1 = linearDir1Ref->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() &&
                 modelingDraft.value("direction1").toMap().contains(QStringLiteral("reference")))
            ref1 = modelingDraft.value("direction1").toMap().value("reference").toMap();
        d1.insert(QStringLiteral("reference"), ref1);
        d1.insert(QStringLiteral("spacing"), linearSpacing1->value() * u);
        d1.insert(QStringLiteral("count"), linearCount1->value());
        d1.insert(QStringLiteral("reverse"), linearReverse1->isChecked());
        m.insert(QStringLiteral("direction1"), d1);

        if (linearCount2->value() > 1) {
            QCborMap d2;
            QCborMap ref2;
            if (linearDir2Ref->currentData().canConvert<QCborMap>())
                ref2 = linearDir2Ref->currentData().value<QCborMap>();
            else if (!modelingDraft.isEmpty() && modelingDraft.value("direction2")
                                                     .toMap()
                                                     .contains(QStringLiteral("reference")))
                ref2 = modelingDraft.value("direction2").toMap().value("reference").toMap();
            d2.insert(QStringLiteral("reference"), ref2);
            d2.insert(QStringLiteral("spacing"), linearSpacing2->value() * u);
            d2.insert(QStringLiteral("count"), linearCount2->value());
            d2.insert(QStringLiteral("reverse"), linearReverse2->isChecked());
            m.insert(QStringLiteral("direction2"), d2);
        } else {
            m.insert(QStringLiteral("direction2"), QCborValue(nullptr));
        }
        return m;
    }

    if (kind == "CircularPattern") {
        auto op = operation->currentText();
        m.insert(QStringLiteral("operation"), op);
        m.insert(QStringLiteral("target"), op == "NewBody" ? QCborValue(nullptr) : makeTarget());
        auto skind = patSeedKind->currentText();
        m.insert(QStringLiteral("seed_kind"), skind);
        if (skind == "Feature") {
            m.insert(QStringLiteral("seed"), patSeed->currentData().toString());
        } else if (skind == "Body") {
            m.insert(QStringLiteral("seed"), patSeed->currentData().value<QCborMap>());
        } else {
            m.insert(QStringLiteral("seed"), checkedEdges(patFaces));
        }

        QCborMap axisRef;
        if (circAxisRef->currentData().canConvert<QCborMap>())
            axisRef = circAxisRef->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("axis")))
            axisRef = modelingDraft.value("axis").toMap();
        m.insert(QStringLiteral("axis"), axisRef);

        m.insert(QStringLiteral("count"), circCount->value());
        m.insert(QStringLiteral("equal_spacing"), circEqualSpacing->isChecked());
        m.insert(QStringLiteral("angle"), circAngle->value() * std::numbers::pi / 180.0);
        m.insert(QStringLiteral("reverse"), circReverse->isChecked());
        return m;
    }

    if (kind == "Mirror") {
        auto op = operation->currentText();
        m.insert(QStringLiteral("operation"), op);
        m.insert(QStringLiteral("target"), op == "NewBody" ? QCborValue(nullptr) : makeTarget());
        auto skind = patSeedKind->currentText();
        m.insert(QStringLiteral("seed_kind"), skind);
        if (skind == "Feature") {
            m.insert(QStringLiteral("seed"), patSeed->currentData().toString());
        } else if (skind == "Body") {
            m.insert(QStringLiteral("seed"), patSeed->currentData().value<QCborMap>());
        } else {
            m.insert(QStringLiteral("seed"), checkedEdges(patFaces));
        }

        QCborMap planeRef;
        if (mirrorPlaneRef->currentData().canConvert<QCborMap>())
            planeRef = mirrorPlaneRef->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("plane")))
            planeRef = modelingDraft.value("plane").toMap();
        m.insert(QStringLiteral("plane"), planeRef);
        return m;
    }

    if (kind == "CosmeticThread") {
        m.insert(QStringLiteral("target"), makeTarget());
        QCborMap edgeRef;
        if (threadEdgeRef->currentData().canConvert<QCborMap>())
            edgeRef = threadEdgeRef->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("reference")))
            edgeRef = modelingDraft.value("reference").toMap();
        m.insert(QStringLiteral("reference"), edgeRef);
        m.insert(QStringLiteral("designation"), threadDesignation->text().isEmpty()
                                                    ? QStringLiteral("M10x1.5")
                                                    : threadDesignation->text());
        m.insert(QStringLiteral("standard"), threadStandard->currentText());
        m.insert(QStringLiteral("type"), threadType->currentText());
        m.insert(QStringLiteral("size"),
                 threadSize->text().isEmpty() ? QStringLiteral("M10x1.5") : threadSize->text());
        m.insert(QStringLiteral("thread_type"), threadInternal->isChecked()
                                                    ? QStringLiteral("Internal")
                                                    : QStringLiteral("External"));
        double maj = threadMajorDia->value() * u;
        double min = threadMinorDia->value() * u;
        m.insert(QStringLiteral("major_diameter"), maj);
        m.insert(QStringLiteral("minor_diameter"), min);
        m.insert(QStringLiteral("pitch"), threadPitch->value() * u);
        QCborMap term;
        term.insert(QStringLiteral("mode"), threadTermination->currentText());
        if (threadTermination->currentText() == "Blind")
            term.insert(QStringLiteral("depth"), threadDepth->value() * u);
        m.insert(QStringLiteral("termination"), term);
        return m;
    }

    if (kind == "Shell") {
        m.insert(QStringLiteral("target"), makeTarget());
        m.insert(QStringLiteral("thickness"), shellThickness->value() * u);
        m.insert(QStringLiteral("inward"), shellInward->isChecked());

        QCborArray removeFaces;
        for (int i = 0; i < shellRemoveFaces->count(); ++i) {
            if (shellRemoveFaces->item(i)->checkState() == Qt::Checked) {
                removeFaces.append(shellRemoveFaces->item(i)->data(Qt::UserRole).value<QCborMap>());
            }
        }
        if (removeFaces.isEmpty() && !modelingDraft.isEmpty() &&
            modelingDraft.contains(QStringLiteral("remove_faces"))) {
            removeFaces = modelingDraft.value("remove_faces").toArray();
        }
        m.insert(QStringLiteral("remove_faces"), removeFaces);

        QCborArray overrides;
        for (int row = 0; row < shellOverrides->rowCount(); ++row) {
            auto *faceItem = shellOverrides->item(row, 0);
            auto *thickItem = shellOverrides->item(row, 1);
            if (faceItem && thickItem) {
                QCborMap ov;
                ov.insert(QStringLiteral("face"), faceItem->data(Qt::UserRole).value<QCborMap>());
                ov.insert(QStringLiteral("thickness"), thickItem->text().toDouble() * u);
                overrides.append(ov);
            }
        }
        if (overrides.isEmpty() && !modelingDraft.isEmpty() &&
            modelingDraft.contains(QStringLiteral("overrides"))) {
            overrides = modelingDraft.value("overrides").toArray();
        }
        m.insert(QStringLiteral("overrides"), overrides);
        return m;
    }

    if (kind == "Sweep") {
        auto op = operation->currentText();
        m.insert(QStringLiteral("operation"), op);
        m.insert(QStringLiteral("target"), op == "NewBody" ? QCborValue(nullptr) : makeTarget());

        QCborMap prof;
        if (sweepProfileType->currentText() == "Circular") {
            prof.insert(QStringLiteral("type"), QStringLiteral("Circular"));
            prof.insert(QStringLiteral("diameter"), sweepDiameter->value() * u);
        } else {
            prof.insert(QStringLiteral("type"), QStringLiteral("Sketch"));
            auto profSketchId = profile->currentData().toString();
            prof.insert(QStringLiteral("sketch"), profSketchId);
            prof.insert(QStringLiteral("mode"), QStringLiteral("AllClosed"));
            prof.insert(QStringLiteral("regions"), QCborArray{});
        }
        m.insert(QStringLiteral("profile"), prof);

        QCborMap pathMap;
        QCborMap pathRef;
        if (sweepPath->currentData().canConvert<QCborMap>()) {
            pathRef = sweepPath->currentData().value<QCborMap>();
        } else if (!sweepPath->currentData().toString().isEmpty()) {
            pathRef = makeRef(sweepPath->currentData().toString(), "Curve").map();
        } else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("path"))) {
            pathRef = modelingDraft.value("path").toMap().value("reference").toMap();
        }
        pathMap.insert(QStringLiteral("reference"), pathRef);
        pathMap.insert(QStringLiteral("reverse"), sweepPathReverse->isChecked());
        m.insert(QStringLiteral("path"), pathMap);

        QCborArray guides;
        for (int i = 0; i < sweepGuides->count(); ++i) {
            if (sweepGuides->item(i)->checkState() == Qt::Checked) {
                guides.append(sweepGuides->item(i)->data(Qt::UserRole).value<QCborMap>());
            }
        }
        if (guides.isEmpty() && !modelingDraft.isEmpty() &&
            modelingDraft.contains(QStringLiteral("guides"))) {
            guides = modelingDraft.value("guides").toArray();
        }
        m.insert(QStringLiteral("guides"), guides);

        m.insert(QStringLiteral("corner_transition"), sweepCornerTransition->currentText());
        m.insert(QStringLiteral("start_tangency"), sweepStartTangency->currentText());
        m.insert(QStringLiteral("end_tangency"), sweepEndTangency->currentText());

        QCborMap th;
        th.insert(QStringLiteral("mode"), thinMode->currentText());
        if (thinMode->currentText() == "OneDirection" || thinMode->currentText() == "Midplane") {
            th.insert(QStringLiteral("thickness"), thickness1->value() * u);
            th.insert(QStringLiteral("reverse"), thinReverse->isChecked());
        } else if (thinMode->currentText() == "TwoDirection") {
            th.insert(QStringLiteral("thickness1"), thickness1->value() * u);
            th.insert(QStringLiteral("thickness2"), thickness2->value() * u);
        }
        m.insert(QStringLiteral("thin"), th);

        m.insert(QStringLiteral("preview_sections"), sweepPreviewSections->value());
        return m;
    }

    if (kind == "Loft") {
        auto out = loftOutput->currentText();
        m.insert(QStringLiteral("output"), out);
        auto op = out == "Surface" ? QStringLiteral("NewBody") : operation->currentText();
        m.insert(QStringLiteral("operation"), op);
        m.insert(QStringLiteral("target"),
                 (out == "Surface" || op == "NewBody") ? QCborValue(nullptr) : makeTarget());

        QCborArray sections;
        for (int i = 0; i < loftSections->count(); ++i) {
            auto sId = loftSections->item(i)->data(Qt::UserRole).toString();
            if (!sId.isEmpty()) {
                QCborMap sec;
                sec.insert(QStringLiteral("sketch"), sId);
                sec.insert(QStringLiteral("mode"), QStringLiteral("AllClosed"));
                sec.insert(QStringLiteral("regions"), QCborArray{});
                sections.append(sec);
            }
        }
        if (sections.isEmpty() && !modelingDraft.isEmpty() &&
            modelingDraft.contains(QStringLiteral("sections"))) {
            sections = modelingDraft.value("sections").toArray();
        }
        m.insert(QStringLiteral("sections"), sections);

        if (loftCenterline->currentIndex() > 0 && !loftCenterline->currentData().isNull()) {
            QCborMap clMap;
            QCborMap clRef;
            if (loftCenterline->currentData().canConvert<QCborMap>()) {
                clRef = loftCenterline->currentData().value<QCborMap>();
            } else if (!loftCenterline->currentData().toString().isEmpty()) {
                clRef = makeRef(loftCenterline->currentData().toString(), "Curve").map();
            } else if (!modelingDraft.isEmpty() &&
                       modelingDraft.contains(QStringLiteral("centerline")) &&
                       !modelingDraft.value("centerline").isNull()) {
                clRef = modelingDraft.value("centerline").toMap().value("reference").toMap();
            }
            if (!clRef.isEmpty()) {
                clMap.insert(QStringLiteral("reference"), clRef);
                clMap.insert(QStringLiteral("reverse"), loftCenterlineReverse->isChecked());
                m.insert(QStringLiteral("centerline"), clMap);
            } else {
                m.insert(QStringLiteral("centerline"), QCborValue(nullptr));
            }
        } else if (!modelingDraft.isEmpty() &&
                   modelingDraft.contains(QStringLiteral("centerline"))) {
            m.insert(QStringLiteral("centerline"), modelingDraft.value("centerline"));
        } else {
            m.insert(QStringLiteral("centerline"), QCborValue(nullptr));
        }

        QCborArray guides;
        for (int i = 0; i < loftGuides->count(); ++i) {
            if (loftGuides->item(i)->checkState() == Qt::Checked) {
                guides.append(loftGuides->item(i)->data(Qt::UserRole).value<QCborMap>());
            }
        }
        if (guides.isEmpty() && !modelingDraft.isEmpty() &&
            modelingDraft.contains(QStringLiteral("guides"))) {
            guides = modelingDraft.value("guides").toArray();
        }
        m.insert(QStringLiteral("guides"), guides);

        QCborArray connectors;
        for (int row = 0; row < loftConnectors->rowCount(); ++row) {
            auto *idItem = loftConnectors->item(row, 0);
            auto *ptsItem = loftConnectors->item(row, 1);
            if (idItem && ptsItem) {
                QCborMap c;
                c.insert(QStringLiteral("id"), idItem->text());
                QCborArray ptArr;
                for (const auto &pStr : ptsItem->text().split(',', Qt::SkipEmptyParts)) {
                    ptArr.append(pStr.trimmed().toDouble());
                }
                c.insert(QStringLiteral("points"), ptArr);
                connectors.append(c);
            }
        }
        if (connectors.isEmpty() && !modelingDraft.isEmpty() &&
            modelingDraft.contains(QStringLiteral("connectors"))) {
            connectors = modelingDraft.value("connectors").toArray();
        }
        m.insert(QStringLiteral("connectors"), connectors);

        m.insert(QStringLiteral("start_condition"), loftStartCondition->currentText());
        m.insert(QStringLiteral("end_condition"), loftEndCondition->currentText());

        QCborMap th;
        if (out == "Surface") {
            th.insert(QStringLiteral("mode"), QStringLiteral("Solid"));
        } else {
            th.insert(QStringLiteral("mode"), thinMode->currentText());
            if (thinMode->currentText() == "OneDirection" ||
                thinMode->currentText() == "Midplane") {
                th.insert(QStringLiteral("thickness"), thickness1->value() * u);
                th.insert(QStringLiteral("reverse"), thinReverse->isChecked());
            } else if (thinMode->currentText() == "TwoDirection") {
                th.insert(QStringLiteral("thickness1"), thickness1->value() * u);
                th.insert(QStringLiteral("thickness2"), thickness2->value() * u);
            }
        }
        m.insert(QStringLiteral("thin"), th);

        return m;
    }

    // Extrude or Revolve
    auto op = operation->currentText();
    m.insert(QStringLiteral("operation"), op);
    if (op == "NewBody") {
        m.insert(QStringLiteral("target"), QCborValue(nullptr));
    } else {
        m.insert(QStringLiteral("target"), makeTarget());
    }

    // profile
    QCborMap prof;
    auto profSketchId = profile->currentData().toString();
    prof.insert(QStringLiteral("sketch"), profSketchId);
    auto pMode = profileMode->currentText();
    prof.insert(QStringLiteral("mode"), pMode);
    QCborArray regions = currentRegions;
    if (regions.isEmpty() && pMode == "OpenChains") {
        QCborArray uses;
        for (auto skVal : document.value("sketches").toArray()) {
            auto sk = skVal.toMap();
            if (sk.value("id").toString() == profSketchId) {
                for (auto entVal : sk.value("entities").toArray()) {
                    auto ent = entVal.toMap();
                    if (!ent.value("construction").toBool()) {
                        QString ek = ent.value("kind").toString();
                        if (ek == "Line" || ek == "Arc" || ek == "Circle" || ek == "Spline") {
                            QCborMap uMap;
                            uMap.insert(QStringLiteral("entity"), ent.value("id").toString());
                            uMap.insert(QStringLiteral("from"), 0.0);
                            uMap.insert(QStringLiteral("to"), 1.0);
                            uMap.insert(QStringLiteral("reverse"), false);
                            uses.append(uMap);
                        }
                    }
                }
                break;
            }
        }
        if (!uses.isEmpty()) {
            regions = QCborArray{QCborArray{uses}};
        }
    }
    prof.insert(QStringLiteral("regions"), regions);
    m.insert(QStringLiteral("profile"), prof);

    // start
    QCborMap st;
    auto sMode = (kind == "Revolve") ? QString("SketchPlane") : startMode->currentText();
    st.insert(QStringLiteral("mode"), sMode);
    if (sMode == "Offset") {
        st.insert(QStringLiteral("distance"), startOffset->value() * u);
    } else if (sMode == "Surface" || sMode == "Vertex") {
        QCborMap refMap;
        if (startReference->currentData().canConvert<QCborMap>())
            refMap = startReference->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() &&
                 modelingDraft.value("start").toMap().contains(QStringLiteral("reference")))
            refMap = modelingDraft.value("start").toMap().value("reference").toMap();
        st.insert(QStringLiteral("reference"), refMap);
    }
    m.insert(QStringLiteral("start"), st);

    // direction 1
    QCborMap d1;
    auto ext1 = (kind == "Revolve") ? revolveExtent->currentText() : extent->currentText();
    d1.insert(QStringLiteral("mode"), ext1);
    if (ext1 == "Blind" || ext1 == "Midplane") {
        if (kind == "Revolve")
            d1.insert(QStringLiteral("value"), angle->value() * std::numbers::pi / 180.0);
        else
            d1.insert(QStringLiteral("value"), depth->value() * u);
    } else if (ext1 == "UpToVertex" || ext1 == "UpToSurface" || ext1 == "OffsetFromSurface" ||
               ext1 == "UpToBody") {
        QCborMap refMap;
        if (dir1Reference->currentData().canConvert<QCborMap>())
            refMap = dir1Reference->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() &&
                 modelingDraft.value("direction1").toMap().contains(QStringLiteral("reference")))
            refMap = modelingDraft.value("direction1").toMap().value("reference").toMap();
        d1.insert(QStringLiteral("reference"), refMap);
        if (ext1 == "UpToBody") {
            auto bodyId =
                dir1Reference->currentData().canConvert<QCborMap>()
                    ? dir1Reference->currentData().value<QCborMap>().value("object").toString()
                    : dir1Reference->currentData().toString();
            d1.insert(QStringLiteral("body"),
                      dir1Reference->currentData(Qt::UserRole + 1).toString().isEmpty()
                          ? bodyId
                          : dir1Reference->currentData(Qt::UserRole + 1).toString());
        } else if (ext1 == "OffsetFromSurface") {
            d1.insert(QStringLiteral("offset"), surfaceOffset->value() * u);
            d1.insert(QStringLiteral("offset_reverse"), offsetReverse->isChecked());
        }
    }
    if (kind != "Revolve") {
        QCborMap d1Draft;
        d1Draft.insert(QStringLiteral("angle"),
                       (draftEnable->isChecked() ? draftAngle->value() : 0.0) * std::numbers::pi /
                           180.0);
        d1Draft.insert(QStringLiteral("outward"), draftOutward->isChecked());
        d1.insert(QStringLiteral("draft"), d1Draft);
    }
    m.insert(QStringLiteral("direction1"), d1);

    // direction 2
    bool midplane1 = (ext1 == "Midplane");
    if (direction2Enable->isChecked() && !midplane1) {
        QCborMap d2;
        auto ext2 = (kind == "Revolve") ? QString("Blind") : extent2->currentText();
        d2.insert(QStringLiteral("mode"), ext2);
        if (kind == "Revolve") {
            d2.insert(QStringLiteral("value"), angle2->value() * std::numbers::pi / 180.0);
        } else {
            if (ext2 == "Blind") {
                d2.insert(QStringLiteral("value"), depth2->value() * u);
            } else if (ext2 == "UpToVertex" || ext2 == "UpToSurface" ||
                       ext2 == "OffsetFromSurface" || ext2 == "UpToBody") {
                QCborMap refMap;
                if (dir2Reference->currentData().canConvert<QCborMap>())
                    refMap = dir2Reference->currentData().value<QCborMap>();
                else if (!modelingDraft.isEmpty() && modelingDraft.value("direction2")
                                                         .toMap()
                                                         .contains(QStringLiteral("reference")))
                    refMap = modelingDraft.value("direction2").toMap().value("reference").toMap();
                d2.insert(QStringLiteral("reference"), refMap);
                if (ext2 == "UpToBody") {
                    auto bodyId = dir2Reference->currentData().canConvert<QCborMap>()
                                      ? dir2Reference->currentData()
                                            .value<QCborMap>()
                                            .value("object")
                                            .toString()
                                      : dir2Reference->currentData().toString();
                    d2.insert(QStringLiteral("body"),
                              dir2Reference->currentData(Qt::UserRole + 1).toString().isEmpty()
                                  ? bodyId
                                  : dir2Reference->currentData(Qt::UserRole + 1).toString());
                } else if (ext2 == "OffsetFromSurface") {
                    d2.insert(QStringLiteral("offset"), surfaceOffset2->value() * u);
                    d2.insert(QStringLiteral("offset_reverse"), offsetReverse2->isChecked());
                }
            }
            QCborMap d2Draft;
            d2Draft.insert(QStringLiteral("angle"),
                           (draft2Enable->isChecked() ? draft2Angle->value() : 0.0) *
                               std::numbers::pi / 180.0);
            d2Draft.insert(QStringLiteral("outward"), draft2Outward->isChecked());
            d2.insert(QStringLiteral("draft"), d2Draft);
        }
        m.insert(QStringLiteral("direction2"), d2);
    } else {
        m.insert(QStringLiteral("direction2"), QCborValue(nullptr));
    }

    // reverse
    m.insert(QStringLiteral("reverse"), reverseDirection->isChecked());

    // thin
    QCborMap th;
    auto tMode = thinMode->currentText();
    th.insert(QStringLiteral("mode"), tMode);
    if (tMode == "OneDirection" || tMode == "Midplane") {
        th.insert(QStringLiteral("thickness"), thickness1->value() * u);
        if (tMode == "OneDirection")
            th.insert(QStringLiteral("reverse"), thinReverse->isChecked());
    } else if (tMode == "TwoDirection") {
        th.insert(QStringLiteral("thickness1"), thickness1->value() * u);
        th.insert(QStringLiteral("thickness2"), thickness2->value() * u);
    }
    m.insert(QStringLiteral("thin"), th);

    // revolve axis
    if (kind == "Revolve") {
        QCborMap axisMap;
        if (revolveAxis->currentData().canConvert<QCborMap>())
            axisMap = revolveAxis->currentData().value<QCborMap>();
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("axis")))
            axisMap = modelingDraft.value("axis").toMap();
        m.insert(QStringLiteral("axis"), axisMap);
    }

    // open non-thin cut flip_side
    if (pMode == "OpenChains" && tMode == "Solid" && kind == "Extrude" && op == "Cut") {
        m.insert(QStringLiteral("flip_side"), flipSide->isChecked());
    }

    return m;
}

QCborArray FeaturePanel::definitionsModeling() const {
    auto m = definitionModeling();
    features::ModelingIntent::parse(m);

    auto all = document.value("modeling").toMap().value("intents").toArray();
    bool found = false;
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (all[i].toMap().value("id").toString() == draft.id) {
            all[i] = m;
            found = true;
            break;
        }
    }
    if (!found)
        all.append(m);
    return all;
}

void FeaturePanel::preview() {
    try {
        if (isDerivedCurve) {
            curveDraft.id =
                draft.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : draft.id;
            curveDraft.name =
                name->text().trimmed().isEmpty() ? draft.kind : name->text().trimmed();
            if (draft.kind == "HelixSpiral") {
                curveDraft.kind = "HelixSpiral";
                model::HelixSpiralDefinition hd;
                hd.mode = helixMode->currentText();
                domain::GeometryReference axRef;
                QString axisObj = helixAxis->currentData().toString();
                if (!wire::uuid(axisObj)) {
                    QString axName = axisObj.startsWith("axis/") ? axisObj.mid(5) : "Z";
                    auto sup =
                        model::SupportDefinition::parse(document.value("model_supports").toMap());
                    QString foundId;
                    for (const auto &d : sup.datums) {
                        if (d.kind == "DatumAxis" && d.mode == "Principal" &&
                            d.parameters.value("axis").toString() == axName) {
                            foundId = d.id;
                            break;
                        }
                    }
                    if (foundId.isEmpty()) {
                        model::Datum axDatum;
                        axDatum.kind = "DatumAxis";
                        axDatum.mode = "Principal";
                        axDatum.parameters = sketch::fields({{"axis", axName}});
                        sup.datums.push_back(axDatum);
                        sup.sequence();
                        foundId = axDatum.id;
                        document.insert(QStringLiteral("model_supports"), sup.map());
                    }
                    axisObj = foundId;
                }
                axRef.object = axisObj;
                axRef.role = "Axis";
                hd.axis = axRef;
                if (helixCenter->currentIndex() > 0 &&
                    !helixCenter->currentData().toString().isEmpty()) {
                    domain::GeometryReference cRef;
                    cRef.object = helixCenter->currentData().toString();
                    cRef.role = "Point";
                    hd.center = cRef;
                }
                hd.radius = helixRadius->value() * unit();
                hd.pitch = helixPitch->value() * unit();
                hd.height = helixHeight->value() * unit();
                hd.revolutions = helixRevolutions->value();
                hd.startAngle = helixStartAngle->value() * std::numbers::pi / 180.0;
                hd.taperAngle = helixTaperAngle->value() * std::numbers::pi / 180.0;
                hd.clockwise = helixClockwise->isChecked();
                curveDraft.helixSpiral = hd;
                curveDraft.validate();
                diagnostic->setText("Preview ready · Helix/spiral valid");
                validPreview = true;
                accept->setEnabled(true);
                return;
            } else if (draft.kind == "CompositeCurve") {
                curveDraft.kind = "CompositeCurve";
                model::CompositeCurveDefinition cd;
                cd.closed = compositeClosed->isChecked();
                for (int i = 0; i < compositeMembers->count(); ++i) {
                    auto item = compositeMembers->item(i);
                    if (item->checkState() == Qt::Checked) {
                        model::CompositeCurveMember m;
                        m.source = domain::GeometryReference::parse(
                            item->data(Qt::UserRole).value<QCborMap>());
                        cd.members.push_back(m);
                    }
                }
                curveDraft.compositeCurve = cd;
                curveDraft.validate();
                diagnostic->setText("Preview ready · Composite curve valid");
                validPreview = true;
                accept->setEnabled(true);
                return;
            } else if (draft.kind == "ProjectedCurve") {
                auto pmode = projectedMode->currentText();
                curveDraft.kind = pmode;
                if (pmode == "SketchOnSketch") {
                    model::SketchOnSketchDefinition sos;
                    domain::GeometryReference r1, r2;
                    r1.object = projectedSketch1->currentData().toString();
                    r1.role = "Object";
                    r2.object = projectedSketch2->currentData().toString();
                    r2.role = "Object";
                    sos.sketch1 = r1;
                    sos.sketch2 = r2;
                    sos.branch.index = projectedBranch->value();
                    sos.branch.token = QString("branch/%1").arg(projectedBranch->value());
                    sos.branch.reversed = projectedReversed->isChecked();
                    sos.branch.startParam = projectedStartParam->value();
                    sos.branch.endParam = projectedEndParam->value();
                    curveDraft.sketchOnSketch = sos;
                    curveDraft.validate();
                    projectedBranchLabel->setText(
                        QString("Branch index %1").arg(projectedBranch->value()));
                    diagnostic->setText("Preview ready · Projected curve valid");
                    validPreview = true;
                    accept->setEnabled(true);
                    return;
                } else if (pmode == "SketchOnFaces") {
                    model::SketchOnFacesDefinition sof;
                    domain::GeometryReference r;
                    r.object = projectedFaceSketch->currentData().toString();
                    r.role = "Object";
                    sof.sketch = r;
                    sof.mode = projectedVectorMode->currentText();
                    if (sof.mode == "AlongVector") {
                        domain::GeometryReference dirRef;
                        QString dirObj = projectedDirection->currentData().toString();
                        if (!wire::uuid(dirObj)) {
                            QString axName = dirObj.startsWith("axis/") ? dirObj.mid(5) : "Z";
                            auto sup = model::SupportDefinition::parse(
                                document.value("model_supports").toMap());
                            QString foundId;
                            for (const auto &d : sup.datums) {
                                if (d.kind == "DatumAxis" && d.mode == "Principal" &&
                                    d.parameters.value("axis").toString() == axName) {
                                    foundId = d.id;
                                    break;
                                }
                            }
                            if (foundId.isEmpty()) {
                                model::Datum axDatum;
                                axDatum.kind = "DatumAxis";
                                axDatum.mode = "Principal";
                                axDatum.parameters = sketch::fields({{"axis", axName}});
                                sup.datums.push_back(axDatum);
                                sup.sequence();
                                foundId = axDatum.id;
                                document.insert(QStringLiteral("model_supports"), sup.map());
                            }
                            dirObj = foundId;
                        }
                        dirRef.object = dirObj;
                        dirRef.role = "Axis";
                        sof.direction = dirRef;
                    }
                    for (int i = 0; i < projectedFaces->count(); ++i) {
                        auto item = projectedFaces->item(i);
                        if (item->checkState() == Qt::Checked)
                            sof.faces.push_back(domain::GeometryReference::parse(
                                item->data(Qt::UserRole).value<QCborMap>()));
                    }
                    sof.branch.index = projectedBranch->value();
                    sof.branch.token = QString("branch/%1").arg(projectedBranch->value());
                    sof.branch.reversed = projectedReversed->isChecked();
                    sof.branch.startParam = projectedStartParam->value();
                    sof.branch.endParam = projectedEndParam->value();
                    curveDraft.sketchOnFaces = sof;
                    curveDraft.validate();
                    projectedBranchLabel->setText(
                        QString("Branch index %1").arg(projectedBranch->value()));
                    diagnostic->setText("Preview ready · Projected curve valid");
                    validPreview = true;
                    accept->setEnabled(true);
                    return;
                }
            }
        }
        if (isModeling) {
            auto profId = profile->currentData().toString();
            if (document.contains(QStringLiteral("model_supports"))) {
                auto sup =
                    model::SupportDefinition::parse(document.value("model_supports").toMap());
                for (const auto &c : model::parseDerivedCurves(sup.derived_curves)) {
                    if (c.id == profId) {
                        diagnostic->setText("Selected entity is a reference curve, not a solid "
                                            "body or planar profile");
                        validPreview = false;
                        accept->setEnabled(false);
                        return;
                    }
                }
                for (const auto &s : sketch::parseSpatial(sup.spatial_sketches)) {
                    if (s.id == profId) {
                        diagnostic->setText("Selected entity is a reference curve, not a solid "
                                            "body or planar profile");
                        validPreview = false;
                        accept->setEnabled(false);
                        return;
                    }
                }
            }
            candidateIntents = definitionsModeling();
            validPreview = false;
            supportPreviewPending = true;
            auto model = model::editableSupports(document);
            client->projectCommand("PreviewModelSupports", fields({{"model", model.map()},
                                                                   {"intents", candidateIntents},
                                                                   {"display", true},
                                                                   {"deflection_mm", .2}}));
            accept->setEnabled(false);
            return;
        }
        candidate = definitions();
        validPreview = false;
        if (document.contains(QStringLiteral("model_supports"))) {
            supportPreviewPending = true;
            client->projectCommand(
                "PreviewModelSupports",
                fields({{"model", model::editSupportedFeatures(document, candidate).map()}}));
        } else
            client->projectCommand("PreviewFeatures",
                                   fields({{"features", candidate}, {"deflection_mm", .2}}));
    } catch (const std::exception &e) {
        diagnostic->setText(QString::fromUtf8(e.what()));
        validPreview = false;
    }
    accept->setEnabled(false);
}

void FeaturePanel::finish() {
    if (!editing || !validPreview || client->outstanding())
        return;
    try {
        if (isDerivedCurve) {
            auto sup = model::SupportDefinition::parse(document.value("model_supports").toMap());
            auto curves = model::parseDerivedCurves(sup.derived_curves);
            bool replaced = false;
            for (auto &existing : curves) {
                if (existing.id == curveDraft.id) {
                    existing = curveDraft;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                curves.push_back(curveDraft);
            auto modelDef =
                model::editSupportedDerivedCurves(document, model::encodeDerivedCurves(curves));
            commitPending = true;
            validPreview = false;
            client->projectCommand("SetModelSupports", fields({{"model", modelDef.map()}}));
            editing = false;
            isDerivedCurve = false;
            emit editingChanged();
            return;
        }
        if (isModeling) {
            auto current = definitionsModeling();
            if (current != candidateIntents) {
                invalidate();
                return;
            }
            validPreview = false;
            commitPending = true;
            auto model = model::editableSupports(document);
            client->projectCommand("SetModelSupports",
                                   fields({{"model", model.map()}, {"intents", current}}));
            return;
        }
        auto current = definitions();
        if (current != candidate) {
            invalidate();
            return;
        }
        validPreview = false;
        commitPending = true;
        if (document.contains(QStringLiteral("model_supports")))
            client->projectCommand(
                "SetModelSupports",
                fields({{"model", model::editSupportedFeatures(document, current).map()}}));
        else
            client->projectCommand("SetFeatures", fields({{"features", current}}));
    } catch (const std::exception &e) {
        commitPending = false;
        diagnostic->setText(QString::fromUtf8(e.what()));
        return;
    }
}

void FeaturePanel::discard() {
    if (!editing || commitPending)
        return;
    if (isDerivedCurve) {
        editing = false;
        isDerivedCurve = false;
        inputs->hide();
        accept->hide();
        previewButton->hide();
        cancelButton->hide();
        emit clearManipulatorRequested();
        emit editingChanged();
        return;
    }
    editing = false;
    isModeling = false;
    previewAfterInputs = false;
    validPreview = false;
    inputs->hide();
    accept->hide();
    previewButton->hide();
    cancelButton->hide();
    if (repairButton)
        repairButton->hide();
    form->setRowVisible(repairCandidate, false);
    form->setRowVisible(applyRepairButton, false);
    emit clearManipulatorRequested();
    client->cancelPartPreview();
    emit editingChanged();
}

void FeaturePanel::updateManipulator() {
    if (!editing || !isModeling) {
        emit clearManipulatorRequested();
        return;
    }
    QString selected = handleParameter->currentText();
    {
        QSignalBlocker block(handleParameter);
        handleParameter->clear();
        if (draft.kind == "Draft")
            handleParameter->addItem("Draft angle");
        else if (draft.kind == "Revolve") {
            handleParameter->addItem("Direction 1 angle");
            if (direction2Enable->isChecked() && revolveExtent->currentText() != "Midplane")
                handleParameter->addItem("Direction 2 angle");
        } else if (draft.kind == "Extrude") {
            if (extent->currentText() == "Blind" || extent->currentText() == "Midplane")
                handleParameter->addItem("Direction 1 depth");
            if (draftEnable->isChecked())
                handleParameter->addItem("Direction 1 draft");
            if (direction2Enable->isChecked() && extent->currentText() != "Midplane") {
                if (extent2->currentText() == "Blind")
                    handleParameter->addItem("Direction 2 depth");
                if (draft2Enable->isChecked())
                    handleParameter->addItem("Direction 2 draft");
            }
        }
        if ((draft.kind == "Extrude" || draft.kind == "Revolve") &&
            thinMode->currentText() != "Solid") {
            handleParameter->addItem("Thickness 1");
            if (thinMode->currentText() == "TwoDirection")
                handleParameter->addItem("Thickness 2");
        }
        int index = handleParameter->findText(selected);
        if (index >= 0)
            handleParameter->setCurrentIndex(index);
    }
    auto parameter = handleParameter->currentText();
    handleField = nullptr;
    angularHandle = parameter.contains("angle", Qt::CaseInsensitive) ||
                    parameter.contains("draft", Qt::CaseInsensitive);
    if (parameter == "Draft angle")
        handleField = draftStandaloneAngle;
    else if (parameter == "Direction 1 angle")
        handleField = angle;
    else if (parameter == "Direction 2 angle")
        handleField = angle2;
    else if (parameter == "Direction 1 draft")
        handleField = draftAngle;
    else if (parameter == "Direction 2 draft")
        handleField = draft2Angle;
    else if (parameter == "Direction 1 depth")
        handleField = depth;
    else if (parameter == "Direction 2 depth")
        handleField = depth2;
    else if (parameter == "Thickness 1")
        handleField = thickness1;
    else if (parameter == "Thickness 2")
        handleField = thickness2;
    if (!handleField) {
        emit clearManipulatorRequested();
        return;
    }
    QVector3D orig, normal(0, 0, 1), u(1, 0, 0);
    for (auto value :
         inputSketches.empty() ? document.value("sketches").toArray() : inputSketches) {
        auto sketch = value.toMap();
        if (sketch.value("id").toString() != profile->currentData().toString())
            continue;
        auto frame = sketch.value("frame").toMap();
        auto read = [](QCborArray a, double scale) {
            return a.size() == 3
                       ? QVector3D(float(a[0].toDouble() * scale), float(a[1].toDouble() * scale),
                                   float(a[2].toDouble() * scale))
                       : QVector3D();
        };
        orig = read(frame.value("origin").toArray(), 1000);
        u = read(frame.value("u").toArray(), 1);
        normal = QVector3D::crossProduct(u, read(frame.value("v").toArray(), 1)).normalized();
        break;
    }
    auto geometryFor = [&](QComboBox *box) {
        auto wanted = box->currentData().value<QCborMap>();
        for (auto value : inputGeometry)
            if (value.toMap().value("reference").toMap() == wanted)
                return value.toMap();
        return QCborMap{};
    };
    auto readVector = [](QCborValue value, double scale) {
        auto a = value.toArray();
        return a.size() == 3
                   ? QVector3D(float(a[0].toDouble() * scale), float(a[1].toDouble() * scale),
                               float(a[2].toDouble() * scale))
                   : QVector3D();
    };
    if (draft.kind == "Revolve" || draft.kind == "Draft") {
        auto axisGeometry = geometryFor(draft.kind == "Revolve" ? revolveAxis : draftPull);
        auto support = draft.kind == "Draft" ? geometryFor(draftNeutral) : axisGeometry;
        if (!support.empty())
            orig = readVector(support.value("origin"), 1000);
        if (!axisGeometry.empty())
            normal = readVector(axisGeometry.value("direction"), 1);
    } else if (startMode->currentText() == "Vertex" || startMode->currentText() == "Surface") {
        auto start = geometryFor(startReference);
        if (start.empty()) {
            emit clearManipulatorRequested();
            return;
        }
        auto point = readVector(start.value("origin"), 1000);
        auto stationNormal =
            startMode->currentText() == "Vertex" ? normal : readVector(start.value("direction"), 1);
        auto denominator = QVector3D::dotProduct(normal, stationNormal);
        if (std::abs(denominator) < 1e-7) {
            emit clearManipulatorRequested();
            return;
        }
        orig += normal * (QVector3D::dotProduct(point - orig, stationNormal) / denominator);
    }
    if (startMode->currentText() == "Offset")
        orig += normal * float(startOffset->value() * unit() * 1000);
    if (parameter.startsWith("Thickness"))
        normal = u;
    bool reverse = reverseDirection->isChecked() != parameter.startsWith("Direction 2");
    if (angularHandle)
        emit requestDraftManipulator(orig, normal, handleField->value(), handleField->maximum());
    else
        emit requestManipulator(orig, normal, handleField->value() * unit() * 1000, reverse);
}

void FeaturePanel::setDepthFromManipulator(double depthMM) {
    if (editing && handleField && !angularHandle)
        handleField->setValue(depthMM / (unit() * 1000));
}
void FeaturePanel::setDepthFinishedFromManipulator(double depthMM) {
    setDepthFromManipulator(depthMM);
    if (editing)
        preview();
}
void FeaturePanel::setDraftAngleFromManipulator(double angleDeg) {
    if (editing && handleField && angularHandle)
        handleField->setValue(angleDeg);
}
void FeaturePanel::setDraftAngleFinishedFromManipulator(double angleDeg) {
    setDraftAngleFromManipulator(angleDeg);
    if (editing)
        preview();
}

} // namespace cad

#include "FeaturePanel.hpp"
#include "SupportEditor.hpp"
#include "model/SupportEditing.hpp"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
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
    draftAngle = spin("featureDraftAngle", "Draft angle (degrees)", 0, 0, 89.9);
    draftOutward = check("featureDraftOutward", "Draft outward");
    direction2Enable = check("featureDirection2Enable", "Direction 2");
    extent2 = combo("featureExtent2", "Direction 2 end condition");
    extent2->addItems({"Blind", "UpToVertex", "UpToSurface", "OffsetFromSurface", "UpToBody",
                       "UpToNext", "ThroughAll"});
    depth2 = spin("featureDepth2", "Direction 2 depth", 10, 1e-6, 1e6);
    reverseDirection2 = check("featureReverse2", "Reverse direction 2");
    dir2Reference = combo("featureDir2Reference", "Direction 2 reference");
    draft2Enable = check("featureDraft2Enable", "Direction 2 Draft On/Off");
    draft2Angle = spin("featureDraft2Angle", "Direction 2 draft angle (degrees)", 0, 0, 89.9);
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

    // Standalone Draft controls
    draftNeutral = combo("draftNeutral", "Neutral plane / face");
    draftPull = combo("draftPull", "Pull direction axis / edge");
    draftFaces = combo("draftFaces", "Faces to draft");
    draftStandaloneAngle = spin("draftStandaloneAngle", "Draft angle (degrees)", 5, 0.001, 89.9);
    draftStandaloneOutward = check("draftStandaloneOutward", "Draft outward");

    // Repair controls
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
        if (draft.kind == "Draft") {
            draftNeutral->addItem(chosen.value("object").toString(), QVariant::fromValue(chosen));
            draftNeutral->setCurrentIndex(draftNeutral->count() - 1);
        } else {
            dir1Reference->addItem(chosen.value("object").toString(), QVariant::fromValue(chosen));
            dir1Reference->setCurrentIndex(dir1Reference->count() - 1);
        }
        invalidate();
        preview();
    });
    for (auto *c : {profileMode, startMode, extent, extent2, thinMode, operation, revolveExtent})
        connect(c, &QComboBox::currentIndexChanged, this, [this] { fieldsForKind(); });
    for (auto *k : {draftEnable, direction2Enable, draft2Enable, flipSide, thinReverse})
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
        for (auto val : resolution.value("candidates").toArray()) {
            auto ref = val.toMap();
            repairCandidate->addItem(
                QString("%1: %2").arg(ref.value("role").toString(), ref.value("object").toString()),
                QVariant::fromValue(ref));
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
                QString path = "direction1/reference";
                if (draft.kind == "Draft")
                    path = "neutral";
                client->projectCommand(
                    "ModelingReferenceCandidates",
                    fields({{"model", model.map()},
                            {"intents",
                             candidateIntents.empty() ? QCborArray{cand} : candidateIntents},
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
            emit editingChanged();
        }
    });
    connect(client, &Client::changed, this, [this] {
        if (editing) {
            bool idle = !client->outstanding();
            accept->setEnabled(idle && validPreview &&
                               client->state().phase == Phase::PreviewReady);
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
    auto kind = draft.kind;
    bool isExtrude = (kind == "Extrude");
    bool isRevolve = (kind == "Revolve");
    bool isDraft = (kind == "Draft");
    bool isSketch = isExtrude || isRevolve;
    bool isTreatment = (kind == "Fillet" || kind == "Chamfer");
    bool isHole = (kind == "Hole");
    bool isPattern = (kind == "LinearPattern" || kind == "CircularPattern" || kind == "Mirror");

    auto show = [&](QWidget *w, bool yes) {
        if (w)
            form->setRowVisible(w, yes);
    };

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

        if (isDraft) {
            // Standalone draft
            show(profile, false);
            show(profileMode, false);
            show(flipSide, false);
            show(operation, false);
            show(target, true); // target body
            show(startMode, false);
            show(startOffset, false);
            show(startReference, false);
            show(extent, false);
            show(depth, false);
            show(angle, false);
            show(reverseDirection, false);
            show(surfaceOffset, false);
            show(offsetReverse, false);
            show(dir1Reference, false);
            show(draftEnable, false);
            show(draftAngle, false);
            show(draftOutward, false);
            show(direction2Enable, false);
            show(extent2, false);
            show(depth2, false);
            show(reverseDirection2, false);
            show(dir2Reference, false);
            show(draft2Enable, false);
            show(draft2Angle, false);
            show(draft2Outward, false);
            show(thinMode, false);
            show(thickness1, false);
            show(thickness2, false);
            show(thinReverse, false);
            show(revolveAxis, false);
            show(revolveExtent, false);
            show(angle2, false);

            show(draftNeutral, true);
            show(draftPull, true);
            show(draftFaces, true);
            show(draftStandaloneAngle, true);
            show(draftStandaloneOutward, true);
        } else {
            // Extrude or Revolve
            show(draftNeutral, false);
            show(draftPull, false);
            show(draftFaces, false);
            show(draftStandaloneAngle, false);
            show(draftStandaloneOutward, false);

            show(profile, true);
            show(profileMode, true);
            show(operation, true);
            show(target, operation->currentText() != "NewBody");

            // Open chains cut flip side
            bool isOpenCut =
                (profileMode->currentText() == "OpenChains" && thinMode->currentText() == "Solid" &&
                 isExtrude && operation->currentText() == "Cut");
            show(flipSide, isOpenCut);

            // Start condition (only for Extrude; Revolve start is sketch plane)
            show(startMode, isExtrude);
            auto sMode = startMode->currentText();
            show(startOffset, isExtrude && sMode == "Offset");
            show(startReference, isExtrude && (sMode == "Surface" || sMode == "Vertex"));

            // Extent / Direction 1
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

            // Direction 1 Draft (only for Extrude)
            show(draftEnable, isExtrude);
            show(draftAngle, isExtrude && draftEnable->isChecked());
            show(draftOutward, isExtrude && draftEnable->isChecked());

            // Direction 2 (inactive if Midplane)
            bool midplane1 = (isExtrude && ext1 == "Midplane") ||
                             (isRevolve && revolveExtent->currentText() == "Midplane");
            show(direction2Enable, !midplane1);
            bool dir2On = !midplane1 && direction2Enable->isChecked();
            if (isRevolve) {
                show(extent2, false);
                show(depth2, false);
                show(reverseDirection2, false);
                show(dir2Reference, false);
                show(draft2Enable, false);
                show(draft2Angle, false);
                show(draft2Outward, false);
                show(angle2, dir2On);
            } else {
                show(angle2, false);
                show(extent2, dir2On);
                auto ext2 = extent2->currentText();
                show(depth2, dir2On && ext2 == "Blind");
                show(reverseDirection2, dir2On);
                bool dir2RefNeeded = dir2On && (ext2 == "UpToVertex" || ext2 == "UpToSurface" ||
                                                ext2 == "OffsetFromSurface" || ext2 == "UpToBody");
                show(dir2Reference, dir2RefNeeded);
                show(draft2Enable, dir2On);
                show(draft2Angle, dir2On && draft2Enable->isChecked());
                show(draft2Outward, dir2On && draft2Enable->isChecked());
            }

            // Thin mode
            show(thinMode, true);
            auto tm = thinMode->currentText();
            show(thickness1, tm != "Solid");
            show(thickness2, tm == "TwoDirection");
            show(thinReverse, tm == "OneDirection");

            // Revolve specific
            show(revolveAxis, isRevolve);
            show(revolveExtent, isRevolve);
        }
    } else {
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
        // Hide all CM5 widgets
        show(profileMode, false);
        show(flipSide, false);
        show(startMode, false);
        show(startOffset, false);
        show(startReference, false);
        show(reverseDirection, false);
        show(surfaceOffset, false);
        show(offsetReverse, false);
        show(dir1Reference, false);
        show(draftEnable, false);
        show(draftAngle, false);
        show(draftOutward, false);
        show(direction2Enable, false);
        show(extent2, false);
        show(depth2, false);
        show(reverseDirection2, false);
        show(dir2Reference, false);
        show(draft2Enable, false);
        show(draft2Angle, false);
        show(draft2Outward, false);
        show(thinMode, false);
        show(thickness1, false);
        show(thickness2, false);
        show(thinReverse, false);
        show(revolveAxis, false);
        show(revolveExtent, false);
        show(angle2, false);
        show(draftNeutral, false);
        show(draftPull, false);
        show(draftFaces, false);
        show(draftStandaloneAngle, false);
        show(draftStandaloneOutward, false);
    }

    if (auto *label = qobject_cast<QLabel *>(form->labelForField(size)))
        label->setText(isHole ? "Diameter" : kind == "Chamfer" ? "Equal distance" : "Radius");
    auto units = document.value("units").toString();
    for (auto *w :
         {size, depth, depth2, spacing, thickness1, thickness2, startOffset, surfaceOffset})
        if (w)
            w->setSuffix(" " + units);
    for (auto *w : origin)
        if (w)
            w->setSuffix(" " + units);
}

void FeaturePanel::begin(QString kind, QString editId) {
    if (editing || document.empty() || client->outstanding())
        return;
    installing = true;
    candidate = {};
    candidateIntents = {};
    currentRegions = {};

    bool hasLegacyFeatures =
        !document.value("features").toArray().empty() ||
        (!document.value("model_supports").toMap().value("features").toArray().empty());
    bool hasModeling = document.contains(QStringLiteral("modeling")) &&
                       !document.value("modeling").toMap().value("intents").toArray().empty();
    bool hasSupports = document.contains(QStringLiteral("model_supports"));

    isModeling = false;
    modelingDraft = {};
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
        if (!isModeling) {
            auto definitions = document.value("features").toArray();
            for (auto f : definitions) {
                if (f.toMap().value("id").toString() == editId) {
                    draft = features::Feature::parse(f.toMap());
                    break;
                }
            }
        }
    } else {
        if (kind == "Draft") {
            isModeling = true;
        } else if (hasModeling) {
            isModeling = true;
        } else if (hasSupports && !hasLegacyFeatures) {
            isModeling = true;
        } else {
            isModeling = false;
        }
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

    for (auto bId : result.value("bodies").toArray()) {
        auto bid = bId.toString();
        auto bRef = makeRef(bid, "Body");
        addRef(dir1Reference, "Body " + bid.right(6), bRef);
        addRef(dir2Reference, "Body " + bid.right(6), bRef);
    }

    for (auto topVal : result.value("topology").toArray()) {
        auto top = topVal.toMap();
        auto token = top.value("token").toString();
        auto bId = top.value("body").toString();
        if (bId.isEmpty() && result.value("bodies").toArray().size() > 0)
            bId = result.value("bodies").toArray()[0].toString();
        if (token.startsWith("face/")) {
            auto r = makeRef(bId, "Face", {}, token);
            addRef(startReference, "Face " + token.right(8), r);
            addRef(dir1Reference, "Face " + token.right(8), r);
            addRef(dir2Reference, "Face " + token.right(8), r);
            addRef(draftNeutral, "Face " + token.right(8), r);
            addRef(draftFaces, "Face " + token.right(8), r);
        } else if (token.startsWith("vertex/")) {
            auto r = makeRef(bId, "Vertex", {}, token);
            addRef(startReference, "Vertex " + token.right(8), r);
            addRef(dir1Reference, "Vertex " + token.right(8), r);
            addRef(dir2Reference, "Vertex " + token.right(8), r);
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
    reverseDirection2->setChecked(false);
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

    if (isModeling && !modelingDraft.isEmpty()) {
        auto p = modelingDraft;
        if (p.contains(QStringLiteral("operation")))
            operation->setCurrentText(p.value("operation").toString());
        if (p.contains(QStringLiteral("reverse")))
            reverseDirection->setChecked(p.value("reverse").toBool());
        if (p.contains(QStringLiteral("flip_side")))
            flipSide->setChecked(p.value("flip_side").toBool());

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
        }

        if (p.contains(QStringLiteral("direction1"))) {
            auto d1 = p.value("direction1").toMap();
            auto mode = d1.value("mode").toString();
            extent->setCurrentText(mode);
            if (mode == "Blind" || mode == "Midplane") {
                if (kind == "Revolve")
                    angle->setValue(d1.value("value").toDouble() * 180.0 / std::numbers::pi);
                else
                    depth->setValue(d1.value("value").toDouble() / unit());
            }
            if (d1.contains(QStringLiteral("draft"))) {
                auto dr = d1.value("draft").toMap();
                double deg = dr.value("angle").toDouble() * 180.0 / std::numbers::pi;
                draftEnable->setChecked(deg > 1e-6);
                draftAngle->setValue(deg);
                draftOutward->setChecked(dr.value("outward").toBool());
            }
        }

        if (p.contains(QStringLiteral("direction2")) && !p.value("direction2").isNull()) {
            auto d2 = p.value("direction2").toMap();
            direction2Enable->setChecked(true);
            auto mode = d2.value("mode").toString();
            extent2->setCurrentText(mode);
            if (mode == "Blind") {
                if (kind == "Revolve")
                    angle2->setValue(d2.value("value").toDouble() * 180.0 / std::numbers::pi);
                else
                    depth2->setValue(d2.value("value").toDouble() / unit());
            }
            if (d2.contains(QStringLiteral("draft"))) {
                auto dr = d2.value("draft").toMap();
                double deg = dr.value("angle").toDouble() * 180.0 / std::numbers::pi;
                draft2Enable->setChecked(deg > 1e-6);
                draft2Angle->setValue(deg);
                draft2Outward->setChecked(dr.value("outward").toBool());
            }
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
    invalidate();
    updateManipulator();
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

QCborMap FeaturePanel::definitionModeling() const {
    double u = unit();
    auto kind = draft.kind;
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("id"), draft.id);
    m.insert(QStringLiteral("kind"), kind);

    if (kind == "Draft") {
        // target body
        auto targetData = target->currentData();
        QCborMap targetMap;
        if (targetData.canConvert<QCborMap>()) {
            targetMap = targetData.value<QCborMap>();
        } else if (!targetData.toString().isEmpty()) {
            targetMap.insert(QStringLiteral("source"),
                             makeRef(targetData.toString(), "Object").map());
            targetMap.insert(QStringLiteral("body"), targetData.toString());
        } else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("target"))) {
            targetMap = modelingDraft.value("target").toMap();
        } else {
            for (auto idVal : result.value("bodies").toArray()) {
                auto bid = idVal.toString();
                targetMap.insert(QStringLiteral("source"), makeRef(bid, "Object").map());
                targetMap.insert(QStringLiteral("body"), bid);
                break;
            }
        }
        m.insert(QStringLiteral("target"), targetMap);

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
        else if (draftFaces->currentData().canConvert<QCborMap>())
            facesArray = QCborArray{draftFaces->currentData().value<QCborMap>()};
        else if (!modelingDraft.isEmpty() && modelingDraft.contains(QStringLiteral("faces")))
            facesArray = modelingDraft.value("faces").toArray();
        m.insert(QStringLiteral("faces"), facesArray);

        m.insert(QStringLiteral("angle"), draftStandaloneAngle->value() * std::numbers::pi / 180.0);
        m.insert(QStringLiteral("outward"), draftStandaloneOutward->isChecked());
        return m;
    }

    // Extrude or Revolve
    auto op = operation->currentText();
    m.insert(QStringLiteral("operation"), op);
    if (op == "NewBody") {
        m.insert(QStringLiteral("target"), QCborValue(nullptr));
    } else {
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
        m.insert(QStringLiteral("target"), targetMap);
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
            d1.insert(QStringLiteral("body"), bodyId);
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
                    d2.insert(QStringLiteral("body"), bodyId);
                } else if (ext2 == "OffsetFromSurface") {
                    d2.insert(QStringLiteral("offset"), surfaceOffset->value() * u);
                    d2.insert(QStringLiteral("offset_reverse"), offsetReverse->isChecked());
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
        if (isModeling) {
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
    editing = false;
    validPreview = false;
    inputs->hide();
    accept->hide();
    previewButton->hide();
    cancelButton->hide();
    form->setRowVisible(repairCandidate, false);
    form->setRowVisible(applyRepairButton, false);
    emit clearManipulatorRequested();
    client->cancelPartPreview();
    emit editingChanged();
}

void FeaturePanel::updateManipulator() {
    if (!editing || draft.kind != "Extrude") {
        emit clearManipulatorRequested();
        return;
    }
    auto profileSketchId = profile->currentData().toString();
    if (profileSketchId.isEmpty()) {
        emit clearManipulatorRequested();
        return;
    }
    QVector3D orig(0, 0, 0);
    QVector3D norm(0, 0, 1);
    for (auto sVal : document.value("sketches").toArray()) {
        auto sk = sVal.toMap();
        if (sk.value("id").toString() == profileSketchId) {
            auto f = sk.value("frame").toMap();
            auto o = f.value("origin").toArray();
            auto u = f.value("u").toArray();
            auto v = f.value("v").toArray();
            if (o.size() == 3)
                orig = QVector3D(float(o[0].toDouble() * 1000.), float(o[1].toDouble() * 1000.),
                                 float(o[2].toDouble() * 1000.));
            if (u.size() == 3 && v.size() == 3) {
                QVector3D uVec(float(u[0].toDouble()), float(u[1].toDouble()),
                               float(u[2].toDouble()));
                QVector3D vVec(float(v[0].toDouble()), float(v[1].toDouble()),
                               float(v[2].toDouble()));
                norm = QVector3D::crossProduct(uVec, vVec).normalized();
            }
            break;
        }
    }
    double depthMM = depth->value() * (unit() * 1000.0);
    bool reverse = reverseDirection->isChecked();
    emit requestManipulator(orig, norm, depthMM, reverse);
}

void FeaturePanel::setDepthFromManipulator(double depthMM) {
    if (!editing || !depth)
        return;
    double val = depthMM / (unit() * 1000.0);
    if (std::abs(depth->value() - val) > 1e-4) {
        depth->setValue(val);
    }
}

void FeaturePanel::setDepthFinishedFromManipulator(double depthMM) {
    setDepthFromManipulator(depthMM);
    if (editing) {
        preview();
    }
}
} // namespace cad

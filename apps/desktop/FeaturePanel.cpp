#include "FeaturePanel.hpp"
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
    connect(accept, &QPushButton::clicked, this, &FeaturePanel::finish);
    connect(cancelButton, &QPushButton::clicked, this, &FeaturePanel::discard);
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
void FeaturePanel::fieldsForKind() {
    auto kind = draft.kind;
    bool sketch = kind == "Extrude" || kind == "Revolve",
         treatment = kind == "Fillet" || kind == "Chamfer", hole = kind == "Hole";
    auto show = [&](QWidget *w, bool yes) { form->setRowVisible(w, yes); };
    show(profile, sketch);
    show(operation, sketch || (!treatment && !hole));
    show(target, true);
    show(source, !sketch && !hole);
    show(extent, kind == "Extrude" || kind == "Revolve");
    show(distribution, kind == "CircularPattern");
    show(size, treatment || hole);
    show(depth, kind == "Extrude" || hole);
    show(angle, kind == "Revolve" || kind == "CircularPattern");
    show(spacing, kind == "LinearPattern");
    show(count, kind == "LinearPattern" || kind == "CircularPattern");
    show(through, hole);
    bool vectors = kind == "Revolve" || hole || kind == "Mirror" || kind == "CircularPattern";
    for (int i = 0; i < 3; ++i) {
        show(origin[i], vectors);
        show(axis[i], vectors || kind == "LinearPattern");
    }
    if (auto *label = qobject_cast<QLabel *>(form->labelForField(size)))
        label->setText(hole ? "Diameter" : kind == "Chamfer" ? "Equal distance" : "Radius");
    auto units = document.value("units").toString();
    for (auto *w : {size, depth, spacing})
        w->setSuffix(" " + units);
    for (auto *w : origin)
        w->setSuffix(" " + units);
}
void FeaturePanel::begin(QString kind, QString editId) {
    if (editing || document.empty() || client->outstanding())
        return;
    installing = true;
    draft = features::Feature{};
    draft.kind = kind;
    draft.name = kind;
    candidate = {};
    auto definitions = document.value("features").toArray();
    if (!editId.isEmpty())
        for (auto f : definitions)
            if (f.toMap().value("id").toString() == editId)
                draft = features::Feature::parse(f.toMap());
    kind = draft.kind;
    extent->clear();
    extent->addItems(kind == "Revolve" ? QStringList{"Forward", "Reverse"}
                                       : QStringList{"Forward", "Reverse", "Symmetric"});
    name->setText(draft.name);
    profile->clear();
    source->clear();
    target->clear();
    target->addItem("None", QString());
    for (auto s : document.value("sketches").toArray()) {
        auto m = s.toMap();
        profile->addItem(m.value("name").toString(), m.value("id").toString());
    }
    for (auto f : definitions) {
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
    operation->setCurrentText(
        (kind == "Extrude" || kind == "Revolve" || kind == "Mirror" || kind.endsWith("Pattern"))
            ? "NewBody"
            : "Cut");
    extent->setCurrentText("Forward");
    depth->setValue(10);
    size->setValue(2);
    spacing->setValue(10);
    angle->setValue(360);
    count->setValue(2);
    through->setChecked(true);
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
    if (!editId.isEmpty()) {
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
void FeaturePanel::preview() {
    try {
        candidate = definitions();
        validPreview = false;
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
        auto current = definitions();
        if (current != candidate) {
            invalidate();
            return;
        }
        validPreview = false;
        commitPending = true;
        client->projectCommand("SetFeatures", fields({{"features", current}}));
    } catch (const std::exception &e) {
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
    client->cancelPartPreview();
    emit editingChanged();
}
} // namespace cad

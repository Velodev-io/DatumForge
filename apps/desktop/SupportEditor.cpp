#include "SupportEditor.hpp"
#include "domain/Documents.hpp"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include <numbers>
namespace cad {
using sketch::fields;
namespace {
QVariant refData(const domain::GeometryReference &r) {
    return QVariant::fromValue(r.map());
}
domain::GeometryReference ref(QString object, QString role, QString entity = {}) {
    domain::GeometryReference r;
    r.object = object;
    r.role = role;
    r.entity = entity;
    return r;
}
QString label(const domain::GeometryReference &r) {
    return r.role + " " + (r.entity.isEmpty() ? r.object.right(8) : r.entity.right(8)) +
           (r.topology.isEmpty() ? QString() : " · " + r.topology.right(10));
}
} // namespace
double SupportEditor::unit() const {
    return domain::toSI(1, "length", document.value("units").toString());
}
bool SupportEditor::current() const {
    auto &s = client->state();
    return s.projectOpen && s.epoch == epoch && s.context.project == project &&
           s.activeDocument == documentId && s.revision == revision;
}
SupportEditor::SupportEditor(Client *c, QCborMap d, QWidget *parent, QString selected,
                             std::optional<model::SupportDefinition> initialDraft)
    : QDialog(parent), client(c), document(d), draft(model::editableSupports(d)),
      epoch(c->state().epoch), project(c->state().context.project),
      documentId(c->state().activeDocument), revision(c->state().revision) {
    loading = true;
    if (initialDraft) {
        initialDraft->map();
        draft = *initialDraft;
    }
    setObjectName("supportEditor");
    setWindowTitle("Datums and sketch supports");
    resize(760, 760);
    setWindowModality(Qt::WindowModal);
    auto *layout = new QVBoxLayout(this);
    auto *help =
        new QLabel("Edit the original support definitions. Preview rebuilds all descendants; Apply "
                   "records one undoable change. The accepted model stays displayed until Apply.");
    help->setWordWrap(true);
    layout->addWidget(help);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll, 1);
    inputs = new QWidget;
    form = new QFormLayout(inputs);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    scroll->setWidget(inputs);
    auto combo = [&](QString name, QString text) {
        auto *w = new QComboBox;
        w->setObjectName(name);
        form->addRow(text, w);
        connect(w, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
        return w;
    };
    kind = combo("supportKind", "Definition");
    kind->addItems({"Datum plane", "Datum axis", "Sketch support", "Convert Entities"});
    item = combo("supportItem", "Existing definition");
    mode = combo("supportMode", "Construction method");
    principal = combo("supportPrincipal", "Principal datum");
    target = combo("supportTarget", "Target sketch");
    bisector = combo("supportBisector", "Bisector");
    bisector->addItems({"Parallel planes", "Internal", "External"});
    auto spin = [&](QString name, QString text, double value, double low, double high) {
        auto *w = new QDoubleSpinBox;
        w->setObjectName(name);
        w->setDecimals(9);
        w->setRange(low, high);
        w->setValue(value);
        form->addRow(text, w);
        connect(w, &QDoubleSpinBox::valueChanged, this, [this] { invalidate(); });
        return w;
    };
    distance = spin("supportDistance", "Offset (" + d.value("units").toString() + ")", 10,
                    -1000 / unit(), 1000 / unit());
    angle = spin("supportAngle", "Angle (degrees)", 30, -360, 360);
    count = new QSpinBox;
    count->setObjectName("supportCount");
    count->setRange(1, 32);
    form->addRow("Number of planes", count);
    connect(count, &QSpinBox::valueChanged, this, [this] { invalidate(); });
    u = spin("supportU", "Surface U fraction", .5, 0, 1);
    v = spin("supportV", "Surface V fraction", .5, 0, 1);
    scale = spin("supportScale", "Uniform scale", 1, 1e-6, 1e6);
    auto units = d.value("units").toString();
    centerX = spin("supportCenterX", "Scale/rotation center X (" + units + ")", 0, -1000 / unit(),
                   1000 / unit());
    centerY = spin("supportCenterY", "Scale/rotation center Y (" + units + ")", 0, -1000 / unit(),
                   1000 / unit());
    translateX = spin("supportTranslateX", "Translation X (" + units + ")", 0, -1000 / unit(),
                      1000 / unit());
    translateY = spin("supportTranslateY", "Translation Y (" + units + ")", 0, -1000 / unit(),
                      1000 / unit());
    reverse = new QCheckBox("Reverse orientation");
    reverse->setObjectName("supportReverse");
    form->addRow(reverse);
    construction = new QCheckBox("Construction geometry");
    construction->setObjectName("supportConstruction");
    form->addRow(construction);
    for (auto *check : {reverse, construction})
        connect(check, &QCheckBox::toggled, this, [this] { invalidate(); });
    for (int i = 0; i < 3; ++i) {
        referenceRows[i] = new QWidget;
        auto *row = new QVBoxLayout(referenceRows[i]);
        row->setContentsMargins(0, 0, 0, 0);
        references[i] = new QComboBox;
        references[i]->setObjectName("supportReference" + QString::number(i));
        references[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        row->addWidget(references[i]);
        auto *sub = new QHBoxLayout;
        row->addLayout(sub);
        endpoints[i] = new QComboBox;
        endpoints[i]->setObjectName("supportEndpoint" + QString::number(i));
        endpoints[i]->addItems({"Whole curve", "Start", "End", "Parameter"});
        sub->addWidget(endpoints[i]);
        parameters[i] = new QDoubleSpinBox;
        parameters[i]->setObjectName("supportParameter" + QString::number(i));
        parameters[i]->setDecimals(9);
        parameters[i]->setRange(-1e6, 1e6);
        parameters[i]->setValue(.5);
        parameters[i]->setToolTip("Curve's native parameter; edge parameters are fractions along "
                                  "the oriented trimmed edge");
        sub->addWidget(parameters[i]);
        inspectButtons[i] = new QPushButton("Inspect exact choices");
        inspectButtons[i]->setObjectName("supportInspect" + QString::number(i));
        sub->addWidget(inspectButtons[i]);
        form->addRow("Reference " + QString::number(i + 1), referenceRows[i]);
        connect(inspectButtons[i], &QPushButton::clicked, this, [this, i] { inspect(i); });
        connect(references[i], &QComboBox::currentIndexChanged, this, [this, i] {
            invalidate();
            if (loading)
                return;
            auto m = references[i]->currentData().value<QCborMap>();
            auto role = m.value("role").toString();
            bool curve = role == "Curve" || role == "Edge";
            endpoints[i]->setEnabled(curve);
            if (curve && ((kind->currentIndex() < 2 && roles(i).contains("Point")) ||
                          mode->currentText() == "NormalCurve"))
                endpoints[i]->setCurrentText("Start");
            else
                endpoints[i]->setCurrentIndex(0);
            inspectButtons[i]->setEnabled(role == "Face" || role == "Edge" || role == "Vertex");
        });
        connect(endpoints[i], &QComboBox::currentIndexChanged, this, [this, i] {
            parameters[i]->setEnabled(endpoints[i]->currentText() == "Parameter");
            invalidate();
        });
        connect(parameters[i], &QDoubleSpinBox::valueChanged, this, [this] { invalidate(); });
    }
    sources = new QListWidget;
    sources->setObjectName("supportSources");
    sources->setMaximumHeight(110);
    form->addRow("Conversion sources", sources);
    auto *sourceButtons = new QWidget;
    auto *sourceLayout = new QHBoxLayout(sourceButtons);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    addSource = new QPushButton("Add selected source");
    removeSource = new QPushButton("Remove selected source");
    sourceLayout->addWidget(addSource);
    sourceLayout->addWidget(removeSource);
    form->addRow(sourceButtons);
    connect(addSource, &QPushButton::clicked, this, [this] {
        try {
            domain::check(sources->count() < 32, "ResourceLimit",
                          "Select at most 32 conversion sources");
            auto r = reference(0);
            auto *entry = new QListWidgetItem(references[0]->currentText(), sources);
            entry->setData(Qt::UserRole, refData(r));
            invalidate();
        } catch (const std::exception &e) {
            status->setText(e.what());
        }
    });
    connect(removeSource, &QPushButton::clicked, this, [this] {
        delete sources->takeItem(sources->currentRow());
        invalidate();
    });
    replacements = new QComboBox;
    replacements->setObjectName("supportReplacements");
    replacements->setMinimumContentsLength(32);
    replacements->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    form->addRow("Explicit replacement", replacements);
    connect(replacements, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (loading || index < 0 || inspectedSlot < 0)
            return;
        if (index == 0) {
            repairSelected = false;
            previewed.clear();
            refreshState();
            return;
        }
        auto r = domain::GeometryReference::parse(replacements->currentData().value<QCborMap>());
        loading = true;
        if (kind->currentIndex() == 3 && sources->count() > 0) {
            sources->item(inspectedSlot)->setData(Qt::UserRole, refData(r));
            sources->item(inspectedSlot)->setText(label(r));
            referenceChoices(0, r);
        } else
            referenceChoices(inspectedSlot, r);
        loading = false;
        previewed.clear();
        repairSelected = true;
        status->setText("Replacement selected: " + replacements->currentText() +
                        ". Preview this repaired draft before Apply.");
        refreshState();
    });
    status = new QLabel("Define a datum or associative sketch support, then Preview rebuild.");
    status->setObjectName("supportStatus");
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    layout->addWidget(status);
    summary = new QPlainTextEdit;
    summary->setObjectName("supportSummary");
    summary->setReadOnly(true);
    summary->setMaximumHeight(130);
    layout->addWidget(summary);
    auto *buttons = new QHBoxLayout;
    layout->addLayout(buttons);
    stage = new QPushButton("Stage change");
    stage->setObjectName("supportStage");
    remove = new QPushButton("Remove definition");
    remove->setObjectName("supportRemove");
    preview = new QPushButton("Preview rebuild");
    preview->setObjectName("supportPreview");
    apply = new QPushButton("Apply rebuild");
    apply->setObjectName("supportApply");
    cancel = new QPushButton("Cancel");
    cancel->setObjectName("supportCancel");
    for (auto *button : {stage, remove, preview, apply, cancel})
        buttons->addWidget(button);
    connect(stage, &QPushButton::clicked, this, &SupportEditor::stageChange);
    stage->setToolTip("Keep this change in the draft. Preview first to make generated conversion "
                      "members available as sources for another conversion.");
    connect(preview, &QPushButton::clicked, this, &SupportEditor::requestPreview);
    connect(apply, &QPushButton::clicked, this, &SupportEditor::commit);
    connect(cancel, &QPushButton::clicked, this, &SupportEditor::reject);
    connect(kind, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading)
            records();
    });
    connect(item, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading)
            load();
    });
    connect(mode, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading) {
            updateFields();
            for (int i = 0; i < 3; ++i)
                referenceChoices(i);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        try {
            auto id = item->currentData().toString();
            domain::check(!id.isEmpty(), "InvalidInput", "Select an existing definition");
            auto candidate = draft;
            if (kind->currentIndex() < 2)
                std::erase_if(candidate.datums, [&](auto &d) { return d.id == id; });
            else if (kind->currentIndex() == 2)
                std::erase_if(candidate.attachments, [&](auto &a) { return a.sketch == id; });
            else
                std::erase_if(candidate.conversions, [&](auto &c) { return c.id == id; });
            candidate.sequence();
            candidate.map();
            draft = candidate;
            records();
            invalidate();
            removalStaged = true;
            status->setText("Definition removed from the draft; Preview and Apply are required.");
        } catch (const std::exception &e) {
            status->setText(e.what());
        }
    });
    connect(client, &Client::modelSupportsPreview, this, [this](QCborMap result) {
        if (pending != "preview" || !current())
            return;
        pending.clear();
        previewed = submitted;
        previewResult = result;
        QStringList lines;
        auto m = result.value("measurement").toMap();
        if (m.value("valid").toBool())
            lines << QString("Volume: %1 m³").arg(m.value("volume_m3").toDouble(), 0, 'g', 12);
        for (auto value : result.value("datums").toArray()) {
            auto d = value.toMap();
            auto o = d.value("origin").toArray();
            lines << QString("%1 %2 · origin (%3, %4, %5) %6")
                         .arg(d.value("kind").toString(), d.value("id").toString().right(8))
                         .arg(o[0].toDouble() / unit(), 0, 'g', 8)
                         .arg(o[1].toDouble() / unit(), 0, 'g', 8)
                         .arg(o[2].toDouble() / unit(), 0, 'g', 8)
                         .arg(document.value("units").toString());
        }
        lines << QString("%1 evaluated sketches · %2 converted definitions")
                     .arg(result.value("sketches").toArray().size())
                     .arg(result.value("conversions").toArray().size());
        summary->setPlainText(lines.join('\n'));
        status->setText("Exact rebuild succeeded. Apply commits this unchanged definition; the "
                        "displayed model is still the accepted revision.");
        refreshState();
    });
    connect(client, &Client::supportReferenceResolution, this, [this](QCborMap result) {
        if (pending != "inspect" || !current() ||
            result.value("owner").toString() != inspectedOwner ||
            result.value("slot").toInteger() != inspectedSlot)
            return;
        pending.clear();
        inspection = result;
        loading = true;
        replacements->clear();
        replacements->addItem("Choose an exact replacement explicitly", QVariant());
        for (auto value : result.value("replacements").toArray()) {
            auto r = domain::GeometryReference::parse(value.toMap());
            QString text = label(r);
            for (auto detail : result.value("descriptions").toArray())
                if (detail.toMap().value("reference").toMap() == r.map()) {
                    auto m = detail.toMap();
                    auto p = m.value("center_si").toArray();
                    if (p.size() == 3)
                        text = QString("%1 · center (%2, %3, %4) %5 · %6")
                                   .arg(m.value("geometry_type").toString())
                                   .arg(p[0].toDouble() / unit(), 0, 'g', 6)
                                   .arg(p[1].toDouble() / unit(), 0, 'g', 6)
                                   .arg(p[2].toDouble() / unit(), 0, 'g', 6)
                                   .arg(document.value("units").toString(), r.topology.right(8));
                }
            replacements->addItem(text, refData(r));
        }
        loading = false;
        status->setText("Reference state: " + result.value("state").toString() +
                        ". Choose a replacement, Preview, then Apply to repair atomically.");
        refreshState();
    });
    connect(client, &Client::projectChanged, this, [this] {
        if (submitting) {
            submitting = false;
            QDialog::accept();
        }
    });
    connect(client, &Client::changed, this, [this] {
        if (!current() && !submitting) {
            retired = true;
            status->setText("The project, revision or worker session changed. Close this draft and "
                            "reopen the editor.");
        }
        if (!client->outstanding() && !pending.isEmpty() &&
            client->state().phase == Phase::Failed) {
            pending.clear();
            previewed.clear();
            submitting = false;
            status->setText(client->state().message);
        }
        if (cancelPending && !client->outstanding()) {
            cancelPending = false;
            client->cancelPartPreview();
            QDialog::reject();
            return;
        }
        refreshState();
    });
    loading = false;
    for (auto &datum : draft.datums)
        if (datum.outputs().contains(selected)) {
            loading = true;
            kind->setCurrentIndex(datum.kind == "DatumPlane" ? 0 : 1);
            loading = false;
            selected = datum.id;
            break;
        }
    records(selected);
}
void SupportEditor::invalidate() {
    if (loading)
        return;
    removalStaged = false;
    previewed.clear();
    repairSelected = false;
    inspection.clear();
    inspectionModel.clear();
    if (replacements) {
        loading = true;
        replacements->clear();
        loading = false;
    }
    refreshState();
}
void SupportEditor::refreshState() {
    if (!inputs || !apply)
        return;
    bool idle = !client->outstanding() && !submitting;
    bool live = current() && !retired;
    inputs->setEnabled(idle && live);
    preview->setEnabled(idle && live);
    stage->setEnabled(idle && live);
    remove->setEnabled(idle && live && !item->currentData().toString().isEmpty());
    apply->setEnabled(idle && live && !previewed.empty());
    cancel->setEnabled(!submitting);
}
void SupportEditor::records(QString selected) {
    loading = true;
    item->clear();
    item->addItem("New definition", QString());
    if (kind->currentIndex() < 2)
        for (auto &d : draft.datums)
            if (d.kind == (kind->currentIndex() == 0 ? "DatumPlane" : "DatumAxis"))
                item->addItem(d.mode + " " + d.id.right(8), d.id);
    if (kind->currentIndex() == 2)
        for (auto &a : draft.attachments)
            item->addItem("Sketch " + a.sketch.right(8), a.sketch);
    if (kind->currentIndex() == 3)
        for (auto &c : draft.conversions)
            item->addItem("Conversion " + c.id.right(8), c.id);
    auto index = item->findData(selected);
    item->setCurrentIndex(index < 0 ? 0 : index);
    loading = false;
    load();
}
void SupportEditor::load() {
    loading = true;
    recordId = item->currentData().toString();
    if (recordId.isEmpty())
        recordId = newId();
    newSketchId = newId();
    auto k = kind->currentIndex();
    mode->clear();
    if (k == 0)
        mode->addItems({"Principal", "Offset", "ParallelPoint", "Angle", "ThreePoints", "LinePoint",
                        "TwoLines", "NormalCurve", "TangentSurface", "Midplane"});
    if (k == 1)
        mode->addItems({"Principal", "FaceAxis", "Line", "TwoPlanes", "TwoPoints", "PointNormal"});
    principal->clear();
    principal->addItems(k == 1 ? QStringList{"X", "Y", "Z"} : QStringList{"XY", "XZ", "YZ"});
    target->clear();
    target->addItem("New supported sketch", QString());
    for (auto &s : sketch::parse(draft.sketches))
        target->addItem(s.name, s.id);
    distance->setValue(10);
    angle->setValue(k < 2 ? 30 : 0);
    count->setValue(1);
    u->setValue(.5);
    v->setValue(.5);
    scale->setValue(1);
    centerX->setValue(0);
    centerY->setValue(0);
    translateX->setValue(0);
    translateY->setValue(0);
    reverse->setChecked(false);
    construction->setChecked(false);
    sources->clear();
    bisector->setCurrentIndex(0);
    std::vector<domain::GeometryReference> refs;
    for (auto &d : draft.datums)
        if (d.id == item->currentData().toString()) {
            mode->setCurrentText(d.mode);
            refs = d.references;
            auto p = d.parameters;
            principal->setCurrentText(
                p.value(k == 1 ? QString("axis") : QString("plane")).toString());
            distance->setValue(p.value("distance").toDouble() / unit());
            count->setValue(int(p.value("count").toInteger(1)));
            angle->setValue(p.value("angle").toDouble() * 180 / std::numbers::pi);
            u->setValue(p.value("u_fraction").toDouble(.5));
            v->setValue(p.value("v_fraction").toDouble(.5));
            bisector->setCurrentIndex(p.value("bisector").toString() == "Internal"   ? 1
                                      : p.value("bisector").toString() == "External" ? 2
                                                                                     : 0);
            reverse->setChecked(d.reversed);
        }
    for (auto &a : draft.attachments)
        if (k == 2 && a.sketch == item->currentData().toString()) {
            target->setCurrentIndex(target->findData(a.sketch));
            refs = {a.reference};
            distance->setValue(a.offset / unit());
            angle->setValue(a.angle * 180 / std::numbers::pi);
            reverse->setChecked(a.reversed);
        }
    for (auto &c : draft.conversions)
        if (k == 3 && c.id == item->currentData().toString()) {
            target->setCurrentIndex(target->findData(c.sketch));
            refs = c.sources;
            scale->setValue(c.scale);
            angle->setValue(c.angle * 180 / std::numbers::pi);
            centerX->setValue(c.center.x / unit());
            centerY->setValue(c.center.y / unit());
            translateX->setValue(c.translation.x / unit());
            translateY->setValue(c.translation.y / unit());
            construction->setChecked(c.construction);
            for (auto &r : c.sources) {
                auto *entry = new QListWidgetItem(label(r), sources);
                entry->setData(Qt::UserRole, refData(r));
            }
        }
    updateFields();
    for (int i = 0; i < 3; ++i)
        referenceChoices(i,
                         size_t(i) < refs.size() ? std::optional(refs[size_t(i)]) : std::nullopt);
    loading = false;
    invalidate();
}
QStringList SupportEditor::roles(int slot) const {
    auto k = kind->currentIndex();
    auto m = mode->currentText();
    QStringList plane{"Plane", "Face"}, line{"Curve", "Edge", "Axis"},
        point{"Point", "Vertex", "Curve", "Edge"};
    if (k == 2)
        return slot == 0 ? plane : QStringList{};
    if (k == 3)
        return slot == 0 ? QStringList{"Point", "Vertex", "Curve", "Edge", "Face"} : QStringList{};
    if (m == "Principal")
        return {};
    if (m == "ThreePoints")
        return point;
    if (m == "Offset" || m == "TangentSurface" || m == "FaceAxis" || m == "Line" ||
        m == "NormalCurve") {
        if (slot > 0)
            return {};
        return m == "Offset"                                ? plane
               : (m == "FaceAxis" || m == "TangentSurface") ? QStringList{"Face"}
               : m == "Line"                                ? line
                                                            : QStringList{"Curve", "Edge"};
    }
    if (slot > 1)
        return {};
    if (m == "Angle")
        return slot == 0 ? plane : line;
    if (m == "ParallelPoint")
        return slot == 0 ? plane : point;
    if (m == "LinePoint")
        return slot == 0 ? line : point;
    if (m == "TwoLines")
        return line;
    if (m == "TwoPoints")
        return point;
    if (m == "PointNormal")
        return slot == 0 ? point : plane;
    return plane;
}
void SupportEditor::updateFields() {
    auto k = kind->currentIndex();
    auto m = mode->currentText();
    form->setRowVisible(mode, k < 2);
    form->setRowVisible(principal, k < 2 && m == "Principal");
    form->setRowVisible(target, k >= 2);
    form->setRowVisible(distance, (k == 0 && m == "Offset") || k == 2);
    form->setRowVisible(count, k == 0 && m == "Offset");
    form->setRowVisible(angle, m == "Angle" || k >= 2);
    form->setRowVisible(bisector, k == 0 && m == "Midplane");
    form->setRowVisible(u, k == 0 && m == "TangentSurface");
    form->setRowVisible(v, k == 0 && m == "TangentSurface");
    for (auto *w : {scale, centerX, centerY, translateX, translateY})
        form->setRowVisible(w, k == 3);
    form->setRowVisible(reverse, k != 3);
    form->setRowVisible(construction, k == 3);
    form->setRowVisible(sources, k == 3);
    addSource->parentWidget()->setVisible(k == 3);
    for (int i = 0; i < 3; ++i)
        form->setRowVisible(referenceRows[i], !roles(i).empty());
}
void SupportEditor::referenceChoices(int slot, std::optional<domain::GeometryReference> selected) {
    bool before = loading;
    loading = true;
    auto *box = references[slot];
    if (!selected && box->currentData().canConvert<QCborMap>())
        selected = domain::GeometryReference::parse(box->currentData().value<QCborMap>());
    box->clear();
    auto allowed = roles(slot);
    auto add = [&](QString text, domain::GeometryReference r) {
        if (allowed.contains(r.role))
            box->addItem(text, refData(r));
    };
    for (auto &s : sketch::parse(draft.sketches)) {
        add(s.name + " / Plane", ref(s.id, "Plane"));
        for (auto &e : s.entities)
            if (e.kind == "Point")
                add(s.name + " / Point " + e.id.right(8), ref(s.id, "Point", e.id));
            else
                add(s.name + " / " + e.kind + " " + e.id.right(8), ref(s.id, "Curve", e.id));
    }
    for (auto &d : draft.datums)
        for (auto id : d.outputs())
            add(d.mode + " " + d.kind + " " + id.right(8),
                ref(id, d.kind == "DatumPlane" ? "Plane" : "Axis"));
    for (auto &c : draft.conversions)
        for (auto it = c.members.cbegin(); it != c.members.cend(); ++it)
            add("Converted " + it.value() + " " + it.key().right(8),
                ref(c.sketch, it.value(), it.key()));
    for (auto value : draft.features) {
        auto f = value.toMap();
        for (auto role : {"Face", "Edge", "Vertex"}) {
            auto r = ref(f.value("id").toString(), role);
            r.topology = QString(role).toLower() + "/" + QString(64, '0');
            add(f.value("name").toString() + " / " + role + " · inspect exact choices", r);
        }
    }
    if (selected && allowed.contains(selected->role)) {
        auto index = box->findData(refData(*selected));
        if (index < 0) {
            add("Current: " + label(*selected), *selected);
            index = box->count() - 1;
        }
        box->setCurrentIndex(index);
    }
    auto r = box->currentData().value<QCborMap>();
    auto role = r.value("role").toString();
    bool curve = role == "Curve" || role == "Edge";
    endpoints[slot]->setEnabled(curve);
    endpoints[slot]->setCurrentText(
        selected && !selected->endpoint.isEmpty() ? selected->endpoint
        : selected && selected->parameter         ? "Parameter"
        : curve && ((kind->currentIndex() < 2 && allowed.contains("Point")) ||
                    mode->currentText() == "NormalCurve")
            ? "Start"
            : "Whole curve");
    parameters[slot]->setValue(selected && selected->parameter ? *selected->parameter : .5);
    parameters[slot]->setEnabled(curve && endpoints[slot]->currentText() == "Parameter");
    inspectButtons[slot]->setEnabled(role == "Face" || role == "Edge" || role == "Vertex");
    loading = before;
}
domain::GeometryReference SupportEditor::reference(int slot) const {
    domain::check(references[slot]->currentIndex() >= 0, "ReferenceMissing",
                  "Choose a source for reference " + QString::number(slot + 1));
    auto r = domain::GeometryReference::parse(references[slot]->currentData().value<QCborMap>());
    r.endpoint.clear();
    r.parameter.reset();
    if (r.role == "Curve" || r.role == "Edge") {
        auto end = endpoints[slot]->currentText();
        if (end == "Start" || end == "End")
            r.endpoint = end;
        else if (end == "Parameter")
            r.parameter = parameters[slot]->value();
    }
    r.validate();
    return r;
}
model::SupportDefinition SupportEditor::candidate() const {
    if (removalStaged)
        return draft;
    auto result = draft;
    auto k = kind->currentIndex();
    auto rad = angle->value() * std::numbers::pi / 180;
    if (k < 2) {
        model::Datum d;
        d.id = recordId;
        d.kind = k == 0 ? "DatumPlane" : "DatumAxis";
        d.mode = mode->currentText();
        d.reversed = reverse->isChecked();
        for (int i = 0; i < 3; ++i)
            if (!roles(i).empty())
                d.references.push_back(reference(i));
        if (d.mode == "Principal")
            d.parameters = fields({{k == 0 ? "plane" : "axis", principal->currentText()}});
        else if (d.mode == "Offset")
            d.parameters =
                fields({{"distance", distance->value() * unit()}, {"count", count->value()}});
        else if (d.mode == "Angle")
            d.parameters = fields({{"angle", rad}});
        else if (d.mode == "TangentSurface")
            d.parameters = fields({{"u_fraction", u->value()}, {"v_fraction", v->value()}});
        else if (d.mode == "Midplane")
            d.parameters =
                fields({{"bisector",
                         bisector->currentIndex() == 0 ? QString() : bisector->currentText()}});
        std::erase_if(result.datums, [&](auto &old) { return old.id == d.id; });
        result.datums.push_back(d);
    } else {
        auto sketch = target->currentData().toString();
        if (sketch.isEmpty()) {
            cad::sketch::Sketch s;
            s.id = newSketchId;
            s.name = "Supported sketch " + s.id.right(8);
            result.sketches.append(s.map());
            sketch = s.id;
        }
        if (k == 2) {
            model::Attachment a;
            a.sketch = sketch;
            a.reference = reference(0);
            a.offset = distance->value() * unit();
            a.angle = rad;
            a.reversed = reverse->isChecked();
            std::erase_if(result.attachments, [&](auto &old) {
                return old.sketch == item->currentData().toString() || old.sketch == sketch;
            });
            result.attachments.push_back(a);
        } else {
            model::Conversion c;
            for (auto &old : result.conversions)
                if (old.id == recordId)
                    c = old;
            c.id = recordId;
            c.sketch = sketch;
            c.sources.clear();
            for (int i = 0; i < sources->count(); ++i)
                c.sources.push_back(domain::GeometryReference::parse(
                    sources->item(i)->data(Qt::UserRole).value<QCborMap>()));
            if (c.sources.empty())
                c.sources = {reference(0)};
            c.angle = rad;
            c.scale = scale->value();
            c.center = {centerX->value() * unit(), centerY->value() * unit()};
            c.translation = {translateX->value() * unit(), translateY->value() * unit()};
            c.construction = construction->isChecked();
            std::erase_if(result.conversions, [&](auto &old) { return old.id == c.id; });
            result.conversions.push_back(c);
        }
    }
    result.sequence();
    result.map();
    return result;
}
void SupportEditor::requestPreview() {
    try {
        submitted = candidate().map();
        previewed.clear();
        pending = "preview";
        client->projectCommand("PreviewModelSupports", fields({{"model", submitted}}));
        refreshState();
    } catch (const std::exception &e) {
        status->setText(e.what());
    }
}
void SupportEditor::stageChange() {
    try {
        auto staged = candidate();
        if (previewed == staged.map())
            for (auto value : previewResult.value("conversions").toArray())
                for (auto &c : staged.conversions)
                    if (c.id == value.toMap().value("id").toString()) {
                        c.members.clear();
                        auto members = value.toMap().value("members").toMap();
                        for (auto it = members.cbegin(); it != members.cend(); ++it)
                            c.members.insert(it.key().toString(), it.value().toString());
                    }
        staged.map();
        draft = staged;
        auto selected = kind->currentIndex() == 2 ? (target->currentData().toString().isEmpty()
                                                         ? newSketchId
                                                         : target->currentData().toString())
                                                  : recordId;
        records(selected);
        status->setText("Change staged in this dialog only. Preview rebuild before Apply.");
    } catch (const std::exception &e) {
        status->setText(e.what());
    }
}
void SupportEditor::inspect(int slot) {
    try {
        auto model = candidate();
        auto role = reference(slot).role;
        inspectedSlot = slot;
        inspectedOwner = kind->currentIndex() == 2 ? (target->currentData().toString().isEmpty()
                                                          ? newSketchId
                                                          : target->currentData().toString())
                                                   : recordId;
        if (kind->currentIndex() == 3) {
            inspectedSlot = sources->count() > 0 ? std::max(0, sources->currentRow()) : 0;
            for (auto &c : model.conversions)
                if (c.id == recordId)
                    role = c.sources[size_t(inspectedSlot)].role;
        }
        domain::check(role == "Face" || role == "Edge" || role == "Vertex", "InvalidInput",
                      "Choose a feature face, edge or vertex");
        inspectionModel = model.map();
        pending = "inspect";
        repairSelected = false;
        previewed.clear();
        client->projectCommand("SupportReferenceCandidates", fields({{"model", inspectionModel},
                                                                     {"owner", inspectedOwner},
                                                                     {"slot", inspectedSlot}}));
        refreshState();
    } catch (const std::exception &e) {
        status->setText(e.what());
    }
}
void SupportEditor::commit() {
    try {
        auto model = candidate().map();
        domain::check(current() && !previewed.empty() && previewed == model, "StaleRevision",
                      "Preview the current unchanged draft before Apply");
        QCborMap payload = fields({{"model", model}});
        QString command = "SetModelSupports";
        if (repairSelected && !inspection.empty()) {
            payload = fields({{"model", inspectionModel},
                              {"owner", inspectedOwner},
                              {"slot", inspectedSlot},
                              {"replacement", replacements->currentData().value<QCborMap>()},
                              {"fingerprint", inspection.value("fingerprint")}});
            command = "ResolveSupportReference";
        }
        submitting = true;
        pending = "commit";
        client->projectCommand(command, payload);
        refreshState();
    } catch (const std::exception &e) {
        submitting = false;
        status->setText(e.what());
    }
}
void SupportEditor::reject() {
    if (submitting)
        return;
    if (client->outstanding()) {
        cancelPending = true;
        client->cancelExchange();
        status->setText("Canceling the exact rebuild; waiting for the worker outcome.");
        return;
    }
    client->cancelPartPreview();
    QDialog::reject();
}
} // namespace cad

#include "AssemblyPanel.hpp"
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QQuaternion>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <numbers>
namespace cad {
using sketch::fields;
AssemblyPanel::AssemblyPanel(Client *worker) : client(worker) {
    setObjectName("assemblyPanel");
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto *form = new QFormLayout(content);
    auto text = new QLabel(
        "Rigid assembly · positions in mm, rotations in degrees. Changes remain a draft until "
        "Accept. Geometry is shared by definition; each occurrence keeps its own placement.");
    text->setWordWrap(true);
    form->addRow(text);
    auto combo = [&](QString object, QString label) {
        auto *c = new QComboBox;
        c->setObjectName(object);
        form->addRow(label, c);
        return c;
    };
    definition = combo("assemblyDefinition", "Definition");
    name = new QLineEdit("Component");
    name->setObjectName("assemblyName");
    name->setMaxLength(128);
    form->addRow("Occurrence name", name);
    auto button = [&](QString object, QString label, std::function<void()> fn) {
        auto *b = new QPushButton(label);
        b->setObjectName(object);
        form->addRow(b);
        connect(b, &QPushButton::clicked, this, [this, fn] {
            try {
                fn();
            } catch (const domain::Error &e) {
                diagnostics->setText(e.code + ": " + QString::fromUtf8(e.what()));
            }
        });
        return b;
    };
    button("assemblyInsert", "Insert occurrence", [this] {
        begin();
        assembly::Occurrence o;
        o.definition = domain::id(definition->currentData().toString());
        o.name = domain::label(name->text());
        o.grounded = draft.occurrences.empty();
        draft.occurrences.push_back(o);
        updateLists();
        occurrence->setCurrentIndex(occurrence->findData(o.id));
        preview();
    });
    occurrence = combo("assemblyOccurrence", "Occurrence");
    connect(occurrence, &QComboBox::currentIndexChanged, this, [this] {
        if (!sync)
            chooseOccurrence();
    });
    for (int k = 0; k < 3; ++k) {
        position[k] = new QDoubleSpinBox;
        position[k]->setRange(-1000000, 1000000);
        position[k]->setDecimals(5);
        position[k]->setObjectName("assemblyPosition" + QString::number(k));
        form->addRow(QString("Position %1 (mm)").arg("XYZ"[k]), position[k]);
    }
    for (int k = 0; k < 3; ++k) {
        angles[k] = new QDoubleSpinBox;
        angles[k]->setRange(-180, 180);
        angles[k]->setDecimals(4);
        angles[k]->setObjectName("assemblyAngle" + QString::number(k));
        form->addRow(QString("Rotation %1 (deg)").arg("XYZ"[k]), angles[k]);
    }
    auto flag = [&](QString object, QString label) {
        auto *c = new QCheckBox(label);
        c->setObjectName(object);
        form->addRow(c);
        return c;
    };
    grounded = flag("assemblyGrounded", "Grounded (fixed placement)");
    hidden = flag("assemblyHidden", "Hidden (not drawn; still evaluated)");
    suppressed = flag("assemblySuppressed", "Suppressed (excluded)");
    unloaded = flag("assemblyUnloaded", "Unloaded (cached coarse display only)");
    button("assemblyPlace", "Preview placement / states", [this] {
        begin();
        auto id = occurrence->currentData().toString();
        for (auto &o : draft.occurrences)
            if (o.id == id) {
                for (int k = 0; k < 3; ++k)
                    o.pose.translation[k] = position[k]->value() / 1000;
                auto q = QQuaternion::fromEulerAngles(float(angles[0]->value()),
                                                      float(angles[1]->value()),
                                                      float(angles[2]->value()))
                             .normalized();
                o.pose.rotation = {q.scalar(), q.x(), q.y(), q.z()};
                double norm = 0;
                for (auto x : o.pose.rotation)
                    norm += x * x;
                for (auto &x : o.pose.rotation)
                    x /= std::sqrt(norm);
                o.pose = assembly::Pose::parse(o.pose.map());
                o.grounded = grounded->isChecked();
                o.hidden = hidden->isChecked();
                o.suppressed = suppressed->isChecked();
                o.unloaded = unloaded->isChecked();
                o.name = domain::label(name->text());
            }
        preview();
    });
    button("assemblyReplace", "Replace / relink selected definition", [this] {
        begin();
        for (auto &o : draft.occurrences)
            if (o.id == occurrence->currentData().toString())
                o.definition = domain::id(definition->currentData().toString());
        preview();
    });
    button("assemblyRemove", "Remove occurrence and its mates", [this] {
        begin();
        auto id = occurrence->currentData().toString();
        std::erase_if(draft.occurrences, [&](auto &o) { return o.id == id; });
        std::erase_if(draft.mates, [&](auto &m) {
            return m.first.occurrence == id || m.second.occurrence == id;
        });
        updateLists();
        preview();
    });
    mateList = combo("assemblyMate", "Edit mate");
    connect(mateList, &QComboBox::currentIndexChanged, this, [this] {
        if (!sync)
            chooseMate();
    });
    mateKind = combo("assemblyMateKind", "Mate type");
    mateKind->addItems(
        {"Coincident", "Concentric", "Parallel", "Perpendicular", "Distance", "Angle"});
    alignment = combo("assemblyAlignment", "Alignment");
    alignment->addItems({"Aligned", "Opposed"});
    firstOccurrence = combo("assemblyFirst", "First occurrence");
    secondOccurrence = combo("assemblySecond", "Second occurrence");
    firstReference = combo("assemblyFirstReference", "First reference");
    secondReference = combo("assemblySecondReference", "Second reference");
    auto datumOptions = [this](QComboBox *list, QComboBox *choice) {
        choice->clear();
        for (auto token :
             {"Origin", "XY Plane", "XZ Plane", "YZ Plane", "X Axis", "Y Axis", "Z Axis"}) {
            assembly::Reference r;
            r.occurrence = list->currentData().toString();
            r.token = token;
            choice->addItem(token, QVariant::fromValue(r.map()));
        }
    };
    connect(firstOccurrence, &QComboBox::currentIndexChanged, this, [=, this] {
        if (!sync)
            datumOptions(firstOccurrence, firstReference);
    });
    connect(secondOccurrence, &QComboBox::currentIndexChanged, this, [=, this] {
        if (!sync)
            datumOptions(secondOccurrence, secondReference);
    });
    button("assemblyReferences", "Load exact planar / axial choices", [this] {
        pendingReference = firstOccurrence->currentData().toString();
        readReferences();
    });
    mateValue = new QDoubleSpinBox;
    mateValue->setObjectName("assemblyMateValue");
    mateValue->setRange(-1000000, 1000000);
    mateValue->setDecimals(6);
    form->addRow("Distance (mm) / angle (deg)", mateValue);
    button("assemblyAddMate", "Add / update mate in draft", [this] {
        begin();
        assembly::Mate m;
        auto id = mateList->currentData().toString();
        for (auto &old : draft.mates)
            if (old.id == id)
                m = old;
        m.kind = mateKind->currentText();
        m.alignment = alignment->currentText();
        m.first = assembly::Reference::parse(firstReference->currentData().value<QCborMap>());
        m.second = assembly::Reference::parse(secondReference->currentData().value<QCborMap>());
        m.value = mateKind->currentText() == "Angle" ? mateValue->value() * std::numbers::pi / 180
                                                     : mateValue->value() / 1000;
        m.name = m.kind + " mate";
        m = assembly::Mate::parse(m.map());
        auto it = std::find_if(draft.mates.begin(), draft.mates.end(),
                               [&](auto &v) { return v.id == m.id; });
        if (it == draft.mates.end())
            draft.mates.push_back(m);
        else
            *it = m;
        updateLists();
        preview();
    });
    button("assemblyRemoveMate", "Remove selected mate from draft", [this] {
        begin();
        auto id = mateList->currentData().toString();
        std::erase_if(draft.mates, [&](auto &m) { return m.id == id; });
        updateLists();
        preview();
    });
    button("assemblyInterference", "Check exact interference",
           [this] { client->projectCommand("AssemblyInterference"); });
    button("assemblyPortable", "Create portable assembly copy…", [this] {
        auto path = QFileDialog::getSaveFileName(
            this, "Portable assembly copy (new filename required)", {}, "CAD project (*.cadproj)");
        if (path.isEmpty())
            return;
        if (QFileInfo(path).suffix().isEmpty())
            path += ".cadproj";
        client->portableCopy(path);
    });
    diagnostics = new QLabel;
    diagnostics->setObjectName("assemblyDiagnostics");
    diagnostics->setWordWrap(true);
    diagnostics->setTextFormat(Qt::PlainText);
    form->addRow(diagnostics);
    states = new QLabel;
    states->setObjectName("assemblyStates");
    states->setWordWrap(true);
    states->setTextFormat(Qt::PlainText);
    form->addRow(states);
    previewButton = new QPushButton("Preview / solve draft");
    previewButton->setObjectName("assemblyPreview");
    outer->addWidget(previewButton);
    connect(previewButton, &QPushButton::clicked, this, [this] {
        begin();
        preview();
    });
    acceptButton = new QPushButton("Accept assembly change");
    acceptButton->setObjectName("assemblyAccept");
    acceptButton->setEnabled(false);
    outer->addWidget(acceptButton);
    connect(acceptButton, &QPushButton::clicked, this,
            [this] { client->projectCommand("SetAssembly", fields({{"assembly", draft.map()}})); });
    cancelButton = new QPushButton("Cancel assembly draft");
    cancelButton->setObjectName("assemblyCancel");
    cancelButton->setEnabled(false);
    outer->addWidget(cancelButton);
    connect(cancelButton, &QPushButton::clicked, this, &AssemblyPanel::discard);
    connect(client, &Client::partResult, this, [this](QCborMap result) {
        if (!result.contains(QStringLiteral("assembly_result")))
            return;
        auto solution = result.value("assembly_result").toMap();
        bool valid = solution.value("valid").toBool();
        acceptButton->setEnabled(editing && valid);
        QStringList messages{QString("%1 · freedom %2 · residual %3")
                                 .arg(valid ? "Solved" : "Unresolved / conflict")
                                 .arg(solution.value("freedom").toInteger())
                                 .arg(solution.value("residual").toDouble(), 0, 'g', 5)};
        for (auto error : solution.value("diagnostics").toArray()) {
            auto d = error.toMap();
            messages << d.value("code").toString() + ": " + d.value("message").toString();
        }
        diagnostics->setText(messages.join('\n'));
        if (editing && valid)
            draft = assembly::Model::parse(solution.value("assembly").toMap());
        QStringList lines;
        for (auto row : result.value("occurrence_states").toArray()) {
            auto m = row.toMap();
            lines << m.value("name").toString() + ": " + m.value("status").toString() +
                         (m.value("source_changed").toBool() ? " · definition changed" : "");
        }
        states->setText(lines.join('\n'));
    });
    connect(client, &Client::assemblyInformation, this, [this](QCborMap result) {
        if (result.contains(QStringLiteral("interference"))) {
            QStringList lines;
            for (auto v : result.value("interference").toArray()) {
                auto m = v.toMap();
                if (m.value("status").toString() == "Overlap" && lines.empty())
                    client->select(
                        {"Component", "body/reference-box", m.value("first").toString()});
                if (m.value("status").toString() != "Disjoint")
                    lines << m.value("status").toString() + " · " +
                                 m.value("first").toString().left(8) + " / " +
                                 m.value("second").toString().left(8) +
                                 QString(" · %1 mm³").arg(m.value("volume_m3").toDouble() * 1e9);
            }
            diagnostics->setText(lines.isEmpty() ? "Exact interference: all pairs disjoint"
                                                 : lines.join('\n'));
            return;
        }
        auto resultRefs = result.value("assembly_references").toMap();
        auto id = resultRefs.value("occurrence").toString();
        auto *list =
            id == firstOccurrence->currentData().toString() ? firstReference : secondReference;
        list->clear();
        for (auto v : resultRefs.value("candidates").toArray()) {
            auto m = v.toMap();
            list->addItem(m.value("label").toString(),
                          QVariant::fromValue(m.value("reference").toMap()));
        }
        if (id == firstOccurrence->currentData().toString() &&
            id != secondOccurrence->currentData().toString()) {
            pendingReference = secondOccurrence->currentData().toString();
            QTimer::singleShot(0, this, [this] { readReferences(); });
        } else
            pendingReference.clear();
    });
    connect(client, &Client::changed, this, [this] {
        if (client->state().phase == Phase::Failed) {
            acceptButton->setEnabled(false);
            diagnostics->setText(client->state().message);
        }
    });
}
void AssemblyPanel::drag(QString id, QVector3D deltaMM) {
    if (client->outstanding())
        return;
    id = id.section('/', 0, 0);
    begin();
    for (auto &o : draft.occurrences)
        if (o.id == id) {
            if (o.grounded) {
                diagnostics->setText("Grounded component cannot be dragged");
                return;
            }
            for (int k = 0; k < 3; ++k)
                o.pose.translation[k] += deltaMM[k] / 1000;
        }
    preview();
}
void AssemblyPanel::begin() {
    if (editing)
        return;
    draft = accepted;
    editing = true;
    cancelButton->setEnabled(true);
    emit editingChanged();
}
void AssemblyPanel::preview() {
    acceptButton->setEnabled(false);
    client->projectCommand("PreviewAssembly", fields({{"assembly", draft.map()}}));
}
void AssemblyPanel::discard() {
    editing = false;
    draft = accepted;
    cancelButton->setEnabled(false);
    acceptButton->setEnabled(false);
    client->cancelPartPreview();
    client->displayAssembly();
    emit editingChanged();
}
void AssemblyPanel::readReferences() {
    if (!pendingReference.isEmpty() && !client->outstanding())
        client->projectCommand("AssemblyReferences", fields({{"occurrence", pendingReference}}));
}
void AssemblyPanel::install(QCborMap doc, QCborArray docs, bool idle) {
    bool changed = installed.value("id") != doc.value("id") ||
                   installed.value("revision") != doc.value("revision");
    if (changed) {
        installed = doc;
        accepted =
            assembly::Model::parse(doc.value("assembly").isMap() ? doc.value("assembly").toMap()
                                                                 : assembly::Model{}.map());
        editing = false;
        draft = accepted;
        cancelButton->setEnabled(false);
        acceptButton->setEnabled(false);
        updateLists();
    }
    sync = true;
    auto selected = definition->currentData();
    definition->clear();
    for (auto v : docs) {
        auto d = v.toMap();
        if (d.value("id") != doc.value("id") &&
            (d.value("type") == "Part" || d.value("type") == "Assembly"))
            definition->addItem(d.value("name").toString() + " (" + d.value("type").toString() +
                                    ")",
                                d.value("id").toString());
    }
    int found = definition->findData(selected);
    if (found >= 0)
        definition->setCurrentIndex(found);
    sync = false;
    for (auto *b : findChildren<QPushButton *>())
        if (b != cancelButton && b != acceptButton)
            b->setEnabled(idle);
    findChild<QPushButton *>("assemblyInterference")->setEnabled(idle && !editing);
    findChild<QPushButton *>("assemblyPortable")->setEnabled(idle && !editing);
    cancelButton->setEnabled(editing && idle);
    if (!idle)
        acceptButton->setEnabled(false);
}
void AssemblyPanel::updateLists() {
    sync = true;
    auto old = occurrence->currentData(), first = firstOccurrence->currentData(),
         second = secondOccurrence->currentData(), mate = mateList->currentData();
    for (auto *list : {occurrence, firstOccurrence, secondOccurrence})
        list->clear();
    for (auto &o : draft.occurrences)
        for (auto *list : {occurrence, firstOccurrence, secondOccurrence})
            list->addItem(o.name, o.id);
    for (auto pair : {std::pair{occurrence, old}, std::pair{firstOccurrence, first},
                      std::pair{secondOccurrence, second}}) {
        int i = pair.first->findData(pair.second);
        if (i >= 0)
            pair.first->setCurrentIndex(i);
    }
    if (secondOccurrence->currentIndex() == firstOccurrence->currentIndex() &&
        secondOccurrence->count() > 1)
        secondOccurrence->setCurrentIndex(1);
    mateList->clear();
    mateList->addItem("New mate", QString());
    for (auto &m : draft.mates)
        mateList->addItem(m.name, m.id);
    int found = mateList->findData(mate);
    if (found >= 0)
        mateList->setCurrentIndex(found);
    sync = false;
    for (auto pair : {std::pair{firstOccurrence, firstReference},
                      std::pair{secondOccurrence, secondReference}}) {
        pair.second->clear();
        for (auto token :
             {"Origin", "XY Plane", "XZ Plane", "YZ Plane", "X Axis", "Y Axis", "Z Axis"}) {
            assembly::Reference r;
            r.occurrence = pair.first->currentData().toString();
            r.token = token;
            pair.second->addItem(token, QVariant::fromValue(r.map()));
        }
    }
    chooseOccurrence();
}
void AssemblyPanel::chooseOccurrence() {
    for (auto &o : draft.occurrences)
        if (o.id == occurrence->currentData().toString()) {
            name->setText(o.name);
            for (int k = 0; k < 3; ++k)
                position[k]->setValue(o.pose.translation[k] * 1000);
            auto r = o.pose.rotation;
            auto e =
                QQuaternion(float(r[0]), float(r[1]), float(r[2]), float(r[3])).toEulerAngles();
            for (int k = 0; k < 3; ++k)
                angles[k]->setValue(e[k]);
            grounded->setChecked(o.grounded);
            hidden->setChecked(o.hidden);
            suppressed->setChecked(o.suppressed);
            unloaded->setChecked(o.unloaded);
        }
}
void AssemblyPanel::chooseMate() {
    for (auto &m : draft.mates)
        if (m.id == mateList->currentData().toString()) {
            mateKind->setCurrentText(m.kind);
            alignment->setCurrentText(m.alignment);
            mateValue->setValue(m.kind == "Angle" ? m.value * 180 / std::numbers::pi
                                                  : m.value * 1000);
            sync = true;
            firstOccurrence->setCurrentIndex(firstOccurrence->findData(m.first.occurrence));
            secondOccurrence->setCurrentIndex(secondOccurrence->findData(m.second.occurrence));
            sync = false;
            firstReference->clear();
            firstReference->addItem("Stored reference (reload to replace)",
                                    QVariant::fromValue(m.first.map()));
            secondReference->clear();
            secondReference->addItem("Stored reference (reload to replace)",
                                     QVariant::fromValue(m.second.map()));
        }
}
} // namespace cad

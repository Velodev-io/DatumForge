#include "SketchPanel.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QTimer>
#include <QVBoxLayout>
namespace cad {
void SketchPanel::clearInference() {
    ++inferenceGeneration;
    if (inferencePending)
        client->discardSketchPreview();
    inferencePending = false;
    inferenceBefore.clear();
    inferenceReceived.clear();
    inferenceProposed.clear();
    inferenceSelection.clear();
    reviewInference->setEnabled(false);
    view->showInferenceGuides({});
}
void SketchPanel::queueInference(const sketch::Sketch &before) {
    clearInference();
    if (!automaticRelations->isChecked() || dimensionEditing ||
        current().constraints.size() != before.constraints.size())
        return;
    QStringList targets;
    QMap<QString, QCborValue> originals;
    for (auto value : before.map().value("entities").toArray())
        originals.insert(value.toMap().value("id").toString(), value);
    for (auto value : current().map().value("entities").toArray()) {
        auto id = value.toMap().value("id").toString();
        if (originals.value(id) != value)
            targets << id;
    }
    if (targets.empty())
        return;
    auto generation = inferenceGeneration;
    QTimer::singleShot(0, this, [this, targets, generation] {
        if (generation != inferenceGeneration || !editing || dimensionEditing ||
            !automaticRelations->isChecked())
            return;
        inferenceBefore = sketch::encode(draft);
        inferenceSelection = targets;
        inferencePending = true;
        QCborArray selection;
        for (auto &id : targets)
            selection.append(id);
        double pixels = std::abs(view->local({8, 0}).x - view->local({0, 0}).x);
        auto request = sketch::fields({{"kind", "Infer"},
                                       {"sketch", current().id},
                                       {"selection", selection},
                                       {"tolerance", std::clamp(pixels, 1e-8, .01)}});
        client->projectCommand("PreviewSketches", sketch::fields({{"sketches", inferenceBefore},
                                                                  {"interaction", request}}));
    });
}
void SketchPanel::completeInference(const QCborArray &diagnostics) {
    if (sketch::encode(draft) != inferenceBefore) {
        clearInference();
        return;
    }
    bool valid = !inferenceReceived.empty();
    for (auto value : diagnostics)
        valid = valid && value.toMap().value("valid").toBool();
    inferencePending = false;
    if (!valid) {
        clearInference();
        context->setText("Automatic relations unavailable · draft retained");
        return;
    }
    QSet<QString> existing;
    for (auto &c : current().constraints)
        existing.insert(c.id);
    std::vector<sketch::Constraint> guides;
    for (auto &s : sketch::parse(inferenceReceived))
        if (s.id == current().id)
            for (auto &c : s.constraints)
                if (!existing.contains(c.id))
                    guides.push_back(c);
    inferenceProposed = inferenceReceived;
    view->showInferenceGuides(guides);
    reviewInference->setEnabled(!guides.empty());
    context->setText(
        guides.empty() ? "No safe automatic relations · draft retained"
                       : QString("%1 unconfirmed relations · Review to apply or Escape to dismiss")
                             .arg(guides.size()));
}
bool SketchPanel::proposalAvailable() const {
    for (auto &s : editing ? draft : accepted)
        if (s.id == sketchId())
            return !s.entities.empty();
    return false;
}
void SketchPanel::inferSelection() {
    reviewProposals("Infer");
}
void SketchPanel::fullyDefineSelection() {
    reviewProposals("FullyDefine");
}
void SketchPanel::reviewProposals(QString kind) {
    if (!editing || client->outstanding() || dimensionEditing || gestureBefore ||
        !lineEditBefore.empty())
        return;
    guard([&] {
        bool defining = kind == "FullyDefine";
        bool suggested = !defining && !inferenceProposed.empty();
        auto selection = suggested ? inferenceSelection : relationSelectionOrder;
        clearInference();
        auto before = current();
        auto allBefore = sketch::encode(draft);
        auto epoch = client->state().epoch;
        auto revisionBefore = client->state().revision;
        auto contextBefore = client->state().context;
        QSet<QString> originalIds;
        for (auto &c : before.constraints)
            originalIds.insert(c.id);
        QDialog dialog(view);
        dialog.setObjectName("sketchProposalEditor");
        dialog.setWindowTitle(defining ? "Fully Define sketch" : "Proposed sketch relations");
        auto *layout = new QVBoxLayout(&dialog);
        auto *form = new QFormLayout;
        layout->addLayout(form);
        auto *tolerance = new QLineEdit;
        tolerance->setObjectName("inferenceTolerance");
        double unit = domain::toSI(1, "length", property("units").toString());
        double pixels = std::abs(view->local({8, 0}).x - view->local({0, 0}).x);
        tolerance->setText(QString::number(std::clamp(pixels, 1e-8, .01) / unit, 'g', 10));
        if (!defining)
            form->addRow("Recognition tolerance (" + property("units").toString() + ")", tolerance);
        else {
            tolerance->setParent(&dialog);
            tolerance->hide();
        }
        auto *scope = new QComboBox(&dialog);
        scope->setObjectName("proposalScope");
        scope->addItems({"All", "Selected"});
        if (defining)
            form->addRow("Geometry to define", scope);
        else
            scope->hide();
        auto *relations = new QListWidget;
        relations->setObjectName("proposalRelations");
        relations->setAccessibleName("Proposed sketch relations");
        relations->setMaximumHeight(220);
        layout->addWidget(
            new QLabel("Only checked proposals are applied. Existing relations are retained."));
        layout->addWidget(relations);
        auto *status = new QLabel("Calculate proposals, then review before applying");
        status->setObjectName("proposalStatus");
        status->setWordWrap(true);
        status->setTextFormat(Qt::PlainText);
        layout->addWidget(status);
        auto *buttons = new QDialogButtonBox;
        auto *calculate = buttons->addButton("Calculate", QDialogButtonBox::ActionRole);
        auto *preview = buttons->addButton("Preview checked", QDialogButtonBox::ActionRole);
        auto *apply = buttons->addButton("Apply", QDialogButtonBox::AcceptRole);
        auto *cancel = buttons->addButton(QDialogButtonBox::Cancel);
        calculate->setObjectName("proposalCalculate");
        preview->setObjectName("proposalPreview");
        apply->setObjectName("proposalApply");
        apply->setEnabled(false);
        preview->setEnabled(false);
        apply->setDefault(false);
        calculate->setDefault(true);
        layout->addWidget(buttons);
        QScopedValueRollback<bool> reviewing(dimensionEditing, true);
        QCborArray calculated, received, valid;
        bool pending = false, generating = false, filling = false;
        auto invalidate = [&] {
            pending = false;
            received.clear();
            valid.clear();
            apply->setEnabled(false);
            client->discardSketchPreview();
            view->showSketch(before, selectedId, true);
        };
        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(tolerance, &QLineEdit::textChanged, &dialog, [&] {
            invalidate();
            calculated.clear();
            QSignalBlocker block(relations);
            relations->clear();
            preview->setEnabled(false);
        });
        connect(scope, &QComboBox::currentIndexChanged, &dialog, [&] {
            invalidate();
            calculated.clear();
            QSignalBlocker block(relations);
            relations->clear();
            preview->setEnabled(false);
        });
        connect(relations, &QListWidget::itemChanged, &dialog, [&] {
            if (!filling) {
                invalidate();
                status->setText("Preview the checked relations before applying");
            }
        });
        connect(calculate, &QPushButton::clicked, &dialog, [&] {
            invalidate();
            try {
                bool numeric;
                double value = tolerance->text().toDouble(&numeric) * unit;
                domain::check(numeric && std::isfinite(value) && value > 0 && value <= .01,
                              "InvalidInput",
                              "Recognition tolerance must be positive and at most 10 mm");
                QCborArray ids;
                for (auto &id : selection)
                    ids.append(id);
                auto interaction = defining ? sketch::fields({{"kind", "FullyDefine"},
                                                              {"sketch", before.id},
                                                              {"selection", ids},
                                                              {"scope", scope->currentText()}})
                                            : sketch::fields({{"kind", "Infer"},
                                                              {"sketch", before.id},
                                                              {"selection", ids},
                                                              {"tolerance", value}});
                generating = pending = true;
                client->projectCommand(
                    "PreviewSketches",
                    sketch::fields({{"sketches", allBefore}, {"interaction", interaction}}));
            } catch (const std::exception &error) {
                status->setText(error.what());
            }
        });
        connect(preview, &QPushButton::clicked, &dialog, [&] {
            invalidate();
            QSet<QString> checked;
            for (int i = 0; i < relations->count(); ++i)
                if (relations->item(i)->checkState() == Qt::Checked)
                    checked.insert(relations->item(i)->data(Qt::UserRole).toString());
            auto all = sketch::parse(allBefore);
            for (auto &s : all)
                if (s.id == before.id)
                    for (auto &proposal : sketch::parse(calculated))
                        if (proposal.id == before.id) {
                            for (auto &e : proposal.entities)
                                if (std::none_of(s.entities.begin(), s.entities.end(),
                                                 [&](auto &old) { return old.id == e.id; }))
                                    s.entities.push_back(e);
                            for (auto &c : proposal.constraints)
                                if (!originalIds.contains(c.id) && checked.contains(c.id))
                                    s.constraints.push_back(c);
                        }
            generating = false;
            pending = true;
            client->projectCommand("PreviewSketches",
                                   sketch::fields({{"sketches", sketch::encode(all)}}));
        });
        connect(client, &Client::sketchPreview, &dialog, [&](QCborArray result) {
            if (pending)
                received = result;
        });
        connect(client, &Client::sketchDiagnosed, &dialog, [&](QCborArray diagnostics) {
            if (!pending)
                return;
            bool accepted = !received.empty();
            QStringList messages;
            for (auto value : diagnostics) {
                auto d = value.toMap();
                accepted = accepted && d.value("valid").toBool();
                if (d.value("sketch_id").toString() == before.id || !d.value("valid").toBool())
                    messages << d.value("status").toString() + " · " +
                                    QString::number(d.value("freedom").toInteger()) +
                                    " degrees of freedom · " + d.value("message").toString();
            }
            status->setText(messages.join("\n"));
            if (accepted) {
                if (generating) {
                    calculated = received;
                    filling = true;
                    relations->clear();
                    for (auto &s : sketch::parse(calculated))
                        if (s.id == before.id)
                            for (auto &c : s.constraints)
                                if (!originalIds.contains(c.id)) {
                                    QString label = c.kind == "CurveTangent" ? "Tangent" : c.kind;
                                    label.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
                                    if (defining && c.kind == "Fixed")
                                        label = "Construction origin (required)";
                                    auto *item = new QListWidgetItem(label, relations);
                                    item->setData(Qt::UserRole, c.id);
                                    item->setData(Qt::UserRole + 1, c.kind);
                                    item->setFlags(c.kind == "Fixed"
                                                       ? item->flags() & ~Qt::ItemIsUserCheckable
                                                       : item->flags() | Qt::ItemIsUserCheckable);
                                    item->setCheckState(Qt::Checked);
                                    QStringList operands;
                                    for (auto &id : QStringList{c.a, c.b, c.third})
                                        for (int i = 0; i < int(s.entities.size()); ++i)
                                            if (s.entities[i].id == id)
                                                operands << s.entities[i].kind + " " +
                                                                QString::number(i + 1);
                                    item->setToolTip(operands.join(", "));
                                }
                    filling = false;
                }
                int checked = 0;
                for (int i = 0; i < relations->count(); ++i)
                    checked += relations->item(i)->checkState() == Qt::Checked &&
                               relations->item(i)->data(Qt::UserRole + 1).toString() != "Fixed";
                if (checked)
                    valid = received;
                else
                    status->setText("No safe, independent relations selected");
                for (auto &s : sketch::parse(received))
                    if (s.id == before.id)
                        view->showSketch(s, selectedId, true);
                apply->setEnabled(checked > 0);
            }
            pending = false;
        });
        connect(client, &Client::changed, &dialog, [&] {
            if (client->state().epoch != epoch || client->state().revision != revisionBefore ||
                client->state().context != contextBefore) {
                pending = false;
                valid.clear();
                dialog.reject();
                return;
            }
            bool idle = client->ready() && !client->outstanding();
            calculate->setEnabled(idle);
            preview->setEnabled(idle && !calculated.empty());
            if (!client->ready() || client->state().phase == Phase::Failed) {
                apply->setEnabled(false);
                status->setText(client->state().message);
            }
        });
        connect(apply, &QPushButton::clicked, &dialog, [&] {
            if (!valid.empty() && client->ready() && !client->outstanding())
                dialog.accept();
        });
        if (suggested)
            QTimer::singleShot(0, calculate, &QPushButton::click);
        bool accepted = dialog.exec() == QDialog::Accepted && !valid.empty();
        client->discardSketchPreview();
        if (accepted) {
            draft = sketch::parse(valid);
            remember(before);
        }
        rebuild();
    });
}
} // namespace cad

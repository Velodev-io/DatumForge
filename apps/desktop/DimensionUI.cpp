#include "SketchPanel.hpp"
#include "sketch/Dimensions.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
namespace cad {
void SketchPanel::dimensionSelection() {
    openDimensionEditor();
}
void SketchPanel::openDimensionEditor(QString constraintId) {
    if (!editing || client->outstanding() || dimensionEditing || !lineEditBefore.empty())
        return;
    guard([&] {
        auto before = current();
        sketch::Constraint initial;
        QStringList kinds;
        if (!constraintId.isEmpty()) {
            auto found = std::find_if(before.constraints.begin(), before.constraints.end(),
                                      [&](auto &c) { return c.id == constraintId; });
            domain::check(found != before.constraints.end() && sketch::dimensional(found->kind),
                          "InvalidInput", "Select a dimension");
            initial = *found;
            kinds << initial.kind;
        } else {
            QStringList selection = relationSelectionOrder;
            if (selection.isEmpty() && !selectedId.isEmpty())
                selection << selectedId;
            domain::check(!selection.empty() && selection.size() <= 2, "InvalidInput",
                          "Select a line, circle, arc, two points or two lines");
            initial.a = selection[0];
            initial.b = selection.size() == 2 ? selection[1] : QString();
            // Axis-role selection is canonical regardless of pointer/list order.
            if (!initial.b.isEmpty()) {
                auto &first = before.entity(initial.a), &second = before.entity(initial.b);
                if (first.kind == "Line" && first.construction &&
                    (second.kind == "Point" || (second.kind == "Line" && !second.construction)))
                    std::swap(initial.a, initial.b);
            }
            for (QString kind : {"Length", "Distance", "HorizontalDistance", "VerticalDistance",
                                 "Angle", "Radius", "Diameter", "Sweep", "CenterlineDiameter",
                                 "ArcLength", "HorizontalOrdinate", "VerticalOrdinate"})
                if (sketch::constraintReason(before, kind, initial.a, initial.b).isEmpty())
                    kinds << kind;
            if (kinds.removeAll("CenterlineDiameter")) {
                auto candidate = initial;
                candidate.kind = "CenterlineDiameter";
                bool parallel = before.entity(candidate.a).kind == "Point" ||
                                std::abs(sketch::axisDistance(before, candidate) -
                                         sketch::axisDistance(before, candidate, true)) <= 1e-10;
                if (parallel)
                    kinds.prepend("CenterlineDiameter");
                else
                    kinds.append("CenterlineDiameter");
            }
            domain::check(!kinds.empty(), "InvalidInput", "This selection has no dimension mode");
            initial.kind = kinds[0];
            if (initial.kind == "CenterlineDiameter")
                initial.reversed = sketch::axisDistance(before, initial) < 0;
            for (auto &c : before.constraints)
                if (c.kind == initial.kind && c.a == initial.a && c.b == initial.b) {
                    initial = c;
                    break;
                }
        }
        QDialog dialog(view);
        dialog.setObjectName("sketchDimensionEditor");
        dialog.setWindowTitle("Sketch dimension");
        auto *layout = new QVBoxLayout(&dialog);
        auto *form = new QFormLayout;
        layout->addLayout(form);
        auto *mode = new QComboBox;
        mode->setObjectName("dimensionMode");
        for (auto kind : kinds) {
            auto label = kind == "CenterlineDiameter"   ? QString("Centerline diameter")
                         : kind == "ArcLength"          ? QString("Arc length")
                         : kind == "HorizontalOrdinate" ? QString("Horizontal ordinate")
                         : kind == "VerticalOrdinate"   ? QString("Vertical ordinate")
                                                        : kind;
            mode->addItem(label, kind);
        }
        mode->setCurrentIndex(mode->findData(initial.kind));
        form->addRow("Measure", mode);
        if (kinds.contains("HorizontalOrdinate") || kinds.contains("VerticalOrdinate"))
            form->addRow(new QLabel("Ordinate: first selected point is the datum origin."));
        auto *reference = new QCheckBox("Reference (read only measurement)");
        reference->setObjectName("dimensionReference");
        reference->setChecked(!initial.driving);
        form->addRow(reference);
        auto *expression = new QLineEdit;
        expression->setObjectName("dimensionExpression");
        form->addRow("Value / expression", expression);
        auto *px = new QLineEdit, *py = new QLineEdit;
        px->setObjectName("dimensionPlacementX");
        py->setObjectName("dimensionPlacementY");
        auto units = property("units").toString();
        double unit = domain::toSI(1, "length", units);
        auto &entity = before.entity(initial.a);
        auto anchor = entity.kind == "Point" ? entity.point : before.entity(entity.a).point;
        auto placement = initial.placement.value_or(sketch::Vec2{anchor.x + .006, anchor.y + .006});
        px->setText(QString::number(placement.x / unit, 'g', 17));
        py->setText(QString::number(placement.y / unit, 'g', 17));
        form->addRow("Label X (" + units + ")", px);
        form->addRow("Label Y (" + units + ")", py);
        auto *status = new QLabel("Preview to check the dimension before applying");
        status->setWordWrap(true);
        status->setObjectName("dimensionStatus");
        layout->addWidget(status);
        auto *buttons = new QDialogButtonBox;
        layout->addWidget(buttons);
        auto *preview = buttons->addButton("Preview", QDialogButtonBox::ActionRole);
        auto *restore = buttons->addButton("Restore", QDialogButtonBox::ResetRole);
        auto *apply = buttons->addButton("Apply", QDialogButtonBox::AcceptRole);
        auto *cancel = buttons->addButton(QDialogButtonBox::Cancel);
        preview->setObjectName("dimensionPreview");
        restore->setObjectName("dimensionRestore");
        apply->setObjectName("dimensionApply");
        cancel->setObjectName("dimensionCancel");
        apply->setEnabled(false);
        apply->setDefault(false);
        preview->setDefault(true);
        auto allBefore = sketch::encode(draft);
        QCborArray received, validPreview;
        QString pending;
        auto invalidate = [&] {
            validPreview.clear();
            received.clear();
            pending.clear();
            apply->setEnabled(false);
            client->discardSketchPreview();
            view->showSketch(before, selectedId, true);
            status->setText("Preview to check the dimension before applying");
        };
        auto selection = [&] {
            auto c = initial;
            c.kind = mode->currentData().toString();
            if (c.kind == "CenterlineDiameter")
                c.reversed = sketch::axisDistance(before, c) < 0;
            for (auto &existing : before.constraints)
                if (existing.kind == c.kind && existing.a == c.a && existing.b == c.b) {
                    c = existing;
                    break;
                }
            return c;
        };
        auto resetValue = [&] {
            auto c = selection();
            c.expression.clear();
            c.driving = true;
            bool angular = c.kind == "Angle" || c.kind == "Sweep";
            auto displayUnit = angular ? QString("deg") : units;
            auto value = sketch::measureDimension(before, c) /
                         domain::toSI(1, angular ? "angle" : "length", displayUnit);
            auto existing = selection();
            expression->setText(existing.expression.isEmpty()
                                    ? QString::number(value, 'g', 17) + " " + displayUnit
                                    : existing.expression);
            expression->setEnabled(!reference->isChecked());
        };
        resetValue();
        dimensionEditing = true;
        connect(mode, &QComboBox::currentTextChanged, &dialog, [&] {
            invalidate();
            resetValue();
        });
        connect(reference, &QCheckBox::toggled, &dialog, [&] {
            invalidate();
            resetValue();
        });
        for (auto *field : {expression, px, py})
            connect(field, &QLineEdit::textChanged, &dialog, invalidate);
        connect(restore, &QPushButton::clicked, &dialog, [&] {
            invalidate();
            resetValue();
        });
        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(preview, &QPushButton::clicked, &dialog, [&] {
            invalidate();
            try {
                auto c = selection();
                bool wasDriving = c.driving;
                c.driving = !reference->isChecked();
                if (c.kind == "CenterlineDiameter" && c.driving && !wasDriving)
                    c.reversed = sketch::axisDistance(before, c) < 0;
                c.expression = c.driving ? expression->text().trimmed() : QString();
                bool number;
                c.expression.toDouble(&number);
                if (c.driving && number)
                    c.expression += c.kind == "Angle" || c.kind == "Sweep" ? " deg" : " " + units;
                bool xOK, yOK;
                c.placement = sketch::Vec2{px->text().toDouble(&xOK) * unit,
                                           py->text().toDouble(&yOK) * unit};
                domain::check(xOK && yOK, "InvalidInput", "Numeric label coordinates required");
                auto candidate = before;
                std::erase_if(candidate.constraints, [&](auto &old) { return old.id == c.id; });
                candidate.constraints.push_back(c);
                candidate.validate();
                auto sketches = sketch::parse(allBefore);
                for (auto &s : sketches)
                    if (s.id == before.id)
                        s = candidate;
                pending = c.id;
                client->projectCommand("PreviewSketches",
                                       sketch::fields({{"sketches", sketch::encode(sketches)}}));
            } catch (const std::exception &e) {
                status->setText(e.what());
            }
        });
        connect(client, &Client::sketchPreview, &dialog, [&](QCborArray a) {
            if (!pending.isEmpty())
                received = a;
        });
        connect(client, &Client::sketchDiagnosed, &dialog, [&](QCborArray diagnostics) {
            if (pending.isEmpty())
                return;
            bool valid = !received.empty();
            QStringList messages;
            for (auto d : diagnostics) {
                auto m = d.toMap();
                valid = valid && m.value("valid").toBool();
                messages << m.value("status").toString() + " " + m.value("message").toString();
            }
            status->setText(messages.join("; "));
            apply->setEnabled(valid);
            if (valid) {
                validPreview = received;
                for (auto &s : sketch::parse(received))
                    if (s.id == before.id)
                        view->showSketch(s, selectedId, true);
            }
            pending.clear();
        });
        connect(client, &Client::changed, &dialog, [&] {
            preview->setEnabled(client->ready() && !client->outstanding());
            if (!client->ready() || client->state().phase == Phase::Failed) {
                apply->setEnabled(false);
                status->setText(client->state().message);
            }
        });
        connect(apply, &QPushButton::clicked, &dialog, [&] {
            if (!validPreview.empty() && !client->outstanding() && client->ready())
                dialog.accept();
        });
        bool accepted = dialog.exec() == QDialog::Accepted && !validPreview.empty();
        client->discardSketchPreview();
        dimensionEditing = false;
        if (accepted) {
            draft = sketch::parse(validPreview);
            remember(before);
        }
        rebuild();
        if (accepted)
            previewDraft();
    });
}
} // namespace cad

#include "SketchPanel.hpp"
#include "domain/Expressions.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Dimensions.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QScopedValueRollback>
#include <QVBoxLayout>
namespace cad {
namespace {
sketch::Constraint pathDefinition(const sketch::Sketch &s, QStringList ids) {
    domain::check(!ids.empty() && ids.size() <= 64, "ResourceLimit",
                  "Select 1–64 connected path curves");
    QSet<QString> unique(ids.begin(), ids.end());
    domain::check(unique.size() == ids.size(), "ReferenceAmbiguous",
                  "A path cannot contain duplicate members");
    std::vector<std::array<sketch::Vec2, 2>> ends;
    for (auto &id : ids) {
        auto &e = s.entity(id);
        domain::check(e.kind != "Point", "InvalidInput", "Select path curves, not points");
        auto range = sketch::curveRange(e);
        ends.push_back(
            {sketch::curveValue(s, e, range.first), sketch::curveValue(s, e, range.second)});
    }
    auto same = [](auto a, auto b) { return sketch::length(a, b) <= 1e-9; };
    int first = 0, entry = 0, openEnds = 0;
    for (int i = 0; i < ids.size(); ++i)
        for (int end = 0; end < 2; ++end) {
            int degree = 0;
            for (int j = 0; j < ids.size(); ++j)
                if (i != j)
                    for (int side = 0; side < 2; ++side)
                        degree += same(ends[i][end], ends[j][side]);
            domain::check(degree <= 1, "ReferenceAmbiguous",
                          "Path branches are ambiguous; select a single chain");
            if (degree == 0 && !same(ends[i][0], ends[i][1])) {
                if (openEnds == 0) {
                    first = i;
                    entry = end;
                }
                ++openEnds;
            }
        }
    domain::check(openEnds == 0 || openEnds == 2, "ReferenceMissing",
                  "Selected path curves are disconnected");
    sketch::Constraint c;
    c.kind = "PathLength";
    QSet<int> visited;
    int current = first, start = entry;
    while (true) {
        visited.insert(current);
        c.members << ids[current];
        c.directions.push_back(start == 1);
        auto nextPoint = ends[current][1 - start];
        int next = -1, nextStart = 0;
        for (int j = 0; j < ids.size(); ++j)
            if (!visited.contains(j))
                for (int side = 0; side < 2; ++side)
                    if (same(nextPoint, ends[j][side])) {
                        domain::check(next == -1, "ReferenceAmbiguous",
                                      "Ambiguous path continuation");
                        next = j;
                        nextStart = side;
                    }
        if (next == -1) {
            domain::check(visited.size() == ids.size(), "ReferenceMissing",
                          "Path has disconnected members");
            c.closed = same(nextPoint, ends[first][entry]);
            break;
        }
        current = next;
        start = nextStart;
    }
    c.a = c.members[0];
    return c; // The worker measures the initial value; UI only orders IDs.
}
} // namespace
bool SketchPanel::pathAvailable() const {
    QStringList ids = relationSelectionOrder;
    if (ids.empty() && !selectedId.isEmpty())
        ids << selectedId;
    if (ids.empty())
        return false;
    for (auto &s : editing ? draft : accepted)
        if (s.id == sketchId()) {
            for (auto &id : ids) {
                auto entity = std::find_if(s.entities.begin(), s.entities.end(),
                                           [&](auto &e) { return e.id == id; });
                if (entity == s.entities.end() || entity->kind == "Point")
                    return false;
            }
            return true;
        }
    return false;
}
void SketchPanel::pathSelection() {
    auto ids = relationSelectionOrder;
    if (ids.empty() && !selectedId.isEmpty())
        ids << selectedId;
    openDimensionEditor({}, ids);
}
bool SketchPanel::ordinateAvailable() const {
    for (auto &s : editing ? draft : accepted)
        if (s.id == sketchId())
            for (auto &e : s.entities)
                if (e.kind == "Point" &&
                    (relationSelectionOrder.contains(e.id) || e.id == selectedId))
                    return true;
    return false;
}
void SketchPanel::ordinateSelection() {
    if (!editing || client->outstanding() || dimensionEditing || !lineEditBefore.empty())
        return;
    guard([&] {
        clearInference();
        auto sourceEpoch = client->state().epoch;
        auto sourceRevision = client->state().revision;
        auto sourceContext = client->state().context;
        auto before = current();
        QStringList targets;
        for (auto &id : relationSelectionOrder)
            if (before.entity(id).kind == "Point")
                targets << id;
        if (targets.empty() && !selectedId.isEmpty() && before.entity(selectedId).kind == "Point")
            targets << selectedId;
        domain::check(!targets.empty(), "InvalidInput", "Select ordinate target points");
        QDialog dialog(view);
        dialog.setObjectName("sketchOrdinateEditor");
        dialog.setWindowTitle("Ordinate dimensions");
        auto *layout = new QVBoxLayout(&dialog);
        auto *form = new QFormLayout;
        layout->addLayout(form);
        auto *datum = new QComboBox;
        datum->setObjectName("ordinateDatum");
        datum->addItem("Sketch origin", QString());
        for (auto &point : before.entities)
            if (point.kind == "Point")
                datum->addItem("Point " + point.id.right(8), point.id);
        auto *axis = new QComboBox;
        axis->setObjectName("ordinateAxis");
        axis->addItems({"Horizontal", "Vertical"});
        auto *reference = new QCheckBox("Reference measurements");
        reference->setObjectName("ordinateReference");
        form->addRow("Datum", datum);
        form->addRow("Axis", axis);
        form->addRow(reference);
        auto *status = new QLabel("Preview the selected points before applying");
        status->setWordWrap(true);
        layout->addWidget(status);
        auto *buttons = new QDialogButtonBox;
        auto *preview = buttons->addButton("Preview", QDialogButtonBox::ActionRole);
        auto *apply = buttons->addButton("Apply", QDialogButtonBox::AcceptRole);
        auto *cancel = buttons->addButton(QDialogButtonBox::Cancel);
        preview->setObjectName("ordinatePreview");
        apply->setObjectName("ordinateApply");
        apply->setEnabled(false);
        apply->setDefault(false);
        preview->setDefault(true);
        layout->addWidget(buttons);
        QScopedValueRollback<bool> editingDimension(dimensionEditing, true);
        QCborArray received, valid;
        bool pending = false;
        auto invalidate = [&] {
            pending = false;
            received.clear();
            valid.clear();
            apply->setEnabled(false);
            client->discardSketchPreview();
            view->showSketch(before, selectedId, true);
        };
        connect(datum, &QComboBox::currentIndexChanged, &dialog, invalidate);
        connect(axis, &QComboBox::currentIndexChanged, &dialog, invalidate);
        connect(reference, &QCheckBox::toggled, &dialog, invalidate);
        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(preview, &QPushButton::clicked, &dialog, [&] {
            invalidate();
            try {
                auto candidate = before;
                auto origin = datum->currentData().toString();
                if (origin.isEmpty()) {
                    for (auto &c : candidate.constraints)
                        if (c.kind == "Fixed" && c.value == 0 && c.second == 0 &&
                            candidate.entity(c.a).kind == "Point" &&
                            candidate.entity(c.a).construction)
                            origin = c.a;
                    if (origin.isEmpty()) {
                        origin = candidate.point({0, 0}, true);
                        sketch::Constraint fixed;
                        fixed.kind = "Fixed";
                        fixed.a = origin;
                        candidate.constraints.push_back(fixed);
                    }
                }
                double low = before.entity(targets[0]).point.y,
                       right = before.entity(targets[0]).point.x;
                for (auto &id : targets) {
                    low = std::min(low, before.entity(id).point.y);
                    right = std::max(right, before.entity(id).point.x);
                }
                int count = 0;
                for (auto &id : targets) {
                    if (id == origin)
                        continue;
                    sketch::Constraint dimension;
                    dimension.kind =
                        axis->currentIndex() == 0 ? "HorizontalOrdinate" : "VerticalOrdinate";
                    dimension.a = origin;
                    dimension.b = id;
                    dimension.driving = !reference->isChecked();
                    dimension.value = sketch::measureDimension(candidate, dimension);
                    auto point = before.entity(id).point;
                    dimension.placement = axis->currentIndex() == 0
                                              ? sketch::Vec2{point.x, low - .008 - count * .005}
                                              : sketch::Vec2{right + .008 + count * .005, point.y};
                    candidate.constraints.push_back(dimension);
                    ++count;
                }
                domain::check(count > 0, "InvalidInput",
                              "Select target points distinct from the datum");
                candidate.validate();
                auto all = draft;
                for (auto &s : all)
                    if (s.id == candidate.id)
                        s = candidate;
                pending = true;
                client->projectCommand("PreviewSketches",
                                       sketch::fields({{"sketches", sketch::encode(all)}}));
            } catch (const std::exception &error) {
                status->setText(error.what());
            }
        });
        connect(client, &Client::sketchPreview, &dialog, [&](QCborArray sketches) {
            if (pending)
                received = sketches;
        });
        connect(client, &Client::sketchDiagnosed, &dialog, [&](QCborArray diagnostics) {
            if (!pending)
                return;
            bool accepted = !received.empty();
            QStringList messages;
            for (auto item : diagnostics) {
                auto diagnostic = item.toMap();
                accepted = accepted && diagnostic.value("valid").toBool();
                messages << diagnostic.value("status").toString() + " " +
                                diagnostic.value("message").toString();
            }
            status->setText(messages.join("; "));
            apply->setEnabled(accepted);
            if (accepted) {
                valid = received;
                for (auto &s : sketch::parse(valid))
                    if (s.id == before.id)
                        view->showSketch(s, selectedId, true);
            }
            pending = false;
        });
        connect(client, &Client::changed, &dialog, [&] {
            if (client->state().epoch != sourceEpoch ||
                client->state().revision != sourceRevision ||
                client->state().context != sourceContext) {
                pending = false;
                valid.clear();
                dialog.reject();
                return;
            }
            preview->setEnabled(client->ready() && !client->outstanding());
            if (!client->ready() || client->state().phase == Phase::Failed)
                apply->setEnabled(false);
        });
        connect(apply, &QPushButton::clicked, &dialog, [&] {
            if (!valid.empty() && !client->outstanding() && client->ready())
                dialog.accept();
        });
        bool accepted = dialog.exec() == QDialog::Accepted && !valid.empty();
        client->discardSketchPreview();
        if (accepted) {
            draft = sketch::parse(valid);
            remember(before);
        }
        rebuild();
    });
}
void SketchPanel::dimensionSelection() {
    openDimensionEditor();
}
void SketchPanel::openDimensionEditor(QString constraintId, QStringList pathMembers) {
    if (!editing || client->outstanding() || dimensionEditing || !lineEditBefore.empty())
        return;
    guard([&] {
        clearInference();
        auto sourceEpoch = client->state().epoch;
        auto sourceRevision = client->state().revision;
        auto sourceContext = client->state().context;
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
        } else if (!pathMembers.empty()) {
            initial = pathDefinition(before, pathMembers);
            kinds << "PathLength";
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
        if (initial.kind == "PathLength") {
            auto *members = new QListWidget;
            members->setObjectName("dimensionPathMembers");
            members->setMaximumHeight(110);
            for (int i = 0; i < initial.members.size(); ++i)
                members->addItem(before.entity(initial.members[i]).kind + " " +
                                 initial.members[i].right(8) +
                                 (initial.directions[i] ? " ←" : " →"));
            form->addRow(initial.closed ? "Closed path" : "Ordered path", members);
        }
        if (kinds.contains("HorizontalOrdinate") || kinds.contains("VerticalOrdinate"))
            form->addRow(new QLabel("Ordinate: first selected point is the datum origin."));
        auto *reference = new QCheckBox("Reference (read only measurement)");
        reference->setObjectName("dimensionReference");
        reference->setChecked(!initial.driving);
        form->addRow(reference);
        auto *drawingMark = new QCheckBox("Mark for future drawing import");
        drawingMark->setObjectName("dimensionDrawingMark");
        drawingMark->setChecked(initial.drawing);
        form->addRow(drawingMark);
        auto *expression = new QLineEdit;
        expression->setObjectName("dimensionExpression");
        form->addRow("Value / expression", expression);
        auto *reverse = new QPushButton("Reverse sense");
        reverse->setObjectName("dimensionReverse");
        form->addRow(reverse);
        auto *increment = new QLineEdit;
        increment->setObjectName("dimensionIncrement");
        auto *increase = new QPushButton("+");
        auto *decrease = new QPushButton("−");
        increase->setObjectName("dimensionIncrease");
        decrease->setObjectName("dimensionDecrease");
        auto *steps = new QHBoxLayout;
        steps->addWidget(increment);
        steps->addWidget(decrease);
        steps->addWidget(increase);
        form->addRow("Spin increment (document units / degrees)", steps);
        auto *px = new QLineEdit, *py = new QLineEdit;
        px->setObjectName("dimensionPlacementX");
        py->setObjectName("dimensionPlacementY");
        auto units = property("units").toString();
        double unit = domain::toSI(1, "length", units);
        bool incrementSet = initial.increment.has_value();
        bool initialAngular = sketch::angularDimension(initial.kind);
        double initialUnit = initialAngular ? domain::toSI(1, "angle", "deg") : unit;
        increment->setText(
            QString::number(initial.increment.value_or(initialUnit) / initialUnit, 'g', 17));
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
        bool measuringPath = false;
        bool reverseAligned = false;
        auto invalidate = [&] {
            measuringPath = false;
            validPreview.clear();
            received.clear();
            pending.clear();
            apply->setEnabled(false);
            client->discardSketchPreview();
            view->showSketch(before, selectedId, true);
            status->setText("Preview to check the dimension before applying");
        };
        auto selection = [&] {
            if (!constraintId.isEmpty())
                return initial;
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
            bool angular = sketch::angularDimension(c.kind);
            auto displayUnit = angular ? QString("deg") : units;
            auto value = (c.kind == "PathLength" ? c.value : sketch::measureDimension(before, c)) /
                         domain::toSI(1, angular ? "angle" : "length", displayUnit);
            auto existing = selection();
            expression->setText(c.kind == "PathLength" && value == 0 ? QString()
                                : existing.expression.isEmpty()
                                    ? QString::number(value, 'g', 17) + " " + displayUnit
                                    : existing.expression);
            expression->setEnabled(!reference->isChecked());
            increase->setEnabled(!reference->isChecked());
            decrease->setEnabled(!reference->isChecked());
            reverse->setEnabled(!reference->isChecked() &&
                                (sketch::componentDimension(c.kind) || c.kind == "Length" ||
                                 c.kind == "Distance" || c.kind == "CenterlineDiameter"));
        };
        resetValue();
        dimensionEditing = true;
        connect(mode, &QComboBox::currentTextChanged, &dialog, [&] {
            reverseAligned = false;
            invalidate();
            resetValue();
        });
        connect(reference, &QCheckBox::toggled, &dialog, [&] {
            reverseAligned = false;
            invalidate();
            resetValue();
        });
        connect(drawingMark, &QCheckBox::toggled, &dialog, invalidate);
        connect(reverse, &QPushButton::clicked, &dialog, [&] {
            if (sketch::componentDimension(mode->currentData().toString()))
                expression->setText("-(" + expression->text() + ")");
            else {
                reverseAligned = !reverseAligned;
                invalidate();
            }
        });
        for (auto *field : {expression, px, py})
            connect(field, &QLineEdit::textChanged, &dialog, invalidate);
        connect(increment, &QLineEdit::textChanged, &dialog, [&] {
            incrementSet = true;
            invalidate();
        });
        auto quantity = [&](QString text) {
            bool numeric;
            text.toDouble(&numeric);
            bool angular = sketch::angularDimension(mode->currentData().toString());
            if (numeric)
                text += angular ? " deg" : " " + units;
            auto result = expression::Environment(before.names).evaluate(text);
            domain::check(result.dimension == (angular ? expression::angle : expression::length),
                          "UnitMismatch", "Dimension value/increment has the wrong unit");
            return result.value;
        };
        auto step = [&](double sign) {
            try {
                double amount = quantity(increment->text());
                domain::check(amount > 0 && amount <= 1e3, "InvalidInput",
                              "Positive bounded spin increment required");
                double value = quantity(expression->text()) + sign * amount;
                bool angular = sketch::angularDimension(mode->currentData().toString());
                incrementSet = true;
                expression->setText(
                    QString::number(value / (angular ? domain::toSI(1, "angle", "deg") : unit), 'g',
                                    17) +
                    (angular ? " deg" : " " + units));
            } catch (const std::exception &error) {
                status->setText(error.what());
            }
        };
        connect(increase, &QPushButton::clicked, &dialog, [&] { step(1); });
        connect(decrease, &QPushButton::clicked, &dialog, [&] { step(-1); });
        connect(restore, &QPushButton::clicked, &dialog, [&] {
            reverseAligned = false;
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
                c.drawing = drawingMark->isChecked();
                if (c.kind == "CenterlineDiameter" && c.driving && !wasDriving)
                    c.reversed = sketch::axisDistance(before, c) < 0;
                c.expression = c.driving ? expression->text().trimmed() : QString();
                measuringPath = c.kind == "PathLength" && c.driving && c.expression.isEmpty();
                if (measuringPath)
                    c.driving = false;
                if (incrementSet)
                    c.increment = quantity(increment->text());
                bool number;
                c.expression.toDouble(&number);
                if (c.driving && number)
                    c.expression += sketch::angularDimension(c.kind) ? " deg" : " " + units;
                bool xOK, yOK;
                c.placement = sketch::Vec2{px->text().toDouble(&xOK) * unit,
                                           py->text().toDouble(&yOK) * unit};
                domain::check(xOK && yOK, "InvalidInput", "Numeric label coordinates required");
                auto candidate = before;
                if (reverseAligned && c.driving && c.kind == "CenterlineDiameter")
                    c.reversed = !c.reversed;
                if (reverseAligned && c.driving && (c.kind == "Length" || c.kind == "Distance")) {
                    auto &operand = candidate.entity(c.a);
                    auto first = operand.kind == "Line" ? operand.a : c.a;
                    auto second = operand.kind == "Line" ? operand.b : c.b;
                    auto a = candidate.entity(first).point;
                    auto b = candidate.entity(second).point;
                    candidate.entity(second).point = {2 * a.x - b.x, 2 * a.y - b.y};
                    c.reversed = !c.reversed;
                }
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
            if (valid && measuringPath) {
                double measured = 0;
                for (auto &s : sketch::parse(received))
                    if (s.id == before.id)
                        for (auto &c : s.constraints)
                            if (c.id == pending)
                                measured = c.value;
                initial.value = measured;
                expression->setText(QString::number(measured / unit, 'g', 17) + " " + units);
                status->setText(
                    "Path measured by the worker; preview the driving value before Apply");
                apply->setEnabled(false);
                return;
            }
            if (valid && reverseAligned && !reference->isChecked() &&
                (mode->currentData().toString() == "Length" ||
                 mode->currentData().toString() == "Distance")) {
                auto c = selection();
                auto &operand = before.entity(c.a);
                auto first = operand.kind == "Line" ? operand.a : c.a;
                auto second = operand.kind == "Line" ? operand.b : c.b;
                auto a = before.entity(first).point, b = before.entity(second).point;
                for (auto &s : sketch::parse(received))
                    if (s.id == before.id) {
                        auto p = s.entity(first).point, q = s.entity(second).point;
                        if ((b.x - a.x) * (q.x - p.x) + (b.y - a.y) * (q.y - p.y) >= 0) {
                            valid = false;
                            messages << "Reverse sense is prevented by existing constraints";
                        }
                    }
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
            if (client->state().epoch != sourceEpoch ||
                client->state().revision != sourceRevision ||
                client->state().context != sourceContext) {
                pending.clear();
                validPreview.clear();
                dialog.reject();
                return;
            }
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
        if (!pathMembers.empty())
            QTimer::singleShot(0, preview, &QPushButton::click);
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

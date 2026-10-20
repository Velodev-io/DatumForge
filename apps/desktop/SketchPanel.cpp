#include "SketchPanel.hpp"
#include "domain/Documents.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStandardItemModel>
#include <QVBoxLayout>
namespace cad {
using sketch::fields;
SketchPanel::SketchPanel(Client *c, QWidget *p) : QWidget(p), client(c) {
    setObjectName("sketchWorkspace");
    auto *layout = new QHBoxLayout(this);
    auto *split = new QSplitter;
    layout->addWidget(split);
    view = new SketchView;
    split->addWidget(view);
    auto *controls = new QWidget;
    auto *form = new QFormLayout(controls);
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(controls);
    scroll->setMinimumWidth(280);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *side = new QWidget;
    side->setMaximumWidth(400);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->addWidget(scroll, 1);
    split->addWidget(side);
    split->setSizes({700, 320});
    split->setStretchFactor(0, 1);
    context = new QLabel;
    context->setWordWrap(true);
    context->setTextFormat(Qt::PlainText);
    context->setObjectName("sketchContext");
    form->addRow(context);
    sketches = new QComboBox;
    sketches->setObjectName("sketchChoice");
    form->addRow("Sketch", sketches);
    plane = new QComboBox;
    plane->setObjectName("sketchPlane");
    plane->addItems({"XY", "XZ", "YZ"});
    form->addRow("New support plane", plane);
    auto button = [&](QString text, QString id) {
        auto *b = new QPushButton(text);
        b->setObjectName(id);
        b->setAccessibleName(text);
        form->addRow(b);
        return b;
    };
    create = button("New sketch", "newSketchButton");
    edit = button("Edit selected sketch", "editSketchButton");
    tool = new QComboBox;
    tool->setObjectName("sketchTool");
    tool->addItems({"Point", "Line", "Polyline", "Rectangle", "Circle", "Arc", "Select / drag"});
    form->addRow("Tool", tool);
    auto input = [&](QString label, QString id, QString text) {
        auto *i = new QLineEdit(text);
        i->setObjectName(id);
        i->setAccessibleName(label);
        i->setMaxLength(4096);
        form->addRow(label, i);
        return i;
    };
    x = input("X (display units)", "sketchX", "0");
    y = input("Y (display units)", "sketchY", "0");
    radius = input("Radius (display units)", "sketchRadius", "2");
    start = input("Arc start (degrees)", "sketchStart", "0");
    sweep = input("Arc sweep (degrees)", "sketchSweep", "90");
    construction = new QCheckBox("Construction geometry");
    construction->setObjectName("sketchConstruction");
    form->addRow(construction);
    add = button("Add coordinate / next vertex", "addSketchCoordinate");
    entities = new QListWidget;
    entities->setObjectName("sketchEntities");
    entities->setSelectionMode(QAbstractItemView::ExtendedSelection);
    entities->setMaximumHeight(120);
    form->addRow("Entities (select 1–2)", entities);
    relation = new QComboBox;
    relation->setObjectName("sketchRelation");
    relation->addItems({"Fixed", "Coincident", "Horizontal", "Vertical", "Parallel",
                        "Perpendicular", "Tangent", "Equal", "Length", "Distance", "Angle",
                        "Radius"});
    form->addRow("Constraint", relation);
    value = input("Dimension / expression", "sketchDimension", "10 mm");
    addConstraint = button("Add constraint", "addSketchConstraint");
    constraints = new QListWidget;
    constraints->setObjectName("sketchConstraints");
    constraints->setMaximumHeight(110);
    form->addRow("Constraints", constraints);
    setDimension = button("Edit selected dimension", "editSketchDimension");
    removeConstraint = button("Delete selected constraint", "deleteSketchConstraint");
    nameExpression =
        input("Named scalar: name = expression", "sketchNameExpression", "Width = 80 mm");
    setName = button("Set named expression", "setSketchName");
    preview = button("Solve preview", "previewSketchButton");
    accept = button("Accept sketch edit", "acceptSketchButton");
    cancel = button("Cancel sketch edit (Escape)", "cancelSketchButton");
    for (auto *b : {preview, accept, cancel}) {
        form->removeWidget(b);
        sideLayout->addWidget(b);
    }
    for (auto *label : controls->findChildren<QLabel *>()) {
        label->setWordWrap(true);
        label->setMinimumWidth(0);
    }

    connect(create, &QPushButton::clicked, this, &SketchPanel::begin);
    connect(edit, &QPushButton::clicked, this, [this] {
        if (accepted.empty())
            return;
        editing = true;
        draft = accepted;
        clicks.clear();
        rebuild();
        emit editingChanged();
    });
    connect(sketches, &QComboBox::currentIndexChanged, this, [this] {
        if (!syncing) {
            selectedId.clear();
            clicks.clear();
            rebuild();
            view->fit();
        }
    });
    connect(tool, &QComboBox::currentTextChanged, this, [this] {
        clicks.clear();
        view->setTool(tool->currentText());
        updateToolFields();
    });
    updateToolFields();
    connect(add, &QPushButton::clicked, this, [this] {
        guard([&] {
            bool a, b;
            double px = x->text().toDouble(&a), py = y->text().toDouble(&b);
            domain::check(a && b, "InvalidInput", "Numeric coordinates required");
            QString units = property("units").toString();
            addAt({domain::toSI(px, "length", units), domain::toSI(py, "length", units)});
        });
    });
    connect(view, &SketchView::clicked, this, [this](double a, double b) {
        if (editing && tool->currentText() != "Select / drag")
            guard([&] { addAt({a, b}, true); });
    });
    connect(view, &SketchView::circleCreated, this, [this](double a, double b, double radius) {
        if (editing && tool->currentText() == "Circle")
            guard([&] {
                auto candidate = current();
                QString center;
                candidate.snap({a, b}, 1e-10, 0, &center);
                if (center.isEmpty())
                    center = candidate.point({a, b}, construction->isChecked());
                candidate.circle(center, radius, construction->isChecked());
                candidate.validate();
                current() = std::move(candidate);
                rebuild();
            });
    });
    connect(view, &SketchView::selected, this, [this](QString id) {
        selectedId = id;
        for (int i = 0; i < entities->count(); ++i)
            entities->item(i)->setSelected(entities->item(i)->data(Qt::UserRole).toString() == id);
        view->showSketch(current(), selectedId, editing);
    });
    connect(view, &SketchView::dragged, this, [this](QString id, double a, double b) {
        if (editing && tool->currentText() == "Select / drag")
            guard([&] {
                auto &s = current();
                s.entity(id).point = s.snap({a, b}, .00015, .001);
                rebuild();
            });
    });
    connect(entities, &QListWidget::itemSelectionChanged, this, [this] {
        if (syncing)
            return;
        updateRelations();
        if (entities->selectedItems().empty())
            return;
        selectedId = entities->selectedItems().first()->data(Qt::UserRole).toString();
        view->showSketch(current(), selectedId, editing);
    });
    connect(preview, &QPushButton::clicked, this, &SketchPanel::previewDraft);
    connect(accept, &QPushButton::clicked, this, [this] {
        if (view->sizingCircle()) {
            context->setText("Finish or cancel the circle before accepting");
            return;
        }
        if (!clicks.empty() && tool->currentText() != "Polyline") {
            context->setText("Complete the second vertex before accepting");
            return;
        }
        committing = true;
        client->projectCommand("SetSketches", fields({{"sketches", sketch::encode(draft)}}));
    });
    connect(cancel, &QPushButton::clicked, this, &SketchPanel::discard);
    connect(view, &SketchView::canceled, this, &SketchPanel::cancelInput);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &SketchPanel::cancelInput);
    connect(client, &Client::sketchPreview, this, [this](QCborArray a) {
        if (editing) {
            draft = sketch::parse(a);
            rebuild();
        }
    });
    connect(client, &Client::sketchDiagnosed, this, [this](QCborArray diagnostics) {
        if (!editing || draft.empty())
            return;
        for (auto item : diagnostics) {
            auto d = item.toMap();
            if (d.value("sketch_id").toString() != current().id)
                continue;
            QStringList conflict, redundant;
            for (auto id : d.value("conflicting").toArray())
                conflict.append(id.toString());
            for (auto id : d.value("redundant").toArray())
                redundant.append(id.toString());
            for (int i = 0; i < constraints->count(); ++i) {
                auto *row = constraints->item(i);
                auto id = current().constraints[i].id;
                row->setToolTip(id);
                if (conflict.contains(id)) {
                    row->setText("Conflict · " + row->text());
                    row->setBackground(QColor(180, 65, 65));
                } else if (redundant.contains(id)) {
                    row->setText("Redundant · " + row->text());
                    row->setBackground(QColor(150, 125, 45));
                }
            }
        }
    });
    connect(client, &Client::changed, this, [this] {
        bool idle = client->ready() && !client->outstanding();
        if (idle && client->state().phase == Phase::Failed)
            committing = false;
        for (auto *b : {add, addConstraint, setDimension, removeConstraint, setName})
            b->setEnabled(editing && idle);
        view->setEnabled(idle);
        updateRelations();
        cancel->setEnabled(editing && !committing);
        preview->setEnabled(editing && idle);
        accept->setEnabled(editing && idle);
        context->setText((editing ? "Draft · " : "Accepted · ") + client->state().job + "\n" +
                         client->state().message);
    });
    connect(relation, &QComboBox::currentTextChanged, this, [this] { updateRelations(); });
    connect(addConstraint, &QPushButton::clicked, this, [this] { guard([&] { addRelation(); }); });
    connect(removeConstraint, &QPushButton::clicked, this, [this] {
        int at = constraints->currentRow();
        if (editing && at >= 0) {
            current().constraints.erase(current().constraints.begin() + at);
            rebuild();
        }
    });
    connect(setDimension, &QPushButton::clicked, this, [this] {
        guard([&] {
            int at = constraints->currentRow();
            domain::check(editing && at >= 0, "InvalidInput",
                          "Select a dimension in an active edit");
            domain::check(QStringList{"Length", "Distance", "Angle", "Radius"}.contains(
                              current().constraints[at].kind),
                          "InvalidInput", "Select a dimensional constraint");
            current().constraints[at].expression = value->text();
            rebuild();
        });
    });
    connect(setName, &QPushButton::clicked, this, [this] {
        guard([&] {
            auto parts = nameExpression->text().split('=');
            domain::check(parts.size() == 2, "InvalidInput", "Use name = expression");
            current().names.insert(parts[0].trimmed(), parts[1].trimmed());
            rebuild();
        });
    });
    auto *enter = new QShortcut(QKeySequence(Qt::Key_Return), this);
    enter->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enter, &QShortcut::activated, this, [this] {
        if (focusWidget() == view && editing && !client->outstanding() &&
            client->state().phase == Phase::PreviewReady)
            accept->click();
    });
    setVisible(false);
}
void SketchPanel::guard(std::function<void()> f) {
    try {
        f();
    } catch (const std::exception &e) {
        context->setText(QString("Invalid draft: ") + e.what());
    }
}
sketch::Sketch &SketchPanel::current() {
    auto &s = editing ? draft : accepted;
    domain::check(!s.empty(), "InvalidInput", "Create or select a sketch");
    return s[std::clamp(sketches->currentIndex(), 0, int(s.size() - 1))];
}
void SketchPanel::install(QCborMap d, bool idle) {
    bool changed = document != d.value("id").toString() || revision != client->state().revision;
    if (changed) {
        view->cancelPointerInput();
        committing = false;
        document = d.value("id").toString();
        revision = client->state().revision;
        accepted = sketch::parse(d.value("sketches").toArray());
        draft = accepted;
        editing = false;
        clicks.clear();
        selectedId.clear();
        rebuild();
    }
    setProperty("units", d.value("units").toString());
    view->setProperty("units", d.value("units").toString());
    if (!editing) {
        QStringList labels;
        for (auto item : d.value("sketch_diagnostics").toArray()) {
            auto m = item.toMap();
            labels.append(m.value("status").toString() + " · DOF " +
                          QString::number(m.value("freedom").toInteger()) + " · " +
                          QString::number(m.value("redundant").toArray().size()) +
                          " redundant constraints");
        }
        context->setText(labels.isEmpty() ? "Create a planar sketch" : labels.join("\n"));
    }
    create->setEnabled(idle && !editing);
    edit->setEnabled(idle && !editing && !accepted.empty());
    plane->setEnabled(!editing);
    sketches->setEnabled(!editing);
    preview->setEnabled(idle && editing);
    accept->setEnabled(idle && editing);
    cancel->setEnabled(editing && !committing);
}
void SketchPanel::begin() {
    if (editing)
        return;
    draft = accepted;
    sketch::Sketch s;
    s.name = "Sketch " + QString::number(draft.size() + 1);
    if (plane->currentText() == "XZ")
        s.frame = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    if (plane->currentText() == "YZ")
        s.frame = {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    draft.push_back(s);
    editing = true;
    clicks.clear();
    rebuild();
    sketches->setCurrentIndex(int(draft.size() - 1));
    rebuild();
    emit editingChanged();
}
void SketchPanel::discard() {
    if (!editing || committing)
        return;
    draft = accepted;
    editing = false;
    view->cancelPointerInput();
    clicks.clear();
    client->discardSketchPreview();
    rebuild();
    emit editingChanged();
}
void SketchPanel::cancelInput() {
    if (!editing || committing)
        return;
    bool pending = view->cancelPointerInput() || !clicks.empty();
    clicks.clear();
    if (pending) {
        rebuild();
        context->setText("Input canceled · completed draft geometry retained. Escape again cancels "
                         "the sketch edit.");
    } else
        discard();
}
void SketchPanel::updateToolFields() {
    auto *form = qobject_cast<QFormLayout *>(x->parentWidget()->layout());
    auto t = tool->currentText();
    bool draw = t != "Select / drag";
    for (auto *field : {x, y})
        form->setRowVisible(field, draw);
    form->setRowVisible(radius, t == "Circle" || t == "Arc");
    form->setRowVisible(start, t == "Arc");
    form->setRowVisible(sweep, t == "Arc");
    form->setRowVisible(construction, draw);
    form->setRowVisible(add, draw);
    add->setText(t == "Circle" || t == "Arc" ? "Add center with numeric radius"
                                             : "Add coordinate / next vertex");
}
void SketchPanel::rebuild() {
    syncing = true;
    int at = sketches->currentIndex();
    auto &all = editing ? draft : accepted;
    sketches->clear();
    for (auto &s : all)
        sketches->addItem(s.name, s.id);
    sketches->setCurrentIndex(std::clamp(at, 0, std::max(0, int(all.size()) - 1)));
    entities->clear();
    constraints->clear();
    if (!all.empty()) {
        auto &s = current();
        for (auto &e : s.entities) {
            auto *item = new QListWidgetItem(e.kind + " · " + e.id.left(8) +
                                                 (e.construction ? " · construction" : ""),
                                             entities);
            item->setData(Qt::UserRole, e.id);
            item->setSelected(e.id == selectedId);
        }
        for (auto &c : s.constraints)
            new QListWidgetItem(
                c.kind + " · " +
                    (c.expression.isEmpty() ? QString::number(c.value) : c.expression) + " · " +
                    c.id.left(8),
                constraints);
        view->showSketch(s, selectedId, editing);
    } else
        view->clearSketch();
    view->setInputAnchor(clicks.empty() ? std::nullopt : std::optional(clicks.back()));
    syncing = false;
    updateRelations();
    for (auto *b : {add, addConstraint, setDimension, removeConstraint, setName})
        b->setEnabled(editing);
    cancel->setEnabled(editing && !committing);
    updateRelations();
    context->setText(editing ? "Draft · input coordinates in display units. Accept creates one "
                               "recoverable operation."
                             : "Accepted sketches · choose Edit or New sketch.");
}
void SketchPanel::addAt(sketch::Vec2 p, bool snapInput) {
    auto candidate = current();
    auto nextClicks = clicks;
    auto &s = candidate;
    if (snapInput)
        p = s.snap(p, .00015, .001);
    bool c = construction->isChecked();
    QString t = tool->currentText();
    auto point = [&](sketch::Vec2 q) {
        QString id;
        s.snap(q, 1e-10, 0, &id);
        return id.isEmpty() ? s.point(q, c) : id;
    };
    if (t == "Point")
        point(p);
    else if (t == "Circle" || t == "Arc") {
        bool ok;
        double r = radius->text().toDouble(&ok);
        domain::check(ok, "InvalidInput", "Numeric radius required");
        r = domain::toSI(r, "length", property("units").toString());
        domain::check(r > 1e-9, "InvalidGeometry", "Positive radius required");
        if (t == "Circle")
            s.circle(point(p), r, c);
        else {
            bool a, b;
            double startAngle = start->text().toDouble(&a), sweepAngle = sweep->text().toDouble(&b);
            domain::check(a && b, "InvalidInput", "Numeric arc angles required");
            s.arc(point(p), r, domain::toSI(startAngle, "angle", "deg"),
                  domain::toSI(sweepAngle, "angle", "deg"), c);
        }
    } else {
        nextClicks.push_back(p);
        if (nextClicks.size() >= 2) {
            auto a = nextClicks[nextClicks.size() - 2];
            domain::check(sketch::length(a, p) > 1e-9, "InvalidGeometry",
                          "Distinct vertices required");
            if (t == "Rectangle")
                s.rectangle(a, p, c);
            else
                s.line(point(a), point(p), c);
            if (t != "Polyline")
                nextClicks.clear();
        }
    }
    candidate.validate();
    current() = std::move(candidate);
    clicks = std::move(nextClicks);
    rebuild();
}
void SketchPanel::previewDraft() {
    if (view->sizingCircle()) {
        context->setText("Finish or cancel the circle before solving");
        return;
    }
    if (!clicks.empty() && tool->currentText() != "Polyline") {
        context->setText("Complete the second vertex before previewing");
        return;
    }
    client->projectCommand("PreviewSketches", fields({{"sketches", sketch::encode(draft)}}));
}
void SketchPanel::updateRelations() {
    if (!editing || draft.empty())
        return;
    auto selected = entities->selectedItems();
    QString first, second;
    if (!selected.empty())
        first = selected[0]->data(Qt::UserRole).toString();
    if (selected.size() == 2)
        second = selected[1]->data(Qt::UserRole).toString();
    auto *model = qobject_cast<QStandardItemModel *>(relation->model());
    for (int i = 0; i < relation->count(); ++i) {
        auto reason =
            selected.size() > 2
                ? QString("Select at most two entities")
                : sketch::constraintReason(current(), relation->itemText(i), first, second);
        auto *row = model->item(i);
        row->setEnabled(reason.isEmpty());
        row->setToolTip(reason);
    }
    addConstraint->setEnabled(!client->outstanding() &&
                              model->item(relation->currentIndex())->isEnabled());
}
void SketchPanel::addRelation() {
    domain::check(editing, "InvalidInput", "Begin a sketch edit first");
    auto selected = entities->selectedItems();
    domain::check(selected.size() >= 1 && selected.size() <= 2, "InvalidInput",
                  "Select one or two entities");
    sketch::Constraint c;
    c.kind = relation->currentText();
    c.a = selected[0]->data(Qt::UserRole).toString();
    if (c.kind == "Fixed") {
        auto &e = current().entity(c.a);
        c.value = e.point.x;
        c.second = e.point.y;
    }
    if (selected.size() > 1)
        c.b = selected[1]->data(Qt::UserRole).toString();
    if (QStringList{"Length", "Distance", "Angle", "Radius"}.contains(c.kind))
        c.expression = value->text();
    current().constraints.push_back(c);
    rebuild();
}
} // namespace cad

#include "SketchPanel.hpp"
#include "domain/Documents.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolButton>
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
    rootForm = form;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(controls);
    scroll->setMinimumWidth(280);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *side = new QWidget;
    propertyPane = side;
    side->setObjectName("sketchPropertyManager");
    side->setMaximumWidth(400);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    split->addWidget(side);
    split->setSizes({700, 320});
    split->setStretchFactor(0, 1);
    context = new QLabel;
    context->setWordWrap(true);
    context->setTextFormat(Qt::PlainText);
    context->setObjectName("sketchContext");
    context->setMargin(8);
    context->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    sideLayout->addWidget(context);
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
    selectionGroup = new QGroupBox("Selection");
    selectionGroup->setObjectName("selectionProperties");
    auto *selectedForm = new QFormLayout(selectionGroup);
    selectedForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    selectionTitle = new QLabel;
    selectionTitle->setObjectName("selectedGeometryTitle");
    selectedForm->addRow(selectionTitle);
    auto selectedInput = [&](QString label, QString id) {
        auto *field = new QLineEdit;
        field->setObjectName(id);
        field->setAccessibleName(label);
        field->setMaxLength(4096);
        selectedForm->addRow(label, field);
        return field;
    };
    selectedX = selectedInput("X", "selectedX");
    selectedY = selectedInput("Y", "selectedY");
    selectedRadius = selectedInput("Radius", "selectedRadius");
    selectedWidth = selectedInput("Width", "selectedWidth");
    selectedHeight = selectedInput("Height", "selectedHeight");
    selectedStart = selectedInput("Start (°)", "selectedStart");
    selectedSweep = selectedInput("Sweep (°)", "selectedSweep");
    applySelection = new QPushButton("Apply changes");
    applySelection->setObjectName("applySelectionProperties");
    removeEntity = new QPushButton("Delete shape");
    removeEntity->setObjectName("deleteSelectedEntity");
    selectedForm->addRow(applySelection);
    selectedForm->addRow(removeEntity);
    form->addRow(selectionGroup);
    connect(applySelection, &QPushButton::clicked, this,
            [this] { guard([&] { applySelected(); }); });
    connect(removeEntity, &QPushButton::clicked, this,
            [this] { guard([&] { deleteSelected(); }); });
    auto *grid = new QCheckBox("Snap to 1 mm grid");
    grid->setObjectName("sketchGrid");
    form->addRow(grid);
    connect(grid, &QCheckBox::toggled, this,
            [this](bool on) { view->setProperty("gridEnabled", on); });
    auto section = [&](QString title, QString id) {
        auto *toggle = new QToolButton;
        toggle->setObjectName(id + "Toggle");
        toggle->setText(title);
        toggle->setAccessibleName(title);
        toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toggle->setCheckable(true);
        toggle->setArrowType(Qt::RightArrow);
        toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        rootForm->addRow(toggle);
        auto *contents = new QWidget;
        contents->setObjectName(id);
        contents->setProperty("disclosureToggle", id + "Toggle");
        rootForm->addRow(contents);
        contents->hide();
        connect(toggle, &QToolButton::toggled, contents, [=](bool expanded) {
            contents->setVisible(expanded);
            toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        });
        auto *inner = new QFormLayout(contents);
        inner->setContentsMargins(4, 4, 4, 4);
        inner->setRowWrapPolicy(QFormLayout::WrapAllRows);
        return inner;
    };
    form = section("Numeric input", "numericSketchInput");
    x = input("X (display units)", "sketchX", "0");
    y = input("Y (display units)", "sketchY", "0");
    radius = input("Radius (display units)", "sketchRadius", "2");
    start = input("Arc start (degrees)", "sketchStart", "0");
    sweep = input("Arc sweep (degrees)", "sketchSweep", "90");
    construction = new QCheckBox("Construction geometry");
    construction->setObjectName("sketchConstruction");
    form->addRow(construction);
    add = button("Add coordinate / next vertex", "addSketchCoordinate");
    form = section("Relations and expressions", "advancedSketchInput");
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
    accept = button("Finish Sketch", "acceptSketchButton");
    cancel = button("Cancel Sketch", "cancelSketchButton");
    for (auto *b : {preview, accept, cancel}) {
        form->removeWidget(b);
    }
    auto *completion = new QHBoxLayout;
    completion->addWidget(accept);
    completion->addWidget(cancel);
    sideLayout->insertLayout(0, completion);
    sideLayout->addWidget(scroll, 1);
    sideLayout->addWidget(preview);
    for (auto *label : controls->findChildren<QLabel *>()) {
        label->setWordWrap(true);
        label->setMinimumWidth(0);
    }

    connect(create, &QPushButton::clicked, this, &SketchPanel::begin);
    connect(edit, &QPushButton::clicked, this, [this] { editSelected(); });
    connect(view, &SketchView::editRequested, this, &SketchPanel::editSelected);
    connect(view, &SketchView::deleteRequested, this, [this] { guard([&] { deleteSelected(); }); });
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
        emit toolChanged(tool->currentText());
    });
    updateToolFields();
    connect(add, &QPushButton::clicked, this, [this] {
        if (view->sizingCircle())
            view->cancelPointerInput();
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
                auto before = current();
                current() = std::move(candidate);
                remember(std::move(before));
                rebuild();
            });
    });
    connect(view, &SketchView::selected, this, &SketchPanel::selectEntity);
    connect(view, &SketchView::dragStarted, this, [this](QString, double x, double y) {
        if (editing) {
            gestureBefore = current();
            gestureError.clear();
            gestureOrigin = {x, y};
        }
    });
    connect(view, &SketchView::dragged, this, [this](QString id, double x, double y) {
        if (!editing || !gestureBefore)
            return;
        try {
            current() = sketch::dragEntity(*gestureBefore, id, gestureOrigin, {x, y});
            gestureError.clear();
            view->showSketch(current(), selectedId, true);
            updateSelection();
        } catch (const std::exception &e) {
            gestureError = e.what();
            context->setText(gestureError);
        }
    });
    connect(view, &SketchView::dragCanceled, this, [this] {
        if (gestureBefore) {
            current() = *gestureBefore;
            gestureBefore.reset();
            rebuild();
        }
    });
    connect(view, &SketchView::dragFinished, this, [this] {
        if (!gestureBefore)
            return;
        auto before = std::move(*gestureBefore);
        gestureBefore.reset();
        if (!gestureError.isEmpty()) {
            current() = std::move(before);
            rebuild();
            context->setText(gestureError);
            gestureError.clear();
            return;
        }
        bool changed = before.map() != current().map();
        remember(std::move(before));
        rebuild();
        if (changed)
            previewDraft();
    });
    connect(entities, &QListWidget::itemSelectionChanged, this, [this] {
        if (syncing)
            return;
        updateRelations();
        if (entities->selectedItems().empty())
            return;
        selectedId = entities->selectedItems().first()->data(Qt::UserRole).toString();
        view->showSketch(current(), selectedId, editing);
        updateSelection();
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
    auto *paneEscape = new QShortcut(QKeySequence(Qt::Key_Escape), propertyPane);
    paneEscape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(paneEscape, &QShortcut::activated, this, &SketchPanel::cancelInput);
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
        updateSelection();
        cancel->setEnabled(editing && !committing);
        preview->setEnabled(editing && idle);
        accept->setEnabled(editing && idle);
        if (client->state().phase == Phase::Failed)
            context->setText(client->state().message);
        else
            context->setText(editing ? sketches->currentText() + " · Editing"
                                     : client->state().message);
    });
    connect(relation, &QComboBox::currentTextChanged, this, [this] { updateRelations(); });
    connect(addConstraint, &QPushButton::clicked, this, [this] { guard([&] { addRelation(); }); });
    connect(removeConstraint, &QPushButton::clicked, this, [this] {
        int at = constraints->currentRow();
        if (editing && at >= 0) {
            auto before = current();
            current().constraints.erase(current().constraints.begin() + at);
            remember(std::move(before));
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
            auto before = current();
            current().constraints[at].expression = value->text();
            remember(std::move(before));
            rebuild();
        });
    });
    connect(setName, &QPushButton::clicked, this, [this] {
        guard([&] {
            auto parts = nameExpression->text().split('=');
            domain::check(parts.size() == 2, "InvalidInput", "Use name = expression");
            auto before = current();
            current().names.insert(parts[0].trimmed(), parts[1].trimmed());
            remember(std::move(before));
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
    setProperty("units", d.value("units").toString());
    propertyPane->findChild<QToolButton *>("numericSketchInputToggle")
        ->setText("Numeric input" + (d.value("units").toString().isEmpty()
                                         ? QString()
                                         : " · " + d.value("units").toString()));
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
    selectedId.clear();
    sketch::Sketch s;
    s.name = "Sketch " + QString::number(draft.size() + 1);
    if (plane->currentText() == "XZ")
        s.frame = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    if (plane->currentText() == "YZ")
        s.frame = {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    draft.push_back(s);
    editing = true;
    draftUndo.clear();
    draftRedo.clear();
    clicks.clear();
    rebuild();
    sketches->setCurrentIndex(int(draft.size() - 1));
    rebuild();
    emit editingChanged();
}
void SketchPanel::discard() {
    if (!editing || committing)
        return;
    view->cancelPointerInput();
    draft = accepted;
    selectedId.clear();
    editing = false;
    draftUndo.clear();
    draftRedo.clear();
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
    if (!pending) {
        tool->setCurrentText("Select / drag");
        selectedId.clear();
    }
    rebuild();
    context->setText(pending ? "Current input canceled" : "Select geometry to edit");
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
    auto *numericToggle = propertyPane->findChild<QToolButton *>("numericSketchInputToggle");
    numericToggle->setVisible(draw);
    x->parentWidget()->setVisible(draw && numericToggle->isChecked());
    form->setRowVisible(add, draw);
    updateSelection();
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
        int number = 0;
        for (auto &e : s.entities) {
            auto *item = new QListWidgetItem(e.kind + " " + QString::number(++number) +
                                                 (e.construction ? " · construction" : ""),
                                             entities);
            item->setToolTip(e.id);
            item->setData(Qt::UserRole, e.id);
            item->setSelected(e.id == selectedId);
        }
        for (auto &c : s.constraints)
            new QListWidgetItem(
                c.kind + " · " + (c.expression.isEmpty() ? QString::number(c.value) : c.expression),
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
    updateSelection();
    context->setText(editing ? current().name + " · Editing" : "Choose a sketch to edit");
    rootForm->setRowVisible(create, !editing);
    rootForm->setRowVisible(edit, !editing && !accepted.empty());
    rootForm->setRowVisible(plane, !editing);
    rootForm->setRowVisible(sketches, !editing);
    rootForm->setRowVisible(tool, false);
    rootForm->setRowVisible(propertyPane->findChild<QCheckBox *>("sketchGrid"), false);
}
void SketchPanel::addAt(sketch::Vec2 p, bool snapInput) {
    auto before = current();
    auto candidate = current();
    auto nextClicks = clicks;
    auto &s = candidate;
    if (snapInput)
        p = view->snapPoint(p);
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
            if (t == "Rectangle") {
                auto lines = s.rectangle(a, p, c);
                for (auto &id : lines) {
                    auto &line = s.entity(id);
                    sketch::Constraint relation;
                    relation.kind =
                        std::abs(s.entity(line.a).point.y - s.entity(line.b).point.y) < 1e-10
                            ? "Horizontal"
                            : "Vertical";
                    relation.a = id;
                    s.constraints.push_back(relation);
                }
            } else
                s.line(point(a), point(p), c);
            if (t != "Polyline")
                nextClicks.clear();
        }
    }
    candidate.validate();
    current() = std::move(candidate);
    clicks = std::move(nextClicks);
    remember(std::move(before));
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
bool SketchPanel::dimensionAvailable() const {
    for (auto &s : editing ? draft : accepted)
        for (auto &e : s.entities)
            if (e.id == selectedId)
                return e.kind == "Line" || e.kind == "Circle" || e.kind == "Arc";
    return false;
}
void SketchPanel::chooseSketch(QString id) {
    if (!editing)
        sketches->setCurrentIndex(sketches->findData(id));
}
void SketchPanel::choosePlane(QString name) {
    if (!editing)
        plane->setCurrentText(name);
}
void SketchPanel::dimensionSelection() {
    if (!editing || selectedId.isEmpty() || client->outstanding())
        return;
    guard([&] {
        auto &e = current().entity(selectedId);
        QString kind = e.kind == "Line"                          ? "Length"
                       : (e.kind == "Circle" || e.kind == "Arc") ? "Radius"
                                                                 : QString();
        domain::check(!kind.isEmpty(), "InvalidInput", "Select a line, circle or arc");
        auto unit = property("units").toString();
        double value = e.kind == "Line" ? sketch::length(current().entity(e.a).point,
                                                         current().entity(e.b).point)
                                        : e.radius;
        QString text =
            QString::number(value / domain::toSI(1, "length", unit), 'g', 10) + " " + unit;
        for (auto &c : current().constraints)
            if (c.kind == kind && c.a == e.id && !c.expression.isEmpty())
                text = c.expression;
        bool ok = false;
        text = QInputDialog::getText(this, "Dimension", kind + " (" + unit + ") or expression",
                                     QLineEdit::Normal, text, &ok);
        if (!ok)
            return;
        bool number;
        text.toDouble(&number);
        if (number)
            text += " " + unit;
        auto before = current();
        bool found = false;
        for (auto &c : current().constraints)
            if (c.kind == kind && c.a == e.id) {
                c.expression = text;
                found = true;
                break;
            }
        if (!found) {
            sketch::Constraint c;
            c.kind = kind;
            c.a = e.id;
            c.expression = text;
            current().constraints.push_back(c);
        }
        remember(std::move(before));
        rebuild();
        previewDraft();
    });
}
void SketchPanel::editSelected(QString entity) {
    if (!client->ready() || client->outstanding() || committing)
        return;
    if (!editing) {
        if (accepted.empty())
            return;
        draft = accepted;
        editing = true;
        draftUndo.clear();
        draftRedo.clear();
    }
    clicks.clear();
    tool->setCurrentText("Select / drag");
    if (!entity.isEmpty())
        selectedId = entity;
    rebuild();
    emit editingChanged();
}
void SketchPanel::setTool(QString name) {
    tool->setCurrentText(name);
    view->setFocus();
}
void SketchPanel::remember(sketch::Sketch before) {
    if (before.map() == current().map())
        return;
    if (draftUndo.size() >= 40)
        draftUndo.erase(draftUndo.begin());
    draftUndo.push_back(std::move(before));
    draftRedo.clear();
    emit editingChanged();
}
void SketchPanel::undoDraftEdit() {
    if (!editing || draftUndo.empty() || client->outstanding() || committing)
        return;
    view->cancelPointerInput();
    clicks.clear();
    draftRedo.push_back(current());
    current() = std::move(draftUndo.back());
    draftUndo.pop_back();
    client->discardSketchPreview();
    selectedId.clear();
    rebuild();
    emit editingChanged();
}
void SketchPanel::redoDraftEdit() {
    if (!editing || draftRedo.empty() || client->outstanding() || committing)
        return;
    view->cancelPointerInput();
    clicks.clear();
    draftUndo.push_back(current());
    current() = std::move(draftRedo.back());
    draftRedo.pop_back();
    client->discardSketchPreview();
    selectedId.clear();
    rebuild();
    emit editingChanged();
}
void SketchPanel::selectEntity(QString id) {
    selectedId = id;
    QSignalBlocker block(entities);
    for (int i = 0; i < entities->count(); ++i)
        entities->item(i)->setSelected(entities->item(i)->data(Qt::UserRole).toString() == id);
    if (!(editing ? draft : accepted).empty())
        view->showSketch(current(), selectedId, editing);
    updateRelations();
    updateSelection();
    emit selectionChanged();
}
void SketchPanel::updateSelection() {
    bool selected = !(editing ? draft : accepted).empty() && !selectedId.isEmpty();
    if (selected) {
        bool exists = false;
        for (auto &e : current().entities)
            if (e.id == selectedId)
                exists = true;
        if (!exists) {
            selectedId.clear();
            selected = false;
        }
    }
    if (selectionGroup->isHidden() == selected)
        selectionGroup->setVisible(selected);
    if (!selected)
        return;
    auto &s = current();
    auto &e = s.entity(selectedId);
    auto rect = sketch::rectangleOf(s, selectedId);
    bool round = e.kind == "Circle" || e.kind == "Arc";
    auto *form = qobject_cast<QFormLayout *>(selectionGroup->layout());
    auto rowVisible = [&](QWidget *field, bool on) {
        if (field->isHidden() == on)
            form->setRowVisible(field, on);
    };
    rowVisible(selectedRadius, round);
    rowVisible(selectedWidth, bool(rect));
    rowVisible(selectedHeight, bool(rect));
    rowVisible(selectedStart, e.kind == "Arc");
    rowVisible(selectedSweep, e.kind == "Arc");
    auto p = e.kind == "Point" ? e.point : s.entity(e.a).point;
    double width = 0, height = 0;
    if (rect) {
        double maxX = -1e100, maxY = -1e100;
        p = {1e100, 1e100};
        for (auto &id : rect->points) {
            auto q = s.entity(id).point;
            p.x = std::min(p.x, q.x);
            p.y = std::min(p.y, q.y);
            maxX = std::max(maxX, q.x);
            maxY = std::max(maxY, q.y);
        }
        width = maxX - p.x;
        height = maxY - p.y;
    }
    QString units = property("units").toString();
    if (units.isEmpty())
        units = "mm";
    double unit = domain::toSI(1, "length", units);
    auto text = [&](double value) { return QString::number(value / unit, 'g', 10); };
    auto dim = [&](QString id, double value) {
        for (auto &c : s.constraints)
            if ((c.kind == "Radius" || c.kind == "Length") && c.a == id)
                return c.expression.isEmpty() ? text(c.value) : c.expression;
        return text(value);
    };
    selectedX->setText(text(p.x));
    selectedY->setText(text(p.y));
    selectedRadius->setText(dim(e.id, e.radius));
    selectedStart->setText(QString::number(e.start * 180 / 3.141592653589793, 'g', 12));
    selectedSweep->setText(QString::number(e.sweep * 180 / 3.141592653589793, 'g', 12));
    selectedWidth->setText(text(width));
    selectedHeight->setText(text(height));
    if (rect)
        for (auto &id : rect->lines) {
            auto &line = s.entity(id);
            auto a = s.entity(line.a).point, b = s.entity(line.b).point;
            for (auto &c : s.constraints)
                if (c.kind == "Length" && c.a == id) {
                    if (std::abs(a.y - b.y) < 1e-10)
                        selectedWidth->setText(dim(id, width));
                    else
                        selectedHeight->setText(dim(id, height));
                }
        }
    selectionTitle->setText((rect ? "Rectangle" : e.kind) + " · " + units);
    bool idle = !client->outstanding();
    applySelection->setEnabled(editing && idle);
    removeEntity->setEnabled(editing && idle);
    removeEntity->setText(rect ? "Delete rectangle" : "Delete selection");
    for (auto *field : {selectedX, selectedY, selectedWidth, selectedHeight, selectedRadius,
                        selectedStart, selectedSweep})
        field->setReadOnly(!editing || !idle);
}
void SketchPanel::applySelected() {
    domain::check(editing && !selectedId.isEmpty(), "InvalidInput",
                  "Edit a sketch and select geometry");
    auto before = current(), candidate = before;
    auto &e = candidate.entity(selectedId);
    auto rect = sketch::rectangleOf(before, selectedId);
    QString units = property("units").toString();
    auto number = [&](QLineEdit *field) {
        bool ok;
        double value = field->text().toDouble(&ok);
        domain::check(ok, "InvalidInput", "Numeric position required");
        return domain::toSI(value, "length", units);
    };
    double x = number(selectedX), y = number(selectedY);
    auto dimension = [&](QString id, QString kind, QLineEdit *field, double old) {
        bool numeric;
        double n = field->text().toDouble(&numeric);
        if (numeric)
            n = domain::toSI(n, "length", units);
        if (numeric)
            domain::check(n > 1e-9, "InvalidGeometry", "Positive size required");
        QString expression = field->text() + (numeric ? " " + units : QString());
        for (auto &c : candidate.constraints)
            if (c.kind == kind && c.a == id) {
                c.expression = expression;
                return numeric ? n : old;
            }
        if (!numeric) {
            sketch::Constraint c;
            c.kind = kind;
            c.a = id;
            c.expression = expression;
            candidate.constraints.push_back(c);
        }
        return numeric ? n : old;
    };
    if (rect) {
        double left = 1e100, right = -1e100, bottom = 1e100, top = -1e100;
        QString horizontal, vertical;
        for (auto &id : rect->points) {
            auto p = before.entity(id).point;
            left = std::min(left, p.x);
            right = std::max(right, p.x);
            bottom = std::min(bottom, p.y);
            top = std::max(top, p.y);
        }
        for (auto &id : rect->lines) {
            auto &l = before.entity(id);
            auto a = before.entity(l.a).point, b = before.entity(l.b).point;
            if (std::abs(a.y - b.y) < 1e-10)
                horizontal = id;
            else
                vertical = id;
        }
        // Prefer an existing driving dimension, on either parallel edge.
        for (auto &c : before.constraints)
            if (c.kind == "Length")
                for (auto &id : rect->lines)
                    if (c.a == id) {
                        auto &l = before.entity(id);
                        if (std::abs(before.entity(l.a).point.y - before.entity(l.b).point.y) <
                            1e-10)
                            horizontal = id;
                        else
                            vertical = id;
                    }
        double w = dimension(horizontal, "Length", selectedWidth, right - left),
               h = dimension(vertical, "Length", selectedHeight, top - bottom);
        for (auto &id : rect->points) {
            auto p = before.entity(id).point;
            candidate.entity(id).point = {x + (p.x - left) / (right - left) * w,
                                          y + (p.y - bottom) / (top - bottom) * h};
        }
    } else if (e.kind == "Circle" || e.kind == "Arc") {
        candidate.entity(e.a).point = {x, y};
        e.radius = dimension(e.id, "Radius", selectedRadius, e.radius);
        if (e.kind == "Arc") {
            bool a, b;
            double start = selectedStart->text().toDouble(&a),
                   sweep = selectedSweep->text().toDouble(&b);
            domain::check(a && b, "InvalidInput", "Numeric arc angles required");
            e.start = domain::toSI(start, "angle", "deg");
            e.sweep = domain::toSI(sweep, "angle", "deg");
        }
    } else if (e.kind == "Point")
        e.point = {x, y};
    else {
        auto p = before.entity(e.a).point;
        candidate = sketch::dragEntity(before, e.id, p, {x, y});
    }
    // Fixed point anchors remain explicit: changing a size must not silently move them.
    for (auto &c : candidate.constraints)
        if (c.kind == "Fixed") {
            auto p = candidate.entity(c.a).point;
            domain::check(std::abs(p.x - c.value) < 1e-10 && std::abs(p.y - c.second) < 1e-10,
                          "ConstrainedGeometry",
                          "A fixed point would move. Remove or edit its Fixed relation first.");
        }
    candidate.validate();
    current() = std::move(candidate);
    remember(std::move(before));
    rebuild();
    previewDraft();
}
void SketchPanel::deleteSelected() {
    if (!editing || selectedId.isEmpty())
        return;
    view->cancelPointerInput();
    auto before = current();
    current() = sketch::eraseEntity(before, selectedId);
    selectedId.clear();
    remember(std::move(before));
    rebuild();
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
    auto before = current();
    current().constraints.push_back(c);
    remember(std::move(before));
    rebuild();
}
} // namespace cad

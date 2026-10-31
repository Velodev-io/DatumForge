#include "SketchPanel.hpp"
#include "domain/Documents.hpp"
#include "sketch/Constructions.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Dimensions.hpp"
#include "sketch/LineEditing.hpp"
#include "sketch/Slots.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLocale>
#include <QRegularExpression>
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
namespace {
QString preciseNumber(double value) {
    auto locale = QLocale::c();
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    return locale.toString(value, 'g', QLocale::FloatingPointShortest);
}
void relationOperands(const sketch::Sketch &s, const QString &kind, QString &a, QString &b,
                      QString &third) {
    if (kind == "Intersection") {
        QStringList ids{a, b, third};
        QString point;
        QStringList curves;
        for (auto id : ids)
            if (!id.isEmpty()) {
                if (s.entity(id).kind == "Point")
                    point = id;
                else
                    curves.append(id);
            }
        if (!point.isEmpty() && curves.size() == 2) {
            a = point;
            b = curves[0];
            third = curves[1];
        }
    }
    // The list is in entity order, not click order. Canonicalize point/curve
    // operands so a later-created point can be constrained to an earlier curve.
    if ((kind == "Coincident" || kind == "Midpoint") && !a.isEmpty() && !b.isEmpty() &&
        s.entity(a).kind != "Point" && s.entity(b).kind == "Point")
        std::swap(a, b);
}
bool batchRelation(QString kind) {
    return QStringList{"Horizontal", "Vertical",   "Collinear", "Parallel", "Perpendicular",
                       "Equal",      "Concentric", "Coradial",  "Merge",    "Fixed"}
        .contains(kind);
}
std::vector<sketch::Constraint> relationBatch(const sketch::Sketch &s, QString kind,
                                              const QStringList &ids) {
    std::vector<sketch::Constraint> result;
    bool single =
        kind == "Fixed" ||
        ((kind == "Horizontal" || kind == "Vertical") &&
         std::all_of(ids.begin(), ids.end(), [&](auto id) { return s.entity(id).kind == "Line"; }));
    for (int i = single ? 0 : 1; i < ids.size(); ++i) {
        sketch::Constraint c;
        c.kind = kind;
        c.a = single ? ids[i] : ids[0];
        c.b = single ? QString() : ids[i];
        auto reason = sketch::constraintReason(s, kind, c.a, c.b);
        domain::check(reason.isEmpty(), "UnsupportedOperation", reason);
        if (kind == "Fixed") {
            auto p = sketch::location(s, s.entity(c.a));
            c.value = p.x;
            c.second = p.y;
        }
        result.push_back(c);
    }
    return result;
}
} // namespace
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
    tool->addItems({"Point", "Line", "Polyline", "Rectangle", "Circle", "Arc", "3 Point Arc",
                    "Tangent Arc", "Slot", "Ellipse", "EllipticalArc", "Parabola", "Spline",
                    "Select / drag", "Trim / extend"});
    tool->setCurrentText("Select / drag");
    form->addRow("Tool", tool);
    rectangleMethod = new QComboBox;
    rectangleMethod->setObjectName("sketchRectangleMethod");
    rectangleMethod->setAccessibleName("Rectangle method");
    rectangleMethod->addItems(sketch::rectangleMethods());
    form->addRow("Rectangle method", rectangleMethod);
    connect(rectangleMethod, &QComboBox::currentTextChanged, this, [this] {
        clicks.clear();
        view->cancelPointerInput();
        if (editing)
            rebuild();
        view->setRectangleInput(rectangleMethod->currentText(), {});
    });
    slotMethod = new QComboBox;
    slotMethod->setObjectName("sketchSlotMethod");
    slotMethod->setAccessibleName("Slot method");
    slotMethod->addItems(sketch::slotMethods());
    form->addRow("Slot method", slotMethod);
    slotLengthMode = new QComboBox;
    slotLengthMode->setObjectName("sketchSlotLengthMode");
    slotLengthMode->addItems({"Center to Center", "Overall Length"});
    form->addRow("Slot length convention", slotLengthMode);
    slotAddDimensions = new QCheckBox("Add driving dimensions");
    slotAddDimensions->setObjectName("sketchSlotAddDimensions");
    form->addRow(slotAddDimensions);
    connect(slotMethod, &QComboBox::currentIndexChanged, this, [this] {
        clicks.clear();
        view->cancelPointerInput();
        updateToolFields();
        if (editing)
            rebuild();
    });
    splineDegree = new QSpinBox;
    splineDegree->setRange(1, 5);
    splineDegree->setValue(3);
    splineDegree->setObjectName("sketchSplineDegree");
    form->addRow("Spline degree", splineDegree);
    splineClosed = new QCheckBox("Close spline");
    splineClosed->setObjectName("sketchSplineClosed");
    form->addRow(splineClosed);
    finishSpline = new QPushButton("Complete spline (Enter)");
    finishSpline->setObjectName("completeSketchSpline");
    form->addRow(finishSpline);
    connect(finishSpline, &QPushButton::clicked, this,
            [this] { guard([&] { finishSplineInput(); }); });
    connect(view, &SketchView::completePrimitive, this, [this] {
        if (tool->currentText() == "Spline")
            guard([&] { finishSplineInput(); });
    });
    connect(splineDegree, &QSpinBox::valueChanged, this, [this] {
        view->setCurveInput(tool->currentText(), clicks, splineDegree->value(),
                            splineClosed->isChecked());
    });
    connect(splineClosed, &QCheckBox::toggled, this, [this] {
        view->setCurveInput(tool->currentText(), clicks, splineDegree->value(),
                            splineClosed->isChecked());
    });
    arcMethod = new QComboBox;
    arcMethod->setObjectName("sketchArcMethod");
    arcMethod->setAccessibleName("Arc method");
    arcMethod->addItems({"Centerpoint Arc", "3 Point Arc", "Tangent Arc"});
    form->addRow("Arc method", arcMethod);
    connect(arcMethod, &QComboBox::currentIndexChanged, this,
            [this](int i) { tool->setCurrentText(i == 0 ? "Arc" : arcMethod->itemText(i)); });
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
    selectedRotation = selectedInput("Rotation (deg)", "selectedRectangleRotation");
    selectedSideAngle = selectedInput("Side angle (deg)", "selectedRectangleSideAngle");
    selectedStart = selectedInput("Start (°)", "selectedStart");
    selectedSweep = selectedInput("Sweep (°)", "selectedSweep");
    selectedControls = new QPlainTextEdit;
    selectedControls->setObjectName("selectedCurveControls");
    selectedControls->setAccessibleName("Control point coordinates, x y per line");
    selectedControls->setMaximumHeight(140);
    selectedForm->addRow("Control points (x y per line)", selectedControls);
    selectedWeights = selectedInput("Spline weights", "selectedCurveWeights");
    selectedKnots = selectedInput("Distinct knots", "selectedCurveKnots");
    selectedMultiplicities = selectedInput("Knot multiplicities", "selectedCurveMultiplicities");
    selectedCurveDegree = new QSpinBox;
    selectedCurveDegree->setObjectName("selectedCurveDegree");
    selectedCurveDegree->setRange(1, 5);
    selectedForm->addRow("Spline degree", selectedCurveDegree);
    applySelection = new QPushButton("Apply changes");
    applySelection->setObjectName("applySelectionProperties");
    removeEntity = new QPushButton("Delete shape");
    removeEntity->setObjectName("deleteSelectedEntity");
    selectedForm->addRow(applySelection);
    selectedForm->addRow(removeEntity);
    form->addRow(selectionGroup);
    lineEditGroup = new QGroupBox("Trim / extend lines");
    lineEditGroup->setObjectName("lineEditProperties");
    auto *editForm = new QFormLayout(lineEditGroup);
    editForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    editForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *help = new QLabel(
        "Choose a line and boundaries, then pick an interval or endpoint half. Empty boundaries "
        "use all curves. Rectangle/slot members and conic/spline boundaries are unsupported.");
    help->setWordWrap(true);
    editForm->addRow(help);
    lineEditMode = new QComboBox;
    lineEditMode->setObjectName("lineEditMode");
    lineEditMode->addItems(sketch::lineEditModes());
    editForm->addRow("Method", lineEditMode);
    lineEditSource = new QComboBox;
    lineEditSource->setObjectName("lineEditSource");
    editForm->addRow("Line", lineEditSource);
    lineEditBoundaries = new QListWidget;
    lineEditBoundaries->setObjectName("lineEditBoundaries");
    lineEditBoundaries->setSelectionMode(QAbstractItemView::MultiSelection);
    lineEditBoundaries->setMaximumHeight(110);
    editForm->addRow("Boundaries (select to toggle)", lineEditBoundaries);
    auto editInput = [&](QString label, QString id) {
        auto *field = new QLineEdit("0");
        field->setObjectName(id);
        field->setAccessibleName(label);
        editForm->addRow(label, field);
        return field;
    };
    lineEditX = editInput("Pick X", "lineEditPickX");
    lineEditY = editInput("Pick Y", "lineEditPickY");
    lineEditPreview = new QPushButton("Preview line edit");
    lineEditPreview->setObjectName("previewLineEdit");
    editForm->addRow(lineEditPreview);
    lineEditDisclosure = new QListWidget;
    lineEditDisclosure->setObjectName("lineEditDisclosure");
    lineEditDisclosure->setMaximumHeight(120);
    lineEditDisclosure->setWordWrap(true);
    editForm->addRow("Constraints to remove", lineEditDisclosure);
    lineEditConsent = new QCheckBox("Remove the listed constraints");
    lineEditConsent->setObjectName("lineEditConsent");
    editForm->addRow(lineEditConsent);
    lineEditApply = new QPushButton("Apply line edit to draft");
    lineEditApply->setObjectName("applyLineEdit");
    editForm->addRow(lineEditApply);
    lineEditCancel = new QPushButton("Cancel line edit preview");
    lineEditCancel->setObjectName("cancelLineEdit");
    editForm->addRow(lineEditCancel);
    form->addRow(lineEditGroup);
    connect(lineEditPreview, &QPushButton::clicked, this,
            [this] { guard([&] { requestLineEdit(); }); });
    connect(lineEditCancel, &QPushButton::clicked, this, &SketchPanel::clearLineEdit);
    connect(lineEditConsent, &QCheckBox::toggled, this, [this] {
        lineEditApply->setEnabled(!lineEditCandidate.empty() && (lineEditDisclosure->count() == 0 ||
                                                                 lineEditConsent->isChecked()));
    });
    connect(lineEditApply, &QPushButton::clicked, this, [this] {
        guard([&] {
            domain::check(editing && !client->outstanding() && !lineEditCandidate.empty() &&
                              sketch::encode(draft) == lineEditBefore,
                          "StaleRevision", "Draft changed; preview the edit again");
            domain::check(lineEditDisclosure->count() == 0 || lineEditConsent->isChecked(),
                          "ConstraintDecisionRequired",
                          "Review and explicitly approve the listed constraint removals");
            auto candidate = sketch::parse(lineEditCandidate);
            auto before = current();
            clearLineEdit();
            draft = std::move(candidate);
            selectedId.clear();
            relationSelectionOrder.clear();
            remember(std::move(before));
            rebuild();
            context->setText("Line edit applied to draft · Finish Sketch to commit");
        });
    });
    for (auto *field : {lineEditX, lineEditY})
        connect(field, &QLineEdit::textChanged, this, [this] {
            if (!syncing)
                clearLineEdit();
        });
    for (auto *combo : {lineEditMode, lineEditSource})
        connect(combo, &QComboBox::currentIndexChanged, this, [this] {
            if (!syncing)
                clearLineEdit();
        });
    connect(lineEditBoundaries, &QListWidget::itemSelectionChanged, this, [this] {
        if (!syncing)
            clearLineEdit();
    });
    connect(client, &Client::sketchEditPreview, this,
            [this](QCborArray candidate, QCborArray disclosure) {
                if (!editing || lineEditBefore.empty() || sketch::encode(draft) != lineEditBefore)
                    return;
                lineEditCandidate = candidate;
                lineEditDisclosure->clear();
                for (auto value : disclosure) {
                    auto m = value.toMap();
                    QStringList operands;
                    for (auto key : {"a", "b", "third"}) {
                        auto id = m.value(QLatin1String(key)).toString();
                        for (int i = 0; i < int(current().entities.size()); ++i)
                            if (current().entities[i].id == id)
                                operands.append(current().entities[i].kind + " " +
                                                QString::number(i + 1));
                    }
                    auto text = m.value("kind").toString() + " · " + operands.join(", ");
                    if (!m.value("expression").toString().isEmpty())
                        text += " · " + m.value("expression").toString();
                    else
                        text += " · " + preciseNumber(m.value("value").toDouble());
                    auto *item = new QListWidgetItem(text, lineEditDisclosure);
                    item->setToolTip(text + " · " + m.value("id").toString());
                    item->setData(Qt::UserRole, m.value("id").toString());
                }
                lineEditConsent->setChecked(false);
                lineEditConsent->setVisible(!disclosure.empty());
                lineEditApply->setEnabled(disclosure.empty());
                for (auto &s : sketch::parse(candidate))
                    if (s.id == current().id)
                        view->showSketch(s, {}, true);
                context->setText(
                    disclosure.empty()
                        ? "Preview only · Apply to draft or cancel"
                        : "Preview only · Review listed constraint removals before Apply");
            });
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
    form->addRow("Entities (select 1–3)", entities);
    relation = new QComboBox;
    relation->setObjectName("sketchRelation");
    relation->addItems({"Fixed", "Coincident", "Horizontal", "Vertical", "Parallel",
                        "Perpendicular", "Tangent", "Equal", "Length", "Distance", "Angle",
                        "Radius", "Collinear", "Midpoint", "Concentric", "Coradial", "Symmetric",
                        "Intersection"});
    relation->addItem("Merge Points", "Merge");
    relation->addItem("Curve tangent", "CurveTangent");
    relation->addItem("Pierce");
    form->addRow("Constraint", relation);
    value = input("Dimension / expression", "sketchDimension", "10 mm");
    contactParameterA = input("Contact parameter on first curve", "tangentContactA", "0.5");
    contactParameterB = input("Contact parameter on second curve", "tangentContactB", "0.5");
    pierceSource = new QComboBox;
    pierceSource->setObjectName("pierceSource");
    form->addRow("Source in another sketch", pierceSource);
    pierceLower = input("Source parameter from", "pierceLower", "0");
    pierceUpper = input("Source parameter to", "pierceUpper", "1");
    pierceBranch = new QComboBox;
    pierceBranch->setObjectName("pierceBranch");
    form->addRow("Exact intersection choices", pierceBranch);
    repairPierce = button("Repair selected Pierce reference", "repairPierceReference");
    connect(pierceSource, &QComboBox::currentIndexChanged, this, [this] {
        auto source = pierceSource->currentData().toMap();
        pierceLower->setText(QString::number(source.value("lower").toDouble(), 'g', 17));
        pierceUpper->setText(QString::number(source.value("upper").toDouble(), 'g', 17));
        pierceBranch->clear();
    });
    connect(pierceBranch, &QComboBox::activated, this, [this](int row) {
        auto branch = pierceBranch->itemData(row).toMap();
        pierceLower->setText(QString::number(branch.value("lower").toDouble(), 'g', 17));
        pierceUpper->setText(QString::number(branch.value("upper").toDouble(), 'g', 17));
    });
    connect(repairPierce, &QPushButton::clicked, this, [this] {
        guard([&] {
            int row = constraints->currentRow();
            domain::check(row >= 0 && size_t(row) < current().constraints.size() &&
                              current().constraints[row].kind == "Pierce",
                          "InvalidInput", "Select the Pierce relation to repair");
            auto before = current();
            current().constraints[row].reference = pierceReference();
            remember(std::move(before));
            rebuild();
            previewDraft();
        });
    });
    addConstraint = button("Add constraint", "addSketchConstraint");
    constraints = new QListWidget;
    constraints->setObjectName("sketchConstraints");
    constraints->setMaximumHeight(110);
    form->addRow("Constraints", constraints);
    connect(constraints, &QListWidget::currentRowChanged, this, [this](int row) {
        if (syncing || !editing || draft.empty() || row < 0 ||
            size_t(row) >= current().constraints.size())
            return;
        auto &c = current().constraints[row];
        if (!c.reference)
            return;
        relation->setCurrentText("Pierce");
        QSignalBlocker blocker(pierceSource);
        for (int i = 0; i < pierceSource->count(); ++i) {
            auto source = pierceSource->itemData(i).toMap();
            if (source.value("sketch").toString() == c.reference->sketch &&
                source.value("entity").toString() == c.reference->entity)
                pierceSource->setCurrentIndex(i);
        }
        pierceLower->setText(QString::number(c.reference->lower, 'g', 17));
        pierceUpper->setText(QString::number(c.reference->upper, 'g', 17));
    });
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
        clearLineEdit();
        clicks.clear();
        pendingArcJoin.reset();
        resumeLineTool.clear();
        lineContinuationAnchor = false;
        view->setTool(tool->currentText());
        updateToolFields();
        emit toolChanged(tool->currentText());
    });
    updateToolFields();
    connect(add, &QPushButton::clicked, this, [this] {
        if (view->sizingCircle() || view->sizingArc())
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
        if (editing && tool->currentText() == "Trim / extend") {
            double unit = domain::toSI(1, "length", property("units").toString());
            lineEditX->setText(preciseNumber(a / unit));
            lineEditY->setText(preciseNumber(b / unit));
            guard([&] { requestLineEdit(); });
            return;
        }
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
    connect(view, &SketchView::arcCreated, this,
            [this](double a, double b, double radius, double start, double sweep) {
                if (editing &&
                    (tool->currentText() == "Arc" || tool->currentText() == "3 Point Arc"))
                    guard([&] {
                        auto candidate = current();
                        QString center;
                        candidate.snap({a, b}, 1e-10, 0, &center);
                        if (center.isEmpty())
                            center = candidate.point({a, b}, construction->isChecked());
                        candidate.arc(center, radius, start, sweep, construction->isChecked());
                        candidate.validate();
                        auto before = current();
                        current() = std::move(candidate);
                        remember(std::move(before));
                        rebuild();
                    });
            });
    connect(view, &SketchView::tangentTransition, this, [this](QString source, int end) {
        if (!editing || (tool->currentText() != "Line" && tool->currentText() != "Polyline"))
            return;
        auto previous = tool->currentText();
        tool->setCurrentText("Tangent Arc");
        resumeLineTool = previous;
        view->beginTangent(source, end);
    });
    connect(view, &SketchView::arcJoined, this,
            [this](double a, double b, double radius, double start, double sweep, QString source,
                   int ownEnd, int sourceEnd, bool normal) {
                if (editing && tool->currentText() == "Tangent Arc")
                    guard([&] {
                        auto candidate = current();
                        candidate.entity(source);
                        auto center = candidate.point({a, b}, construction->isChecked());
                        auto arc =
                            candidate.arc(center, radius, start, sweep, construction->isChecked());
                        sketch::Constraint relation;
                        relation.kind = normal ? "ArcNormal" : "ArcTangent";
                        relation.a = arc;
                        relation.b = source;
                        relation.value = ownEnd;
                        relation.second = sourceEnd;
                        candidate.constraints.push_back(relation);
                        candidate.validate();
                        auto before = current();
                        current() = std::move(candidate);
                        remember(std::move(before));
                        rebuild();
                        if (!resumeLineTool.isEmpty()) {
                            auto next = resumeLineTool;
                            auto finish =
                                sketch::curveEnd(current(), current().entity(arc), 1 - ownEnd);
                            tool->setCurrentText(next);
                            pendingArcJoin = std::pair{arc, 1 - ownEnd};
                            clicks = {finish};
                            lineContinuationAnchor = true;
                            rebuild();
                            view->continueFrom(arc, 1 - ownEnd);
                        }
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
    connect(view, &SketchView::dimensionEditRequested, this,
            [this](QString id) { openDimensionEditor(id); });
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
        auto selected = entities->selectedItems();
        QStringList ids;
        for (auto *item : selected)
            ids.append(item->data(Qt::UserRole).toString());
        relationSelectionOrder.removeIf([&](const auto &id) { return !ids.contains(id); });
        for (auto id : ids)
            if (!relationSelectionOrder.contains(id))
                relationSelectionOrder.append(id);
        updateRelations();
        selectedId = selected.empty() ? QString() : selected.first()->data(Qt::UserRole).toString();
        view->showSketch(current(), selectedId, editing);
        updateSelection();
        emit selectionChanged();
    });
    connect(preview, &QPushButton::clicked, this, &SketchPanel::previewDraft);
    connect(accept, &QPushButton::clicked, this, [this] {
        if (!lineEditBefore.empty()) {
            context->setText("Apply or cancel the line edit preview before finishing");
            return;
        }
        if (view->sizingCircle() || view->sizingArc()) {
            context->setText("Finish or cancel the circle/arc before accepting");
            return;
        }
        if (!clicks.empty() && tool->currentText() != "Polyline" && !lineContinuationAnchor) {
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
        if (editing && !dimensionEditing) {
            draft = sketch::parse(a);
            rebuild();
        }
    });
    connect(client, &Client::sketchDiagnosed, this, [this](QCborArray diagnostics) {
        if (!editing || draft.empty() || dimensionEditing)
            return;
        for (auto item : diagnostics) {
            auto d = item.toMap();
            if (d.value("sketch_id").toString() != current().id)
                continue;
            if (d.contains(QStringLiteral("reference_candidates"))) {
                QSignalBlocker blocker(pierceBranch);
                pierceBranch->clear();
                for (auto choice : d.value("reference_candidates").toArray()) {
                    auto branch = choice.toMap();
                    double unit = domain::toSI(1, "length", property("units").toString());
                    pierceBranch->addItem(QString("t=%1 · (%2, %3) %4")
                                              .arg(branch.value("parameter").toDouble(), 0, 'g', 8)
                                              .arg(branch.value("x").toDouble() / unit, 0, 'g', 8)
                                              .arg(branch.value("y").toDouble() / unit, 0, 'g', 8)
                                              .arg(property("units").toString()),
                                          branch.toVariantMap());
                    for (int i = 0; i < int(current().constraints.size()); ++i)
                        if (current().constraints[i].id == branch.value("constraint").toString())
                            constraints->setCurrentRow(i);
                }
            }
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
        guard([&] {
            int at = constraints->currentRow();
            if (editing && at >= 0) {
                auto before = current(), candidate = before;
                auto id = candidate.constraints[at].id;
                for (auto &g : candidate.groups)
                    domain::check(!g.relations.contains(id), "ConstructionRelation",
                                  "This relation preserves a rectangle construction. Edit or "
                                  "delete the construction instead.");
                candidate.constraints.erase(candidate.constraints.begin() + at);
                candidate.validate();
                current() = std::move(candidate);
                remember(std::move(before));
                rebuild();
            }
        });
    });
    connect(setDimension, &QPushButton::clicked, this, [this] {
        guard([&] {
            int at = constraints->currentRow();
            domain::check(editing && at >= 0, "InvalidInput",
                          "Select a dimension in an active edit");
            domain::check(sketch::dimensional(current().constraints[at].kind), "InvalidInput",
                          "Select a dimensional constraint");
            domain::check(
                current().constraints[at].driving, "InvalidInput",
                "Reference dimensions are readouts; double-click the label to change mode");
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
        if (focusWidget() == view && editing && !client->outstanding()) {
            if (tool->currentText() == "Spline" && !clicks.empty())
                guard([&] { finishSplineInput(); });
            else if (client->state().phase == Phase::PreviewReady)
                accept->click();
        }
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
        clearLineEdit();
        view->cancelPointerInput();
        committing = false;
        document = d.value("id").toString();
        revision = client->state().revision;
        accepted = sketch::parse(d.value("sketches").toArray());
        draft = accepted;
        editing = false;
        pendingArcJoin.reset();
        resumeLineTool.clear();
        lineContinuationAnchor = false;
        clicks.clear();
        selectedId.clear();
        relationSelectionOrder.clear();
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
    view->cancelPointerInput();
    pendingArcJoin.reset();
    resumeLineTool.clear();
    lineContinuationAnchor = false;
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
    tool->setCurrentText("Select / drag");
    rebuild();
    sketches->setCurrentIndex(int(draft.size() - 1));
    rebuild();
    emit editingChanged();
}
void SketchPanel::discard() {
    if (!editing || committing)
        return;
    clearLineEdit();
    view->cancelPointerInput();
    pendingArcJoin.reset();
    resumeLineTool.clear();
    lineContinuationAnchor = false;
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
    if (!lineEditBefore.empty()) {
        clearLineEdit();
        context->setText("Line edit preview canceled · draft retained");
        return;
    }
    bool pending = view->cancelPointerInput() || !clicks.empty();
    clicks.clear();
    pendingArcJoin.reset();
    resumeLineTool.clear();
    lineContinuationAnchor = false;
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
    bool draw = t != "Select / drag" && t != "Trim / extend";
    lineEditGroup->setVisible(editing && t == "Trim / extend");
    bool arc = t == "Arc" || t == "3 Point Arc" || t == "Tangent Arc";
    rootForm->setRowVisible(splineDegree, t == "Spline");
    rootForm->setRowVisible(splineClosed, t == "Spline");
    rootForm->setRowVisible(finishSpline, t == "Spline");
    view->setCurveInput(t, clicks, splineDegree->value(), splineClosed->isChecked());
    rootForm->setRowVisible(rectangleMethod, t == "Rectangle");
    rootForm->setRowVisible(slotMethod, t == "Slot");
    rootForm->setRowVisible(slotLengthMode,
                            t == "Slot" && !sketch::curvedSlot(slotMethod->currentText()));
    rootForm->setRowVisible(slotAddDimensions, t == "Slot");
    view->setSlotInput(slotMethod->currentText(), clicks);
    if (t == "Rectangle")
        view->setRectangleInput(rectangleMethod->currentText(), clicks);
    qobject_cast<QFormLayout *>(arcMethod->parentWidget()->layout())->setRowVisible(arcMethod, arc);
    {
        QSignalBlocker block(arcMethod);
        arcMethod->setCurrentIndex(t == "Tangent Arc" ? 2 : t == "3 Point Arc" ? 1 : 0);
    }
    for (auto *field : {x, y})
        form->setRowVisible(field, draw && t != "Tangent Arc");
    form->setRowVisible(radius, t == "Circle" || t == "Arc" || t == "3 Point Arc");
    form->setRowVisible(start, (t == "Arc" || t == "3 Point Arc"));
    form->setRowVisible(sweep, (t == "Arc" || t == "3 Point Arc"));
    form->setRowVisible(construction, draw);
    auto *numericToggle = propertyPane->findChild<QToolButton *>("numericSketchInputToggle");
    numericToggle->setVisible(draw);
    x->parentWidget()->setVisible(draw && numericToggle->isChecked());
    form->setRowVisible(add, draw && t != "Tangent Arc");
    updateSelection();
    add->setEnabled(editing && t != "Tangent Arc");
    add->setToolTip(t == "Tangent Arc" ? "Choose a source endpoint in the canvas"
                                       : "Create precise numeric geometry");
    add->setText(t == "Circle" || t == "Arc" || t == "3 Point Arc"
                     ? "Add center with numeric radius"
                     : "Add coordinate / next vertex");
}
void SketchPanel::rebuild() {
    syncing = true;
    auto constraintId = constraints->currentItem()
                            ? constraints->currentItem()->data(Qt::UserRole).toString()
                            : QString();
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
        for (auto &c : s.constraints) {
            QString label =
                c.kind == "ArcTangent"  ? "Tangent at endpoints"
                : c.kind == "ArcNormal" ? "Normal at endpoints"
                : c.kind == "ArcCoincident"
                    ? "Coincident endpoint"
                    : c.kind + " · " +
                          (c.expression.isEmpty() ? QString::number(c.value) : c.expression);
            if (!c.driving)
                label = "Reference · " + label;
            auto *item = new QListWidgetItem(label, constraints);
            item->setData(Qt::UserRole, c.id);
            if (c.id == constraintId)
                constraints->setCurrentItem(item);
        }
        view->showSketch(s, selectedId, editing);
    } else
        view->clearSketch();
    view->setInputAnchor(clicks.empty() ? std::nullopt : std::optional(clicks.back()));
    view->setRectangleInput(rectangleMethod->currentText(), clicks);
    view->setSlotInput(slotMethod->currentText(), clicks);
    view->setCurveInput(tool->currentText(), clicks, splineDegree->value(),
                        splineClosed->isChecked());
    refreshPierceSources();
    refreshLineEdit();
    syncing = false;
    updateRelations();
    for (auto *b : {add, addConstraint, setDimension, removeConstraint, setName})
        b->setEnabled(editing && (b != add || tool->currentText() != "Tangent Arc"));
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
    QString newLine;
    bool joinedEndpoint = false, nextIsContinuation = false;
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
    else if (t == "Circle" || t == "Arc" || t == "3 Point Arc") {
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
    } else if (sketch::advancedCurve(t)) {
        domain::check(nextClicks.size() < 63, "ResourceLimit", "At most 63 input control points");
        nextClicks.push_back(p);
        if (t != "Spline" && int(nextClicks.size()) == sketch::curveInputCount(t)) {
            sketch::createCurveInput(s, t, nextClicks, splineDegree->value(),
                                     splineClosed->isChecked(), c);
            nextClicks.clear();
        }
    } else if (t == "Slot") {
        nextClicks.push_back(p);
        if (int(nextClicks.size()) == sketch::slotInputCount(slotMethod->currentText())) {
            sketch::createSlot(s, slotMethod->currentText(), nextClicks,
                               slotLengthMode->currentText(), slotAddDimensions->isChecked(), c);
            nextClicks.clear();
        }
    } else if (t == "Rectangle") {
        nextClicks.push_back(p);
        if (int(nextClicks.size()) == sketch::rectangleInputCount(rectangleMethod->currentText())) {
            sketch::createRectangle(s, rectangleMethod->currentText(), nextClicks, c);
            nextClicks.clear();
        }
    } else {
        nextClicks.push_back(p);
        if (nextClicks.size() >= 2) {
            auto a = nextClicks[nextClicks.size() - 2];
            domain::check(sketch::length(a, p) > 1e-9, "InvalidGeometry",
                          "Distinct vertices required");
            {
                auto first = point(a);
                newLine = s.line(first, point(p), c);
                if (pendingArcJoin) {
                    sketch::Constraint join;
                    join.kind = "ArcCoincident";
                    join.a = pendingArcJoin->first;
                    join.b = first;
                    join.value = pendingArcJoin->second;
                    join.second = 0;
                    s.constraints.push_back(join);
                    joinedEndpoint = true;
                }
            }
            if (t == "Line" && snapInput) {
                nextClicks = {p};
                nextIsContinuation = true;
            } else if (t != "Polyline")
                nextClicks.clear();
        }
    }
    candidate.validate();
    current() = std::move(candidate);
    clicks = std::move(nextClicks);
    remember(std::move(before));
    if (joinedEndpoint)
        pendingArcJoin.reset();
    lineContinuationAnchor = nextIsContinuation;
    rebuild();
    if (!newLine.isEmpty())
        view->continueFrom(newLine, 1);
}
void SketchPanel::finishSplineInput() {
    domain::check(editing && tool->currentText() == "Spline", "InvalidInput",
                  "Choose the Spline tool");
    auto before = current(), candidate = before;
    auto id = sketch::createSpline(candidate, clicks, splineDegree->value(),
                                   splineClosed->isChecked(), construction->isChecked());
    current() = std::move(candidate);
    clicks.clear();
    selectedId = id;
    remember(std::move(before));
    rebuild();
}
void SketchPanel::previewDraft() {
    if (view->sizingCircle() || view->sizingArc()) {
        context->setText("Finish or cancel the circle/arc before solving");
        return;
    }
    if (!clicks.empty() && tool->currentText() != "Polyline" && !lineContinuationAnchor) {
        context->setText("Complete the remaining vertices before previewing");
        return;
    }
    client->projectCommand("PreviewSketches", fields({{"sketches", sketch::encode(draft)}}));
}
bool SketchPanel::dimensionAvailable() const {
    if (relationSelectionOrder.size() >= 2) {
        if (relationSelectionOrder.size() > 2)
            return false;
        for (auto &s : editing ? draft : accepted)
            if (s.id == sketches->currentData().toString()) {
                QString a, b;
                bool aAxis = false, bAxis = false;
                for (auto &e : s.entities) {
                    if (e.id == relationSelectionOrder[0]) {
                        a = e.kind;
                        aAxis = e.kind == "Line" && e.construction;
                    }
                    if (e.id == relationSelectionOrder[1]) {
                        b = e.kind;
                        bAxis = e.kind == "Line" && e.construction;
                    }
                }
                if ((a == b && (a == "Point" || a == "Line")) || (a == "Point" && bAxis) ||
                    (aAxis && b == "Point"))
                    return true;
            }
        return false;
    }
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
    clearLineEdit();
    if (draftUndo.size() >= 40)
        draftUndo.erase(draftUndo.begin());
    draftUndo.push_back(std::move(before));
    draftRedo.clear();
    emit editingChanged();
}
void SketchPanel::undoDraftEdit() {
    if (!editing || draftUndo.empty() || client->outstanding() || committing)
        return;
    clearLineEdit();
    view->cancelPointerInput();
    pendingArcJoin.reset();
    lineContinuationAnchor = false;
    resumeLineTool.clear();
    clicks.clear();
    draftRedo.push_back(current());
    current() = std::move(draftUndo.back());
    draftUndo.pop_back();
    selectedId.clear();
    relationSelectionOrder.clear();
    rebuild();
    client->discardSketchPreview();
    emit editingChanged();
}
void SketchPanel::redoDraftEdit() {
    if (!editing || draftRedo.empty() || client->outstanding() || committing)
        return;
    clearLineEdit();
    view->cancelPointerInput();
    pendingArcJoin.reset();
    lineContinuationAnchor = false;
    resumeLineTool.clear();
    clicks.clear();
    draftUndo.push_back(current());
    current() = std::move(draftRedo.back());
    draftRedo.pop_back();
    selectedId.clear();
    relationSelectionOrder.clear();
    rebuild();
    client->discardSketchPreview();
    emit editingChanged();
}
void SketchPanel::selectEntity(QString id) {
    selectedId = id;
    relationSelectionOrder = id.isEmpty() ? QStringList{} : QStringList{id};
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
    auto group = sketch::constructionOf(s, selectedId);
    bool slot = group && group->kind == "Slot";
    bool exact = sketch::advancedCurve(e.kind),
         conic = sketch::ellipseCurve(e.kind) || e.kind == "Parabola", spline = e.kind == "Spline";
    bool round = !group && (e.kind == "Circle" || e.kind == "Arc");
    auto *form = qobject_cast<QFormLayout *>(selectionGroup->layout());
    if (auto label = qobject_cast<QLabel *>(form->labelForField(selectedHeight)))
        label->setText(sketch::ellipseCurve(e.kind)                ? "Minor radius"
                       : slot                                      ? "Slot width"
                       : group && group->method == "Parallelogram" ? "Side length"
                                                                   : "Height");
    if (auto label = qobject_cast<QLabel *>(form->labelForField(selectedWidth)))
        label->setText(exact  ? (e.kind == "Parabola" ? "Focal distance" : "Major radius")
                       : slot ? (sketch::curvedSlot(group->method) ? "Path radius"
                                                                   : group->lengthMode + " length")
                       : group && group->method == "Parallelogram" ? "Base length"
                                                                   : "Width");
    auto rowVisible = [&](QWidget *field, bool on) {
        if (field->isHidden() == on)
            form->setRowVisible(field, on);
    };
    rowVisible(selectedRadius, round);
    rowVisible(selectedWidth, bool(rect) || slot || conic);
    rowVisible(selectedHeight, bool(rect) || slot || sketch::ellipseCurve(e.kind));
    rowVisible(selectedRotation,
               conic || (group && group->method != "Corner" && group->method != "Center"));
    rowVisible(selectedSideAngle, group && group->method == "Parallelogram");
    rowVisible(selectedStart, exact ? e.kind != "Ellipse" : !group && e.kind == "Arc");
    rowVisible(selectedSweep, exact  ? e.kind != "Ellipse"
                              : slot ? sketch::curvedSlot(group->method)
                                     : !group && e.kind == "Arc");
    for (QWidget *field : std::array<QWidget *, 5>{selectedControls, selectedWeights, selectedKnots,
                                                   selectedMultiplicities, selectedCurveDegree})
        rowVisible(field, spline);
    if (auto label = qobject_cast<QLabel *>(form->labelForField(selectedStart)))
        label->setText(e.kind == "Parabola" ? "First extent (display units)"
                       : spline             ? "Start parameter"
                                            : "Start (degrees)");
    if (auto label = qobject_cast<QLabel *>(form->labelForField(selectedSweep)))
        label->setText(e.kind == "Parabola" ? "Extent span (display units)"
                       : spline             ? "Parameter span"
                                            : "Sweep (degrees)");
    auto p = e.kind == "Point" ? e.point : s.entity(e.a).point;
    double width = 0, height = 0;
    if (conic) {
        p = s.entity(e.a).point;
        auto focus = s.entity(e.b).point;
        double f = sketch::length(p, focus);
        width = e.kind == "Parabola" ? f : std::hypot(f, e.radius);
        height = e.radius;
        selectedRotation->setText(QString::number(
            std::atan2(focus.y - p.y, focus.x - p.x) * 180 / sketch::arcPi, 'g', 17));
    } else if (slot) {
        auto parameters = sketch::slotParameters(s, *group);
        p = parameters.anchor;
        width = sketch::curvedSlot(group->method)
                    ? parameters.radius
                    : parameters.length +
                          (group->lengthMode == "Overall Length" ? parameters.width : 0);
        height = parameters.width;
        selectedRotation->setText(
            QString::number(parameters.rotation * 180 / sketch::arcPi, 'g', 15));
    } else if (group) {
        auto parameters = sketch::rectangleParameters(s, *group);
        p = parameters.anchor;
        width = parameters.width;
        height = parameters.side;
        selectedRotation->setText(
            QString::number(parameters.rotation * 180 / 3.141592653589793, 'g', 12));
        selectedSideAngle->setText(
            QString::number(parameters.sideAngle * 180 / 3.141592653589793, 'g', 12));
    } else if (rect) {
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
    auto text = [&](double value) {
        double displayed = value / unit;
        return preciseNumber(std::abs(displayed) < 1e-10 ? 0 : displayed);
    };
    auto dim = [&](QString id, double value) {
        for (auto &c : s.constraints)
            if (c.driving && (c.kind == "Radius" || c.kind == "Length") && c.a == id)
                return c.expression.isEmpty() ? text(c.value) : c.expression;
        return text(value);
    };
    selectedX->setText(text(p.x));
    selectedY->setText(text(p.y));
    selectedRadius->setText(dim(e.id, e.radius));
    selectedStart->setText(QString::number(e.start * 180 / 3.141592653589793, 'g', 12));
    selectedSweep->setText(QString::number(
        (slot ? sketch::slotParameters(s, *group).sweep : e.sweep) * 180 / 3.141592653589793, 'g',
        12));
    selectedWidth->setText(text(width));
    selectedHeight->setText(text(height));
    if (e.kind == "Parabola") {
        selectedStart->setText(text(e.start));
        selectedSweep->setText(text(e.sweep));
    }
    if (spline) {
        selectedStart->setText(preciseNumber(e.start));
        selectedSweep->setText(preciseNumber(e.sweep));
        auto numbers = [](const auto &values) {
            QStringList out;
            for (auto v : values)
                out.append(preciseNumber(v));
            return out.join(" ");
        };
        selectedWeights->setText(numbers(e.weights));
        selectedKnots->setText(numbers(e.knots));
        QStringList mult;
        for (auto v : e.multiplicities)
            mult.append(QString::number(v));
        selectedMultiplicities->setText(mult.join(" "));
        selectedCurveDegree->setValue(e.degree);
        QStringList controls;
        for (auto id : e.poles) {
            auto p = s.entity(id).point;
            controls.append(preciseNumber(p.x / unit) + " " + preciseNumber(p.y / unit));
        }
        selectedControls->setPlainText(controls.join("\n"));
    }
    if (group && !slot) {
        for (int i = 0; i < 2; ++i) {
            QString id = group->edges[i];
            for (auto &c : s.constraints)
                if (c.driving && c.kind == "Length" && (c.a == id || c.a == group->edges[i + 2]))
                    id = c.a;
            (i == 0 ? selectedWidth : selectedHeight)->setText(dim(id, i == 0 ? width : height));
        }
    } else if (slot) {
        for (auto &c : s.constraints)
            if (!c.expression.isEmpty()) {
                if (c.kind == "Diameter" && c.a == group->edges[2])
                    selectedHeight->setText(c.expression);
                if (c.a == group->diagonals[0] &&
                    QStringList{"Length", "SlotLength", "Radius"}.contains(c.kind))
                    selectedWidth->setText(c.expression);
                if (c.kind == "Sweep" && c.a == group->diagonals[0])
                    selectedSweep->setText(c.expression);
            }
    } else if (rect)
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
    selectionTitle->setText((group  ? group->method + " " + group->kind
                             : rect ? "Rectangle"
                                    : e.kind) +
                            " · " + units);
    bool idle = !client->outstanding();
    applySelection->setEnabled(editing && idle);
    removeEntity->setEnabled(editing && idle);
    removeEntity->setText(slot ? "Delete slot" : rect ? "Delete rectangle" : "Delete selection");
    for (auto *field : {selectedX, selectedY, selectedWidth, selectedHeight, selectedRadius,
                        selectedStart, selectedSweep, selectedRotation, selectedSideAngle,
                        selectedWeights, selectedKnots, selectedMultiplicities})
        field->setReadOnly(!editing || !idle);
    selectedControls->setReadOnly(!editing || !idle);
    for (auto *field : {selectedX, selectedY, selectedRadius, selectedWidth, selectedHeight,
                        selectedRotation, selectedSideAngle, selectedStart, selectedSweep,
                        selectedWeights, selectedKnots, selectedMultiplicities})
        if (!field->hasFocus())
            field->setCursorPosition(0);
    selectedCurveDegree->setEnabled(editing && idle);
}
void SketchPanel::applySelected() {
    domain::check(editing && !selectedId.isEmpty(), "InvalidInput",
                  "Edit a sketch and select geometry");
    auto before = current(), candidate = before;
    auto &e = candidate.entity(selectedId);
    auto rect = sketch::rectangleOf(before, selectedId);
    auto group = sketch::constructionOf(before, selectedId);
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
            if (c.driving && c.kind == kind && c.a == id) {
                c.expression = expression;
                return numeric ? n : old;
            }
        if (!numeric || group) {
            sketch::Constraint c;
            c.kind = kind;
            c.a = id;
            c.expression = expression;
            candidate.constraints.push_back(c);
        }
        return numeric ? n : old;
    };
    if (sketch::advancedCurve(e.kind)) {
        auto scalar = [](QLineEdit *field) {
            bool ok;
            double n = field->text().toDouble(&ok);
            domain::check(ok && std::isfinite(n), "InvalidInput",
                          "Finite numeric curve parameter required");
            return n;
        };
        if (e.kind == "Spline") {
            domain::check(selectedControls->toPlainText().toUtf8().size() <= 8192, "ResourceLimit",
                          "Control geometry text is too large");
            auto rows = selectedControls->toPlainText().split('\n', Qt::SkipEmptyParts);
            domain::check(rows.size() == e.poles.size(), "InvalidInput",
                          "Keep the identified control-point count when editing coordinates");
            auto old = before.entity(e.a).point;
            QMap<QString, sketch::Vec2> points;
            for (int i = 0; i < rows.size(); ++i) {
                auto coordinates =
                    rows[i].split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
                domain::check(coordinates.size() == 2, "InvalidInput",
                              "Two coordinates per control-point row required");
                bool ax, ay;
                double px = coordinates[0].toDouble(&ax), py = coordinates[1].toDouble(&ay);
                domain::check(ax && ay, "InvalidInput",
                              "Numeric control-point coordinates required");
                sketch::Vec2 q{domain::toSI(px, "length", units) + x - old.x,
                               domain::toSI(py, "length", units) + y - old.y};
                if (points.contains(e.poles[i]))
                    domain::check(sketch::length(points[e.poles[i]], q) < 1e-12, "InvalidInput",
                                  "Closed spline endpoint rows must agree");
                points[e.poles[i]] = q;
            }
            for (auto i = points.begin(); i != points.end(); ++i)
                candidate.entity(i.key()).point = i.value();
            auto numbers = [](QLineEdit *field) {
                auto list = field->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
                domain::check(list.size() <= 64, "ResourceLimit", "At most 64 spline coefficients");
                std::vector<double> out;
                for (auto text : list) {
                    bool ok;
                    double n = text.toDouble(&ok);
                    domain::check(ok && std::isfinite(n), "InvalidInput",
                                  "Finite numeric spline coefficients required");
                    out.push_back(n);
                }
                return out;
            };
            e.weights = numbers(selectedWeights);
            e.knots = numbers(selectedKnots);
            e.multiplicities.clear();
            for (double n : numbers(selectedMultiplicities)) {
                domain::check(n >= 1 && n <= 6 && std::floor(n) == n, "InvalidInput",
                              "Integer knot multiplicities from 1 to 6 required");
                e.multiplicities.push_back(int(n));
            }
            e.degree = selectedCurveDegree->value();
            e.start = scalar(selectedStart);
            e.sweep = scalar(selectedSweep);
        } else {
            double major = number(selectedWidth),
                   minor = sketch::ellipseCurve(e.kind) ? number(selectedHeight) : 0,
                   rotation = scalar(selectedRotation) * sketch::arcPi / 180;
            domain::check(major > 1e-9 && (!sketch::ellipseCurve(e.kind) ||
                                           (minor > 1e-9 && major > minor + 1e-9)),
                          "InvalidGeometry", "Positive distinct curve dimensions required");
            double f = e.kind == "Parabola" ? major : std::sqrt(major * major - minor * minor);
            candidate.entity(e.a).point = {x, y};
            candidate.entity(e.b).point = {x + f * std::cos(rotation), y + f * std::sin(rotation)};
            if (sketch::ellipseCurve(e.kind))
                e.radius = minor;
            if (e.kind == "Parabola") {
                e.start = number(selectedStart);
                e.sweep = number(selectedSweep);
            } else if (e.kind == "EllipticalArc") {
                e.start = scalar(selectedStart) * sketch::arcPi / 180;
                e.sweep = scalar(selectedSweep) * sketch::arcPi / 180;
            }
        }
    } else if (group && group->kind == "Slot") {
        auto parameters = sketch::slotParameters(before, *group);
        parameters.anchor = {x, y};
        auto read = [&](QLineEdit *field, double old) {
            bool ok;
            double value = field->text().toDouble(&ok);
            return ok ? domain::toSI(value, "length", units) : old;
        };
        parameters.width = read(selectedHeight, parameters.width);
        if (sketch::curvedSlot(group->method))
            parameters.radius = read(selectedWidth, parameters.radius);
        else
            parameters.length =
                read(selectedWidth,
                     parameters.length +
                         (group->lengthMode == "Overall Length" ? parameters.width : 0)) -
                (group->lengthMode == "Overall Length" ? parameters.width : 0);
        bool ok;
        parameters.rotation = selectedRotation->text().toDouble(&ok) * sketch::arcPi / 180;
        domain::check(ok, "InvalidInput", "Numeric slot rotation required");
        if (sketch::curvedSlot(group->method)) {
            double sweep = selectedSweep->text().toDouble(&ok);
            if (ok)
                parameters.sweep = sweep * sketch::arcPi / 180;
        }
        for (auto &constraint : before.constraints)
            if (constraint.driving && !group->relations.contains(constraint.id)) {
                auto members = sketch::constructionMembers(*group);
                bool ownDimension =
                    (constraint.a == group->edges[2] && constraint.kind == "Diameter") ||
                    (constraint.a == group->diagonals[0] &&
                     QStringList{"Length", "SlotLength", "Radius", "Sweep"}.contains(
                         constraint.kind));
                domain::check(
                    ownDimension ||
                        (!members.contains(constraint.a) && !members.contains(constraint.b)),
                    "ConstrainedGeometry",
                    "Slot has another controlling relation; edit or remove it explicitly");
            }
        sketch::resizeSlot(candidate, *group, parameters);
        sketch::slotDimensions(candidate, *group);
        for (auto &constraint : candidate.constraints) {
            QLineEdit *field = nullptr;
            if (constraint.kind == "Diameter" && constraint.a == group->edges[2])
                field = selectedHeight;
            else if (constraint.a == group->diagonals[0] &&
                     QStringList{"Length", "SlotLength", "Radius"}.contains(constraint.kind))
                field = selectedWidth;
            else if (constraint.kind == "Sweep" && constraint.a == group->diagonals[0])
                field = selectedSweep;
            if (field && constraint.driving) {
                bool numeric;
                field->text().toDouble(&numeric);
                constraint.expression =
                    field->text() +
                    (numeric ? (constraint.kind == "Sweep" ? " deg" : " " + units) : QString());
            }
        }
        for (auto &g : candidate.groups)
            if (g.id == group->id)
                g.dimensions = true;
    } else if (group) {
        auto parameters = sketch::rectangleParameters(before, *group);
        auto driving = [&](int side) {
            QString id = group->edges[side];
            for (auto &c : before.constraints)
                if (c.driving && c.kind == "Length" && (c.a == id || c.a == group->edges[side + 2]))
                    id = c.a;
            return id;
        };
        parameters.anchor = {x, y};
        parameters.width = dimension(driving(0), "Length", selectedWidth, parameters.width);
        parameters.side = dimension(driving(1), "Length", selectedHeight, parameters.side);
        auto angle = [&](QLineEdit *field) {
            bool ok;
            double v = field->text().toDouble(&ok);
            domain::check(ok, "InvalidInput", "Numeric rectangle angle required");
            return domain::toSI(v, "angle", "deg");
        };
        if (group->method != "Corner" && group->method != "Center")
            parameters.rotation = angle(selectedRotation);
        if (group->method == "Parallelogram")
            parameters.sideAngle = angle(selectedSideAngle);
        sketch::resizeRectangle(candidate, *group, parameters);
        sketch::checkConstructionEdit(before, candidate, *group);
    } else if (rect) {
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
            if (c.driving && c.kind == "Length")
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
void SketchPanel::refreshPierceSources() {
    auto selected = pierceSource->currentData().toMap();
    QString oldSketch = selected.value("sketch").toString(),
            oldEntity = selected.value("entity").toString();
    QSignalBlocker blocker(pierceSource);
    pierceSource->clear();
    if (!editing || draft.empty())
        return;
    int chosen = -1;
    for (auto &s : draft) {
        if (s.id == current().id)
            continue;
        int number = 0;
        for (auto &e : s.entities) {
            ++number;
            auto range =
                e.kind == "Point" ? std::pair<double, double>{-1, 1} : sketch::curveRange(e);
            sketch::CurveReference ref{"Self",
                                       s.id,
                                       e.id,
                                       e.kind,
                                       range.first,
                                       range.second,
                                       (range.first + range.second) / 2};
            pierceSource->addItem(s.name + " · " + e.kind + " " + QString::number(number),
                                  ref.map().toVariantMap());
            if (s.id == oldSketch && e.id == oldEntity)
                chosen = pierceSource->count() - 1;
        }
    }
    pierceSource->setCurrentIndex(chosen >= 0 ? chosen : 0);
    if (chosen < 0 && pierceSource->count()) {
        auto source = pierceSource->currentData().toMap();
        pierceLower->setText(QString::number(source.value("lower").toDouble(), 'g', 17));
        pierceUpper->setText(QString::number(source.value("upper").toDouble(), 'g', 17));
    }
}
sketch::CurveReference SketchPanel::pierceReference() const {
    auto source = QCborMap::fromVariantMap(pierceSource->currentData().toMap());
    domain::check(!source.isEmpty(), "ReferenceMissing", "Select a source in another sketch");
    auto ref = sketch::CurveReference::parse(source);
    bool a, b;
    ref.lower = pierceLower->text().toDouble(&a);
    ref.upper = pierceUpper->text().toDouble(&b);
    domain::check(a && b, "InvalidInput", "Numeric source parameter interval required");
    ref.parameter = (ref.lower + ref.upper) / 2;
    ref.validate();
    return ref;
}
void SketchPanel::updateRelations() {
    if (!editing || draft.empty())
        return;
    auto kind = relation->currentData().toString();
    if (kind.isEmpty())
        kind = relation->currentText();
    auto form = qobject_cast<QFormLayout *>(contactParameterA->parentWidget()->layout());
    form->setRowVisible(contactParameterA, kind == "CurveTangent");
    form->setRowVisible(contactParameterB, kind == "CurveTangent");
    for (QWidget *field : std::initializer_list<QWidget *>{pierceSource, pierceLower, pierceUpper,
                                                           pierceBranch, repairPierce})
        form->setRowVisible(field, kind == "Pierce");
    repairPierce->setEnabled(!client->outstanding());
    auto selected = entities->selectedItems();
    QStringList ids;
    for (auto id : relationSelectionOrder)
        for (auto *item : selected)
            if (item->data(Qt::UserRole).toString() == id)
                ids.append(id);
    for (auto *item : selected) {
        auto id = item->data(Qt::UserRole).toString();
        if (!ids.contains(id))
            ids.append(id);
    }
    auto *model = qobject_cast<QStandardItemModel *>(relation->model());
    for (auto id : ids)
        if (std::none_of(current().entities.begin(), current().entities.end(),
                         [&](auto &entity) { return entity.id == id; })) {
            for (int i = 0; i < relation->count(); ++i) {
                model->item(i)->setEnabled(false);
                model->item(i)->setToolTip("Selection changed; select geometry again");
            }
            addConstraint->setEnabled(false);
            return;
        }
    for (int i = 0; i < relation->count(); ++i) {
        QString a = ids.value(0), b = ids.value(1), third = ids.value(2);
        auto kind = relation->itemData(i).toString();
        if (kind.isEmpty())
            kind = relation->itemText(i);
        relationOperands(current(), kind, a, b, third);
        QString reason;
        if (ids.size() > 2 && batchRelation(kind)) {
            try {
                relationBatch(current(), kind, ids);
            } catch (const domain::Error &e) {
                reason = e.what();
            }
        } else {
            reason = ids.size() > 3 ? QString("This relation uses at most three entities")
                                    : sketch::constraintReason(current(), kind, a, b, third);
            if (ids.size() == 3 && kind != "Intersection" && kind != "Symmetric")
                reason = "This relation uses at most two entities";
        }
        if (kind == "Pierce" && pierceSource->count() == 0)
            reason = "Create source geometry in another sketch first";
        auto row = model->item(i);
        row->setEnabled(reason.isEmpty());
        row->setToolTip(reason);
    }
    addConstraint->setEnabled(!client->outstanding() &&
                              model->item(relation->currentIndex())->isEnabled());
}
void SketchPanel::addRelation() {
    domain::check(editing, "InvalidInput", "Begin a sketch edit first");
    auto selected = entities->selectedItems();
    domain::check(!selected.empty() && selected.size() <= 256, "InvalidInput",
                  "Select geometry for the relation");
    QStringList ids;
    for (auto id : relationSelectionOrder)
        for (auto *item : selected)
            if (item->data(Qt::UserRole).toString() == id)
                ids.append(id);
    for (auto *item : selected) {
        auto id = item->data(Qt::UserRole).toString();
        if (!ids.contains(id))
            ids.append(id);
    }
    sketch::Constraint c;
    c.kind = relation->currentData().toString();
    if (c.kind.isEmpty())
        c.kind = relation->currentText();
    if (ids.size() > 2 && batchRelation(c.kind)) {
        auto candidate = current();
        auto batch = relationBatch(candidate, c.kind, ids);
        candidate.constraints.insert(candidate.constraints.end(), batch.begin(), batch.end());
        candidate.validate();
        auto before = current();
        current() = std::move(candidate);
        remember(std::move(before));
        rebuild();
        return;
    }
    c.a = ids.value(0);
    c.b = ids.value(1);
    c.third = ids.value(2);
    relationOperands(current(), c.kind, c.a, c.b, c.third);
    auto reason = sketch::constraintReason(current(), c.kind, c.a, c.b, c.third);
    domain::check(reason.isEmpty(), "UnsupportedOperation", reason);
    if (c.kind == "Pierce")
        c.reference = pierceReference();
    if (c.kind == "CurveTangent") {
        bool first, second;
        c.value = contactParameterA->text().toDouble(&first);
        c.second = contactParameterB->text().toDouble(&second);
        domain::check(first && second, "InvalidInput", "Numeric curve contact parameters required");
        auto u = sketch::curveDerivative(current(), current().entity(c.a), c.value),
             v = sketch::curveDerivative(current(), current().entity(c.b), c.second);
        c.reversed = u.x * v.x + u.y * v.y < 0;
    }
    if (c.kind == "Fixed") {
        auto position = sketch::location(current(), current().entity(c.a));
        c.value = position.x;
        c.second = position.y;
    }
    if (c.kind == "Tangent" && current().entity(c.a).kind != "Line") {
        auto &a = current().entity(c.a), &b = current().entity(c.b);
        c.value = sketch::length(current().entity(a.a).point, current().entity(b.a).point) <
                          std::max(a.radius, b.radius)
                      ? 1
                      : 0;
    }
    if (c.kind == "Coincident" && current().entity(c.b).kind == "Spline") {
        auto &curve = current().entity(c.b);
        auto range = sketch::curveRange(curve);
        double best = 1e100;
        for (int i = 0; i <= 128; ++i) {
            double t = range.first + (range.second - range.first) * i / 128;
            double distance = sketch::length(current().entity(c.a).point,
                                             sketch::curveValue(current(), curve, t));
            if (distance < best) {
                best = distance;
                c.second = t;
            }
        }
    }
    if (QStringList{"Length", "Distance", "Angle", "Radius"}.contains(c.kind))
        c.expression = value->text();
    auto before = current();
    current().constraints.push_back(c);
    remember(std::move(before));
    rebuild();
}
} // namespace cad

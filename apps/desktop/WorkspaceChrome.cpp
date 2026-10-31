#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QActionGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
namespace cad {
void MainWindow::buildCommandTabs() {
    auto *manager = findChild<QTabWidget *>("managerTabs");
    auto *reference = findChild<QWidget *>("referenceProperties");
    manager->removeTab(1);
    propertyPages = new QStackedWidget;
    propertyPages->setObjectName("propertyPages");
    propertyPages->addWidget(reference);
    propertyPages->addWidget(sketchPanel->propertyManager());
    auto *empty = new QLabel("Select a Part sketch to edit geometry");
    empty->setAlignment(Qt::AlignTop);
    empty->setMargin(12);
    empty->setWordWrap(true);
    propertyPages->addWidget(empty);
    manager->insertTab(1, propertyPages, "Properties");
    resizeDocks({findChild<QDockWidget *>("modelDock")}, {320}, Qt::Horizontal);
    auto *tools = menuBar()->addMenu("&Sketch");
    auto add = [&](QString id, QString title, QString reason = QString()) {
        auto *a = commands.add(this, id, title, {}, reason);
        tools->addAction(a);
        addAction(a);
        return a;
    };
    auto *edit = add("sketch.edit", "Edit Sketch");
    connect(edit, &QAction::triggered, this, [this] { sketchPanel->editSelected(); });
    auto *finish = add("sketch.finish", "Finish Sketch");
    connect(finish, &QAction::triggered, this, [this] { sketchPanel->finish(); });
    auto *cancel = add("sketch.cancel", "Cancel Sketch");
    connect(cancel, &QAction::triggered, sketchPanel, &SketchPanel::discard);
    auto *dimension = add("sketch.dimension", "Dimension");
    connect(dimension, &QAction::triggered, sketchPanel, &SketchPanel::dimensionSelection);
    auto *remove = add("sketch.delete", "Delete");
    connect(remove, &QAction::triggered, sketchPanel, &SketchPanel::removeSelection);
    auto *group = new QActionGroup(this);
    group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (auto name : {"Select / drag", "Line", "Rectangle", "Circle", "Arc", "3 Point Arc",
                      "Tangent Arc", "Slot", "Ellipse", "EllipticalArc", "Parabola", "Spline",
                      "Polyline", "Point", "Trim / extend"}) {
        auto *action =
            add("sketch.tool." + QString(name), QString(name) == "Select / drag" ? "Select"
                                                : QString(name) == "Arc"         ? "Centerpoint Arc"
                                                                                 : name);
        action->setCheckable(true);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, name](bool checked) {
            sketchPanel->setTool(checked ? name : "Select / drag");
            refreshCommandTabs();
        });
    }
    auto *grid = add("sketch.grid", "Grid snap");
    grid->setCheckable(true);
    auto *check = findChild<QCheckBox *>("sketchGrid");
    connect(grid, &QAction::toggled, check, &QCheckBox::setChecked);
    connect(check, &QCheckBox::toggled, grid, &QAction::setChecked);
    for (auto pair :
         {std::pair{"revolve", "Revolve"}, std::pair{"fillet", "Fillet"},
          std::pair{"chamfer", "Chamfer"}, std::pair{"hole", "Hole"},
          std::pair{"pattern", "Linear pattern"}, std::pair{"circularPattern", "Circular pattern"},
          std::pair{"mirror", "Mirror"}})
        add(pair.first, pair.second, "Exact features are planned after workspace UI (4.1/4.2)");
    auto *toolbar = addToolBar("CommandManager");
    toolbar->setObjectName("commandManager");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    insertToolBarBreak(toolbar);
    commandTabs = new QTabWidget;
    commandTabs->setObjectName("commandTabs");
    commandTabs->setDocumentMode(true);
    commandTabs->setElideMode(Qt::ElideNone);
    commandTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto page = [&](QString name, QStringList ids) {
        auto *bar = new QToolBar;
        bar->setObjectName("commands" + name);
        bar->setMovable(false);
        bar->setIconSize({22, 22});
        bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        for (auto &id : ids) {
            if (id.isEmpty())
                bar->addSeparator();
            else
                bar->addAction(commands.get(id));
        }
        commandTabs->addTab(bar, name);
    };
    page("Features", {"sketch", "sketch.edit", "", "extrude", "revolve", "", "fillet", "chamfer",
                      "hole", "pattern", "circularPattern", "mirror"});
    page("Sketch", {"sketch",
                    "sketch.edit",
                    "",
                    "sketch.tool.Select / drag",
                    "sketch.tool.Line",
                    "sketch.tool.Rectangle",
                    "sketch.tool.Slot",
                    "sketch.tool.Ellipse",
                    "sketch.tool.Spline",
                    "sketch.tool.Trim / extend",
                    "sketch.tool.Circle",
                    "sketch.tool.Arc",
                    "sketch.tool.Polyline",
                    "sketch.tool.Point",
                    "",
                    "sketch.dimension",
                    "sketch.delete",
                    "sketch.grid",
                    "",
                    "sketch.finish"});
    auto *sketchBar = commandTabs->findChild<QToolBar *>("commandsSketch");
    auto *arcButton =
        qobject_cast<QToolButton *>(sketchBar->widgetForAction(commands.get("sketch.tool.Arc")));
    auto *ellipseButton = qobject_cast<QToolButton *>(
        sketchBar->widgetForAction(commands.get("sketch.tool.Ellipse")));
    auto *ellipseMenu = new QMenu(ellipseButton);
    for (auto name : {"Ellipse", "EllipticalArc", "Parabola"}) {
        auto action = commands.get("sketch.tool." + QString(name));
        ellipseMenu->addAction(action);
        connect(action, &QAction::triggered, ellipseButton,
                [ellipseButton, action] { ellipseButton->setDefaultAction(action); });
    }
    ellipseButton->setMenu(ellipseMenu);
    ellipseButton->setPopupMode(QToolButton::MenuButtonPopup);
    ellipseButton->setObjectName("sketchConicFlyout");
    auto *arcMenu = new QMenu(arcButton);
    for (auto name : {"Arc", "3 Point Arc", "Tangent Arc"}) {
        auto *action = commands.get("sketch.tool." + QString(name));
        arcMenu->addAction(action);
        connect(action, &QAction::triggered, arcButton,
                [arcButton, action] { arcButton->setDefaultAction(action); });
    }
    connect(sketchPanel, &SketchPanel::toolChanged, arcButton, [this, arcButton](QString name) {
        if (name == "Arc" || name == "3 Point Arc" || name == "Tangent Arc")
            arcButton->setDefaultAction(commands.get("sketch.tool." + name));
    });
    arcButton->setObjectName("sketchArcFlyout");
    arcButton->setMenu(arcMenu);
    arcButton->setPopupMode(QToolButton::MenuButtonPopup);
    page("Evaluate",
         {"measure", "fit", "", "view.Isometric", "view.Front", "view.Top", "view.Right"});
    toolbar->addWidget(commandTabs);
    auto *quick = findChild<QToolBar *>("workspaceToolbar");
    quick->insertAction(commands.get("fit"), commands.get("undo"));
    quick->insertAction(commands.get("fit"), commands.get("redo"));
    quick->removeAction(commands.get("measure"));
    auto *rail = new QToolBar("Task Pane", this);
    rail->setObjectName("taskPaneRail");
    rail->setMovable(false);
    rail->setIconSize({20, 20});
    addToolBar(Qt::RightToolBarArea, rail);
    auto *resources = findChild<QDockWidget *>("resourcesDock");
    resources->toggleViewAction()->setIcon(commands.get("view.Top")->icon());
    resources->toggleViewAction()->setToolTip("Show or hide navigation help");
    rail->addAction(resources->toggleViewAction());
    commands.get("fit")->setShortcutContext(Qt::WidgetShortcut);
    sceneView()->addAction(commands.get("fit"));
    sketchPanel->canvas()->addAction(commands.get("fit"));
    commands.get("measure")->setShortcutContext(Qt::WidgetShortcut);
    sceneView()->addAction(commands.get("measure"));
    editContext = new QLabel;
    editContext->setObjectName("editContext");
    editContext->setContentsMargins(12, 0, 8, 0);
    statusBar()->addPermanentWidget(editContext);
    connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        if (item->data(0, Qt::UserRole + 1).toString() != state().activeDocument ||
            client->outstanding())
            return;
        auto sketch = item->data(0, Qt::UserRole + 3).toString();
        auto plane = item->data(0, Qt::UserRole + 4).toString();
        if (!sketch.isEmpty()) {
            sketchPanel->chooseSketch(sketch);
            sketchPanel->editSelected();
        } else if (!plane.isEmpty() && !sketchPanel->active()) {
            sketchPanel->choosePlane(plane);
            sketchPanel->begin();
        }
    });
    setStyleSheet("QToolBar { spacing: 5px; padding: 4px; border: 0; } QToolButton { padding: 5px; "
                  "border-radius: 4px; } QToolButton:checked { background: palette(highlight); "
                  "color: palette(highlighted-text); } "
                  "QLineEdit { padding: 3px; } QDockWidget::title { padding: 5px; } QGroupBox { "
                  "margin-top: 8px; padding-top: 10px; }");
}
void MainWindow::refreshCommandTabs() {
    if (!commandTabs || !sketchPanel)
        return;
    bool part = false;
    QString unit;
    for (auto value : state().documentRecords) {
        auto d = value.toMap();
        if (d.value("id").toString() == state().activeDocument) {
            part = d.value("type").toString() == "Part";
            unit = d.value("units").toString();
        }
    }
    bool editing = part && sketchPanel->active(), idle = client->ready() && !client->outstanding();
    auto available = [&](QString id, bool enabled, QString why) {
        auto *a = commands.get(id);
        a->setEnabled(enabled);
        a->setProperty("unavailableReason", enabled ? QString() : why);
        a->setToolTip(enabled ? a->text() : a->text() + " — " + why);
    };
    for (auto name : {"Select / drag", "Line", "Rectangle", "Circle", "Arc", "3 Point Arc",
                      "Tangent Arc", "Slot", "Ellipse", "EllipticalArc", "Parabola", "Spline",
                      "Polyline", "Point", "Trim / extend"}) {
        auto *a = commands.get("sketch.tool." + QString(name));
        available(a->objectName(), editing && idle, "Start or edit a sketch");
        if (editing && idle && QString(name) == "3 Point Arc")
            a->setToolTip("3 Point Arc — click start/end, move to bend, click to finish");
        if (editing && idle && QString(name) == "Tangent Arc")
            a->setToolTip("Tangent Arc — choose a line/arc endpoint, move or drag to size");
        if (QString(name) == "Arc" && editing && idle)
            a->setToolTip("Centerpoint Arc — click center, move/click start, move/click end");
        if ((QString(name) == "Line" || QString(name) == "Polyline") && editing && idle)
            a->setToolTip(QString(name) + " — click vertices; move away and return to the last "
                                          "endpoint for Tangent Arc; click tool again to select");
        a->setChecked(editing && sketchPanel->toolName() == name);
    }
    available("sketch.edit", part && idle && !editing && !sketchPanel->sketchId().isEmpty(),
              "Select an existing Part sketch");
    available("sketch.finish", editing && idle, "No sketch edit is active");
    available("sketch.cancel",
              editing && findChild<QPushButton *>("cancelSketchButton")->isEnabled(),
              "No cancelable sketch edit is active");
    available("sketch.dimension", editing && idle && sketchPanel->dimensionAvailable(),
              "Select a line, circle or arc in an active sketch");
    available("sketch.delete", editing && idle && !sketchPanel->selectedEntity().isEmpty(),
              "Select geometry in an active sketch");
    available("sketch.grid", part, "Open a Part document");
    for (auto name : {"Isometric", "Front", "Top", "Right"})
        available("view." + QString(name), bool(client->mesh()) && idle && !editing,
                  "Load a 3D reference scene");
    if (editing && !wasSketchEditing) {
        commandTabs->setCurrentIndex(1);
        findChild<QTabWidget *>("managerTabs")->setCurrentIndex(1);
    } else if (!editing && wasSketchEditing)
        findChild<QTabWidget *>("managerTabs")->setCurrentIndex(0);
    wasSketchEditing = editing;
    editContext->setText(unit + (editing ? " · Editing " + sketchPanel->sketchName()
                                 : part  ? " · Part"
                                         : ""));
    propertyPages->setCurrentIndex(state().projectOpen ? (part ? 1 : 2) : 0);
    refreshFeatures();
    refreshAssemblyUI();
    refreshDrawingUI();
}
} // namespace cad

#include "MainWindow.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QCloseEvent>
#include <QDockWidget>
#include <QFormLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
namespace cad {
MainWindow::MainWindow(QString settingsFile, QWidget *parent, QString recoveryRoot)
    : QMainWindow(parent) {
    setObjectName("cadMainWindow");
    setWindowTitle("CAD Workbench · Native reference workspace");
    resize(1280, 800);
    settings = settingsFile.isEmpty()
                   ? std::make_unique<QSettings>()
                   : std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
    client = new Client(this, {}, recoveryRoot);
    buildFrame();
    buildProjectUI();
    connect(client, &Client::changed, this, &MainWindow::refresh);
    connect(client, &Client::changed, this, [this] {
        if (!recoveryChecked && client->ready()) {
            recoveryChecked = true;
            if (settings->value("startup/offerRecovery", true).toBool())
                QTimer::singleShot(0, this, [this] { client->listRecoveries(); });
        }
    });
    connect(client, &Client::projectChanged, this, &MainWindow::refreshProjectUI);
    connect(client, &Client::meshChanged, this, [this](MeshPtr mesh) {
        viewport->setMesh(mesh);
        if (mesh) {
            widthInput->setValue(mesh->maximum.x() - mesh->minimum.x());
            heightInput->setValue(mesh->maximum.y() - mesh->minimum.y());
            depthInput->setValue(mesh->maximum.z() - mesh->minimum.z());
        }
        rebuildTree();
    });
    connect(client, &Client::measured, this, [this](QString text) { notice->setText(text); });
    connect(viewport, &Viewport::selected, this, &MainWindow::choose);
    connect(viewport, &Viewport::graphicsFailed, this,
            [this](QString reason) { notice->setText(reason); });
    restoreGeometry(settings->value("window/geometry").toByteArray());
    restoreState(settings->value("window/layout").toByteArray(), 1);
    refresh();
    client->start();
}
MainWindow::~MainWindow() = default;
const WorkspaceState &MainWindow::state() const {
    return client->state();
}
void MainWindow::buildFrame() {
    auto *file = menuBar()->addMenu("&File");
    auto *view = menuBar()->addMenu("&View");
    auto *inspect = menuBar()->addMenu("&Inspect");
    auto *tools = menuBar()->addMenu("&Tools");
    auto add = [&](QString id, QString label, QMenu *menu, QKeySequence shortcut = {},
                   QString unavailable = {}) {
        auto *a = commands.add(this, id, label, shortcut.toString(), unavailable);
        menu->addAction(a);
        addAction(a);
        return a;
    };
    add("new", "New project", file, QKeySequence::New);
    add("open", "Open project", file, QKeySequence::Open);
    add("save", "Save", file, QKeySequence::Save);
    auto *load = add("reference", "Load reference box", file);
    connect(load, &QAction::triggered, this, [this] { client->loadReference(); });
    auto *instances = add("instances", "Load 100 reference instances", file);
    connect(instances, &QAction::triggered, this, [this] { client->loadReference(100); });
    auto *close = add("close", "Close reference", file, QKeySequence::Close);
    connect(close, &QAction::triggered, this, [this] {
        if (state().projectOpen) {
            if (confirmRecoverable("Close document"))
                client->closeDocument();
        } else
            client->closeReference();
    });
    add("sketch", "Create sketch", tools, {}, "Requires Module 3 sketch implementation.");
    add("extrude", "Extrude", tools, {}, "Requires Module 4 feature implementation.");
    auto *fit = add("fit", "Zoom to fit", view, QKeySequence("F"));
    connect(fit, &QAction::triggered, viewport = new Viewport(this), &Viewport::fit);
    for (const auto &name : {"Isometric", "Front", "Top", "Right"}) {
        auto *a = add(QString("view.") + name, name, view);
        connect(a, &QAction::triggered, this, [this, name] { viewport->setView(name); });
    }
    auto *measure = add("measure", "Measure selection", inspect, QKeySequence("M"));
    connect(measure, &QAction::triggered, client, &Client::measure);
    auto *restart = add("restart", "Restart worker", tools);
    connect(restart, &QAction::triggered, client, &Client::restart);
    auto *query = add("outcome", "Query last operation outcome", tools);
    connect(query, &QAction::triggered, this,
            [this] { client->queryOperation(client->latestOperation()); });
    auto *toolbar = addToolBar("Workspace commands");
    toolbar->setObjectName("workspaceToolbar");
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    for (const auto &id : {"new", "open", "save", "fit", "measure"})
        toolbar->addAction(commands.get(id));
    search = new QLineEdit;
    search->setObjectName("commandSearch");
    search->setPlaceholderText("Find command…");
    search->setClearButtonEnabled(true);
    toolbar->addWidget(search);
    auto *results = new QMenu(search);
    connect(search, &QLineEdit::textEdited, this, [this, results](QString query) {
        results->clear();
        if (query.trimmed().isEmpty()) {
            results->hide();
            return;
        }
        for (auto *action : commands.all())
            if (action->text().contains(query, Qt::CaseInsensitive))
                results->addAction(action);
        if (!results->isEmpty())
            results->popup(search->mapToGlobal(QPoint(0, search->height())));
        search->setFocus();
    });
    auto *center = new QWidget;
    auto *layout = new QVBoxLayout(center);
    layout->setContentsMargins(0, 0, 0, 0);
    documents = new QTabBar;
    documents->setObjectName("documentTabs");
    documents->addTab("Welcome");
    documents->addTab("Reference scene");
    documents->setTabEnabled(1, false);
    auto *welcome = new QLabel("<h2>Native CAD Workbench</h2><p>Load a reference box to navigate, "
                               "select, and measure exact worker geometry.</p><p>The reference "
                               "scene is temporary. Project documents begin in Module 2.</p>");
    welcome->setAlignment(Qt::AlignCenter);
    welcome->setObjectName("welcomePanel");
    layout->addWidget(welcome, 1);
    layout->addWidget(viewport, 1);
    layout->addWidget(documents);
    setCentralWidget(center);
    connect(documents, &QTabBar::currentChanged, this, [this](int index) {
        if (syncing)
            return;
        if (state().projectOpen) {
            auto id = documents->tabData(index).toString();
            if (!id.isEmpty() && id != state().activeDocument)
                client->switchDocument(id);
            return;
        }
        viewport->setVisible(index == 1);
        findChild<QLabel *>("welcomePanel")->setVisible(index == 0);
    });
    auto *dock = new QDockWidget("Model and properties", this);
    dock->setObjectName("modelDock");
    dock->setMinimumWidth(240);
    auto *tabs = new QTabWidget;
    tabs->setObjectName("managerTabs");
    tree = new QTreeWidget;
    tree->setObjectName("modelTree");
    tree->setHeaderLabel("Reference model");
    tabs->addTab(tree, "Model");
    connect(tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (syncing)
            return;
        auto items = tree->selectedItems();
        if (!items.empty() && state().projectOpen) {
            auto id = items.first()->data(0, Qt::UserRole + 1).toString();
            if (!id.isEmpty() && id != state().activeDocument)
                client->switchDocument(id);
            return;
        }
        if (!items.empty())
            choose(items.first()->data(0, Qt::UserRole).value<Selection>());
    });
    auto *properties = new QWidget;
    auto *form = new QFormLayout(properties);
    form->addRow(new QLabel("Temporary dimensions (mm)"));
    auto spin = [&](QString name, double value) {
        auto *s = new QDoubleSpinBox;
        s->setObjectName(name);
        s->setRange(1, 1000);
        s->setDecimals(2);
        s->setValue(value);
        return s;
    };
    widthInput = spin("widthInput", 10);
    heightInput = spin("heightInput", 20);
    depthInput = spin("depthInput", 30);
    form->addRow("Width", widthInput);
    form->addRow("Height", heightInput);
    form->addRow("Depth", depthInput);
    auto *preview = new QPushButton("Preview dimensions");
    preview->setObjectName("previewButton");
    form->addRow(preview);
    connect(preview, &QPushButton::clicked, this, [this] {
        client->previewReference(widthInput->value(), heightInput->value(), depthInput->value());
    });
    auto *cancel = new QPushButton("Cancel preview");
    cancel->setObjectName("cancelPreviewButton");
    form->addRow(cancel);
    connect(cancel, &QPushButton::clicked, client, &Client::cancelPreview);
    auto *accept = new QPushButton("Accept feature");
    accept->setEnabled(false);
    accept->setToolTip("Requires Module 4 feature transactions.");
    form->addRow(accept);
    tabs->addTab(properties, "Properties");
    dock->setWidget(tabs);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    view->addAction(dock->toggleViewAction());
    auto *resources = new QDockWidget("Resources", this);
    resources->setObjectName("resourcesDock");
    auto *info = new QLabel("Native workbench\n\nReference scene only\n\nProject persistence: "
                            "Module 2\nSketching: Module 3\nFeatures: Module 4");
    info->setMargin(12);
    resources->setWidget(info);
    addDockWidget(Qt::RightDockWidgetArea, resources);
    resources->hide();
    view->addAction(resources->toggleViewAction());
    auto *controls = addToolBar("Viewport controls");
    controls->setObjectName("viewportToolbar");
    addToolBar(Qt::BottomToolBarArea, controls);
    filter = new QComboBox;
    filter->setObjectName("selectionFilter");
    filter->addItems({"Face", "Edge", "Body", "Component"});
    controls->addWidget(new QLabel(" Selection: "));
    controls->addWidget(filter);
    connect(filter, &QComboBox::currentTextChanged, viewport, &Viewport::setFilter);
    viewMode = new QComboBox;
    viewMode->setObjectName("viewMode");
    viewMode->addItems({"Shaded with edges", "Shaded", "Wireframe", "Hidden edges"});
    controls->addWidget(new QLabel(" Display: "));
    controls->addWidget(viewMode);
    connect(viewMode, &QComboBox::currentTextChanged, viewport, &Viewport::setMode);
    notice = new QLabel("Load a reference box to inspect native geometry.");
    notice->setObjectName("notification");
    statusBar()->addWidget(notice, 1);
    status = new QLabel;
    status->setObjectName("workspaceStatus");
    statusBar()->addPermanentWidget(status);
}
void MainWindow::refresh() {
    const auto &s = state();
    bool idle = client->ready() && client->outstanding() == 0;
    for (auto id : {"new", "open", "recover"})
        if (commands.get(id))
            commands.get(id)->setEnabled(idle);
    commands.get("save")->setEnabled(s.projectOpen && idle);
    if (!s.projectOpen) {
        commands.get("save")->setToolTip(
            "Open or create a project before saving; reference scenes are temporary");
        commands.get("save")->setProperty(
            "unavailableReason",
            "Reference scenes are temporary; open or create a project to save");
    } else {
        commands.get("save")->setProperty("unavailableReason", QString());
        commands.get("save")->setToolTip("Publish a standalone project snapshot");
    }
    if (s.projectOpen) {
        refreshProjectUI();
        return;
    }
    if (documents->count() != 2 || documents->tabText(0) != "Welcome") {
        syncing = true;
        while (documents->count())
            documents->removeTab(0);
        documents->addTab("Welcome");
        documents->addTab("Reference scene");
        syncing = false;
    }
    documents->setEnabled(true);
    commands.get("close")->setText("Close reference");
    findChild<QLabel *>("welcomePanel")
        ->setText("<h2>Native CAD Workbench</h2><p>Create or open a project, or load a temporary "
                  "reference scene.</p>");
    for (auto id : {"reference", "instances", "fit", "measure"})
        commands.get(id)->setEnabled(idle);
    for (auto id : {"document.new", "saveAs", "undo", "redo", "project.close"})
        if (commands.get(id))
            commands.get(id)->setEnabled(false);
    findChild<QLabel *>("welcomePanel")->setVisible(documents->currentIndex() == 0);
    viewport->setVisible(documents->currentIndex() == 1);
    status->setText(s.job + " · " + s.save);
    notice->setText(s.message);
    bool loaded = bool(client->mesh());
    documents->setTabEnabled(1, loaded);
    if (loaded && documents->currentIndex() != 1)
        documents->setCurrentIndex(1);
    if (!loaded)
        documents->setCurrentIndex(0);
    viewport->setStale(s.stale);
    viewport->setSelection(s.selection);
    commands.get("measure")->setEnabled(client->ready() && loaded && !s.stale &&
                                        !s.selection.empty());
    commands.get("close")->setEnabled(loaded);
    widthInput->setEnabled(loaded);
    heightInput->setEnabled(loaded);
    depthInput->setEnabled(loaded);
    syncing = true;
    for (auto *item : tree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive)) {
        item->setSelected(item->data(0, Qt::UserRole).value<Selection>() == s.selection &&
                          !s.selection.empty());
    }
    syncing = false;
}
void MainWindow::rebuildTree() {
    if (state().projectOpen) {
        refreshProjectUI();
        return;
    }
    syncing = true;
    tree->clear();
    if (auto mesh = client->mesh()) {
        auto *root = new QTreeWidgetItem(tree, {"Reference scene (temporary)"});
        for (const auto &instance : mesh->instances) {
            auto *body = new QTreeWidgetItem(root, {instance.id});
            body->setData(
                0, Qt::UserRole,
                QVariant::fromValue(Selection{"Body", "body/reference-box", instance.id}));
            for (const auto &face : mesh->faceIds) {
                auto *item = new QTreeWidgetItem(body, {face});
                item->setData(0, Qt::UserRole,
                              QVariant::fromValue(Selection{"Face", face, instance.id}));
            }
            for (const auto &edge : mesh->edgeIds) {
                auto *item = new QTreeWidgetItem(body, {edge});
                item->setData(0, Qt::UserRole,
                              QVariant::fromValue(Selection{"Edge", edge, instance.id}));
            }
        }
        root->setExpanded(true);
        if (mesh->instances.size() == 1)
            root->child(0)->setExpanded(true);
    }
    syncing = false;
}
void MainWindow::choose(const Selection &selection) {
    client->select(selection);
}
void MainWindow::saveLayout() {
    settings->setValue("window/geometry", saveGeometry());
    settings->setValue("window/layout", saveState(1));
    settings->sync();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (state().projectOpen && client->outstanding()) {
        QMessageBox question(QMessageBox::Warning, "Operation outcome is uncertain",
                             "Closing now stops the worker. A pending operation may already be "
                             "durable. Recover and query its outcome on restart.",
                             QMessageBox::Cancel, this);
        auto *stop = question.addButton("Stop and keep recovery", QMessageBox::AcceptRole);
        question.exec();
        if (question.clickedButton() != stop) {
            event->ignore();
            return;
        }
    }
    if (state().projectOpen && !confirmRecoverable("Close application")) {
        event->ignore();
        return;
    }
    saveLayout();
    QMainWindow::closeEvent(event);
}
} // namespace cad

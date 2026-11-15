#include "AssemblyPanel.hpp"
#include "FeaturePanel.hpp"
#include "HistoryPanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/Viewport.hpp"
#include <QMenuBar>
#include <QToolBar>
namespace cad {
void MainWindow::buildAssemblyUI() {
    assemblyPanel = new AssemblyPanel(client);
    propertyPages->addWidget(assemblyPanel);
    auto *action = commands.add(this, "assembly", "Components / mates");
    addAction(action);
    menuBar()->actions().last()->menu()->addAction(action);
    auto *toolbar = new QToolBar;
    toolbar->setObjectName("commandsAssembly");
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addAction(action);
    commandTabs->addTab(toolbar, "Assembly");
    connect(action, &QAction::triggered, this, [this] {
        exchangeShown = false;
        findChild<QTabWidget *>("managerTabs")->setCurrentIndex(1);
        propertyPages->setCurrentWidget(assemblyPanel);
    });
    connect(viewport, &Viewport::componentDragged, assemblyPanel, &AssemblyPanel::drag);
    connect(assemblyPanel, &AssemblyPanel::editingChanged, this, &MainWindow::refresh);
}
void MainWindow::refreshAssemblyUI() {
    if (!assemblyPanel)
        return;
    QCborMap active;
    for (auto v : state().documentRecords)
        if (v.toMap().value("id").toString() == state().activeDocument)
            active = v.toMap();
    bool assembly = active.value("type").toString() == "Assembly",
         idle = client->ready() && !client->outstanding();
    commands.get("assembly")->setEnabled(assembly);
    auto assemblyTab = commandTabs->indexOf(findChild<QToolBar *>("commandsAssembly"));
    commandTabs->setTabEnabled(assemblyTab, assembly);
    viewport->setComponentDrag(assembly);
    if (!assembly)
        return;
    assemblyPanel->install(active, state().documentRecords, idle);
    propertyPages->setCurrentWidget(assemblyPanel);
    findChild<QTabWidget *>("managerTabs")->setCurrentIndex(1);
    if (commandTabs->currentIndex() != assemblyTab)
        commandTabs->setCurrentIndex(assemblyTab);
    viewport->setVisible(bool(client->mesh()));
    viewport->setContext(active.value("units").toString() + " · " +
                         active.value("name").toString() + " · Assembly");
    viewport->setSelection(state().selection);
    viewport->setStale(state().stale);
    findChild<QLabel *>("welcomePanel")->setVisible(!client->mesh());
    findChild<QToolBar *>("viewportToolbar")->setVisible(bool(client->mesh()));
    commands.get("fit")->setEnabled(idle && bool(client->mesh()));
    commands.get("measure")->setEnabled(false);
    if (assemblyPanel->active()) {
        for (auto id : {"new", "open", "save", "saveAs", "document.new", "project.close", "close",
                        "undo", "redo", "restart"})
            commands.get(id)->setEnabled(false);
        documents->setEnabled(false);
        tree->setEnabled(false);
    }
}
} // namespace cad

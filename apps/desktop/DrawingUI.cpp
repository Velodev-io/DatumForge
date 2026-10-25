#include "DrawingPanel.hpp"
#include "MainWindow.hpp"
#include "render/Viewport.hpp"
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>
#include <QVBoxLayout>
namespace cad {
void MainWindow::buildDrawingUI() {
    drawingCanvas = new DrawingCanvas;
    drawingCanvas->hide();
    qobject_cast<QVBoxLayout *>(centralWidget()->layout())->insertWidget(2, drawingCanvas, 1);
    drawingPanel = new DrawingPanel(client, drawingCanvas);
    propertyPages->addWidget(drawingPanel);
    auto *bar = new QToolBar;
    bar->setObjectName("commandsDrawing");
    bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    auto *menu = menuBar()->addMenu("&Drawing");
    for (auto pair :
         {std::pair{"drawing.edit", "Edit drawing"}, std::pair{"drawing.update", "Update views"},
          std::pair{"drawing.pdf", "Export vector PDF"}}) {
        auto *a = commands.add(this, pair.first, pair.second);
        bar->addAction(a);
        menu->addAction(a);
        addAction(a);
    }
    commandTabs->addTab(bar, "Drawing");
    connect(commands.get("drawing.edit"), &QAction::triggered, drawingPanel, &DrawingPanel::begin);
    connect(commands.get("drawing.update"), &QAction::triggered, this,
            [this] { client->projectCommand("UpdateDrawing"); });
    connect(commands.get("drawing.pdf"), &QAction::triggered, this, [this] {
        auto path =
            QFileDialog::getSaveFileName(this, "Export vector drawing PDF", {}, "PDF (*.pdf)");
        if (path.isEmpty())
            return;
        if (!path.endsWith(".pdf", Qt::CaseInsensitive))
            path += ".pdf";
        client->exportDrawing(path, true);
    });
    connect(drawingPanel, &DrawingPanel::editingChanged, this, &MainWindow::refresh);
}
void MainWindow::refreshDrawingUI() {
    if (!drawingPanel)
        return;
    QCborMap record;
    for (auto v : state().documentRecords)
        if (v.toMap().value("id").toString() == state().activeDocument)
            record = v.toMap();
    bool drawing = record.value("type") == "Drawing",
         idle = client->ready() && !client->outstanding();
    drawingCanvas->setVisible(drawing);
    int tab = commandTabs->indexOf(findChild<QToolBar *>("commandsDrawing"));
    commandTabs->setTabEnabled(tab, drawing);
    for (auto id : {"drawing.edit", "drawing.update", "drawing.pdf"}) {
        auto *a = commands.get(id);
        bool enabled = drawing && idle && !drawingPanel->active() &&
                       (QString(id) != "drawing.pdf" || !state().stale);
        a->setEnabled(enabled);
        a->setProperty(
            "unavailableReason",
            enabled ? QString()
                    : "Open a Drawing; finish drafts and update/repair views before publication");
        a->setToolTip(enabled ? a->text() : a->property("unavailableReason").toString());
    }
    if (!drawing) {
        drawingPanel->install({}, state().documentRecords, idle);
        return;
    }
    drawingPanel->install(record, state().documentRecords, idle);
    findChild<QAction *>("cancelFeatureAction")->setEnabled(drawingPanel->active());
    propertyPages->setCurrentWidget(drawingPanel);
    findChild<QTabWidget *>("managerTabs")->setCurrentIndex(1);
    commandTabs->setCurrentIndex(tab);
    viewport->hide();
    findChild<QLabel *>("welcomePanel")->hide();
    findChild<QToolBar *>("viewportToolbar")->hide();
    if (drawingPanel->active()) {
        for (auto id : {"new", "open", "save", "saveAs", "document.new", "project.close", "close",
                        "undo", "redo", "restart"})
            commands.get(id)->setEnabled(false);
        documents->setEnabled(false);
        tree->setEnabled(false);
    }
}
} // namespace cad

#include "FeaturePanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "render/Viewport.hpp"
#include <QDockWidget>
#include <QToolBar>
namespace cad {
void MainWindow::refreshFeatures() {
    if (!featurePanel)
        return;
    QCborMap active;
    for (auto value : state().documentRecords) {
        auto d = value.toMap();
        if (d.value("id").toString() == state().activeDocument)
            active = d;
    }
    bool idle = client->ready() && !client->outstanding(),
         part = active.value("type").toString() == "Part";
    bool sketchEdit = sketchPanel && sketchPanel->active(), edit = featurePanel->active();
    featurePanel->install(active, idle);
    auto available = [&](QString id, bool yes, QString reason) {
        auto *a = commands.get(id);
        a->setEnabled(yes);
        a->setProperty("unavailableReason", yes ? QString() : reason);
        a->setToolTip(a->text() + (yes ? QString() : " — " + reason));
    };
    bool sketches = !active.value("sketches").toArray().empty(),
         solids = !active.value("features").toArray().empty();
    for (auto id : {"extrude", "revolve", "fillet", "chamfer", "hole", "pattern", "circularPattern",
                    "mirror"}) {
        bool profile = QString(id) == "extrude" || QString(id) == "revolve";
        available(id, part && idle && !sketchEdit && !edit && (profile ? sketches : solids),
                  profile ? "Accept a closed Part sketch first"
                          : "Requires an accepted Part solid and no active command");
    }
    findChild<QAction *>("cancelFeatureAction")->setEnabled(edit);
    if (!part)
        return;
    viewport->setContext(active.value("units").toString() + " · " +
                         active.value("name").toString() + " · " +
                         (edit ? "Feature draft" : QString("Revision %1").arg(state().revision)));
    bool showSolid = (solids || edit) && !sketchEdit;
    sketchPanel->setVisible(!showSolid);
    viewport->setVisible(showSolid && bool(client->mesh()));
    findChild<QToolBar *>("viewportToolbar")->setVisible(showSolid);
    if (showSolid) {
        findChild<QLabel *>("welcomePanel")->hide();
        viewport->setSelection(state().selection);
        viewport->setStale(state().stale);
    }
    if (showSolid)
        propertyPages->setCurrentWidget(featurePanel);
    if (edit) {
        findChild<QTabWidget *>("managerTabs")->setCurrentIndex(1);
        commandTabs->setCurrentIndex(0);
        for (auto id : {"new", "open", "save", "saveAs", "document.new", "project.close", "close",
                        "undo", "redo", "sketch", "sketch.edit", "restart"})
            commands.get(id)->setEnabled(false);
        tree->setEnabled(false);
        documents->setEnabled(false);
        findChild<QWidget *>("documentProperties")->setEnabled(false);
        editContext->setText(active.value("units").toString() + " · Feature draft");
    }
    available("measure", showSolid && idle && !edit && bool(client->mesh()) && !state().stale,
              "Requires a current exact solid");
    available("fit", idle && (showSolid ? bool(client->mesh()) : true), "Wait for geometry");
}
} // namespace cad

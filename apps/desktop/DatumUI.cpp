#include "AssemblyPanel.hpp"
#include "DrawingPanel.hpp"
#include "FeaturePanel.hpp"
#include "HistoryPanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "SupportEditor.hpp"
#include "domain/Documents.hpp"
#include "model/Supports.hpp"
#include "render/Viewport.hpp"
#include <QDockWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
namespace cad {
QString MainWindow::datumKey() const {
    return state().epoch + "/" + state().context.project + "/" + state().activeDocument + "/" +
           QString::number(state().revision);
}
void MainWindow::buildDatumUI() {
    auto *dock = new QDockWidget("Reference geometry", this);
    dock->setObjectName("datumDock");
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *help = new QLabel("Select a plane or sketch for Normal To. Check a row to show its "
                            "reference geometry. Visibility lasts for this project session.");
    help->setWordWrap(true);
    layout->addWidget(help);
    datumTree = new QTreeWidget;
    datumTree->setObjectName("datumBrowser");
    datumTree->setAccessibleName("Datum selection and visibility");
    datumTree->setHeaderLabel("Reference geometry · visibility");
    layout->addWidget(datumTree, 1);
    datumInfo = new QLabel;
    datumInfo->setObjectName("datumInformation");
    datumInfo->setTextFormat(Qt::PlainText);
    datumInfo->setWordWrap(true);
    layout->addWidget(datumInfo);
    auto *normal = commands.add(this, "view.normalTo", "Normal To", "Ctrl+8");
    normal->setProperty("aliases", "plane sketch perpendicular orient");
    addAction(normal);
    auto *button = new QPushButton(normal->text());
    button->setObjectName("datumNormalTo");
    layout->addWidget(button);
    connect(button, &QPushButton::clicked, normal, &QAction::trigger);
    connect(normal, &QAction::changed, button, [normal, button] {
        button->setEnabled(normal->isEnabled());
        button->setToolTip(normal->toolTip());
    });
    connect(normal, &QAction::triggered, this, [this] {
        if (!commands.get("view.normalTo")->isEnabled())
            return;
        auto found = datumGeometry.constFind(selectedDatum);
        if (found != datumGeometry.cend() && found->kind == "DatumPlane")
            viewport->normalTo(found->frame);
    });
    auto *refresh = commands.add(this, "datum.refresh", "Refresh datum geometry");
    addAction(refresh);
    auto *reload = new QPushButton(refresh->text());
    reload->setObjectName("datumRefresh");
    layout->addWidget(reload);
    connect(reload, &QPushButton::clicked, refresh, &QAction::trigger);
    connect(refresh, &QAction::changed, reload,
            [refresh, reload] { reload->setEnabled(refresh->isEnabled()); });
    connect(refresh, &QAction::triggered, this, [this] {
        datumRequestedKey.clear();
        datumResultKey.clear();
        acceptedDatums.clear();
        refreshDatumUI();
    });
    dock->setWidget(page);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
    auto *show = commands.add(this, "datum.browser", "Reference geometry");
    show->setCheckable(true);
    show->setProperty("aliases", "datum plane axis visibility show hide");
    addAction(show);
    connect(show, &QAction::toggled, dock, &QDockWidget::setVisible);
    connect(dock, &QDockWidget::visibilityChanged, show, &QAction::setChecked);
    auto *viewMenu = menuBar()->addMenu("&Reference geometry");
    for (auto *action : {show, normal, refresh})
        viewMenu->addAction(action);
    auto *edit = commands.add(this, "supports.edit", "Edit datums / supports");
    edit->setProperty("aliases", "plane axis attach Convert Entities external repair");
    addAction(edit);
    viewMenu->addAction(edit);
    findChild<QToolBar *>("commandsFeatures")->addAction(edit);
    connect(edit, &QAction::triggered, this, [this] {
        if (!commands.get("supports.edit")->isEnabled())
            return;
        for (auto value : state().documentRecords)
            if (value.toMap().value("id").toString() == state().activeDocument) {
                try {
                    SupportEditor dialog(client, value.toMap(), this, selectedDatum);
                    dialog.exec();
                    this->refresh();
                } catch (const std::exception &e) {
                    notice->setText(e.what());
                }
                break;
            }
    });
    findChild<QToolBar *>("commandsEvaluate")->addAction(normal);
    findChild<QToolBar *>("commandsFeatures")->addAction(show);
    connect(datumTree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (!syncingDatums && datumTree->currentItem())
            selectDatum(datumTree->currentItem()->data(0, Qt::UserRole).toString());
    });
    connect(datumTree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int) {
        if (!syncingDatums)
            setDatumVisible(item->data(0, Qt::UserRole).toString(),
                            item->checkState(0) == Qt::Checked);
    });
    connect(tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (syncing || syncingDatums)
            return;
        auto *item = tree->currentItem();
        selectedDatum = item && item->data(0, Qt::UserRole + 1).toString() == state().activeDocument
                            ? item->data(0, Qt::UserRole + 6).toString()
                            : QString();
        refreshDatumUI();
    });
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int) {
        if (!syncing && !syncingDatums &&
            item->data(0, Qt::UserRole + 1).toString() == state().activeDocument)
            setDatumVisible(item->data(0, Qt::UserRole + 6).toString(),
                            item->checkState(0) == Qt::Checked);
    });
    connect(client, &Client::supportGeometry, this, [this](QCborMap result) {
        if (result.value("document").toString() != state().activeDocument ||
            result.value("revision").toInteger() != state().revision ||
            datumRequestedKey != datumKey())
            return;
        acceptedDatums = result.value("datums").toArray();
        datumResultKey = datumKey();
        refreshDatumUI();
    });
}
void MainWindow::selectDatum(QString id) {
    if (!datumLabels.contains(id))
        return;
    selectedDatum = id;
    syncingDatums = true;
    for (auto *item : tree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive))
        if (item->data(0, Qt::UserRole + 1).toString() == state().activeDocument &&
            item->data(0, Qt::UserRole + 6).toString() == id) {
            tree->setCurrentItem(item);
            tree->scrollToItem(item);
            break;
        }
    syncingDatums = false;
    refreshDatumUI();
}
void MainWindow::setDatumVisible(QString id, bool visible) {
    if (id.isEmpty() || !datumLabels.contains(id))
        return;
    datumVisibility[datumScope].insert(id, visible);
    refreshDatumUI();
}
void MainWindow::refreshDatumUI() {
    if (!datumTree || syncingDatums)
        return;
    syncingDatums = true;
    QCborMap active;
    for (auto v : state().documentRecords)
        if (v.toMap().value("id").toString() == state().activeDocument)
            active = v.toMap();
    bool part = state().projectOpen && active.value("type").toString() == "Part";
    bool editing = sketchPanel->active() || featurePanel->active() || historyPanel->active() ||
                   assemblyPanel->active() || drawingPanel->active();
    bool idle = client->ready() && !client->outstanding() && !state().stale;
    auto scope = state().context.project + "/" + state().activeDocument;
    if (scope != datumScope) {
        selectedDatum.clear();
        datumScope = scope;
    }
    if (datumProject != state().context.project) {
        datumVisibility.clear();
        datumProject = state().context.project;
    }
    datumGeometry.clear();
    datumLabels.clear();
    if (part) {
        for (auto plane : {"XY", "XZ", "YZ"}) {
            model::DatumGeometry d;
            d.id = "principal/" + QString(plane);
            d.kind = "DatumPlane";
            if (QString(plane) == "XZ")
                d.frame = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
            if (QString(plane) == "YZ")
                d.frame = {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            datumGeometry.insert(d.id, d);
            datumLabels.insert(d.id, QString(plane) + " Plane");
        }
        for (int i = 0; i < 3; ++i) {
            model::DatumGeometry d;
            d.id = QString("principal/") + QString("XYZ")[i];
            d.kind = "DatumAxis";
            d.direction = {0, 0, 0};
            d.direction[size_t(i)] = 1;
            datumGeometry.insert(d.id, d);
            datumLabels.insert(d.id, QString("XYZ")[i] + QString(" Axis"));
        }
        for (auto &s : sketch::parse(active.value("sketches").toArray())) {
            model::DatumGeometry d;
            d.id = "sketch/" + s.id;
            d.kind = "DatumPlane";
            d.frame = s.frame;
            datumGeometry.insert(d.id, d);
            datumLabels.insert(d.id, "Sketch plane · " + s.name);
        }
        if (active.contains(QStringLiteral("model_supports"))) {
            auto model = model::SupportDefinition::parse(active.value("model_supports").toMap());
            for (auto &datum : model.datums) {
                auto outputs = datum.outputs();
                for (int i = 0; i < outputs.size(); ++i)
                    datumLabels.insert(
                        outputs[i],
                        datum.mode + " " + (datum.kind == "DatumPlane" ? "Plane " : "Axis ") +
                            outputs[i].right(8) +
                            (outputs.size() > 1 ? QString(" (%1/%2)").arg(i + 1).arg(outputs.size())
                                                : QString()));
            }
            if (datumResultKey == datumKey())
                for (auto &d : model::DatumGeometry::parse(acceptedDatums))
                    datumGeometry.insert(d.id, d);
            else if (idle && !editing && datumRequestedKey != datumKey()) {
                datumRequestedKey = datumKey();
                auto key = datumKey();
                QTimer::singleShot(0, this, [this, key] {
                    if (datumKey() == key && client->ready() && !client->outstanding())
                        client->projectCommand("SupportGeometry");
                    else if (datumRequestedKey == key)
                        datumRequestedKey.clear();
                });
            }
        }
    }
    auto visible = [&](QString id) {
        return datumVisibility.value(datumScope).value(id, !id.contains('/'));
    };
    auto projection = QCborValue(sketch::fields({{"labels",
                                                  [&] {
                                                      QCborMap m;
                                                      for (auto it = datumLabels.cbegin();
                                                           it != datumLabels.cend(); ++it)
                                                          m.insert(it.key(), it.value());
                                                      return m;
                                                  }()},
                                                 {"key", datumKey()}}))
                          .toCbor();
    if (datumProjection != projection) {
        datumProjection = projection;
        datumTree->clear();
        for (auto it = datumLabels.cbegin(); it != datumLabels.cend(); ++it) {
            auto *item = new QTreeWidgetItem(datumTree, {it.value()});
            item->setData(0, Qt::UserRole, it.key());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        }
    }
    std::vector<DisplayDatum> display;
    for (auto it = datumGeometry.cbegin(); it != datumGeometry.cend(); ++it)
        display.push_back({it.value(), visible(it.key())});
    viewport->setDatums(part && !editing ? display : std::vector<DisplayDatum>{});
    for (auto *item : datumTree->findItems("*", Qt::MatchWildcard)) {
        auto id = item->data(0, Qt::UserRole).toString();
        item->setCheckState(0, visible(id) ? Qt::Checked : Qt::Unchecked);
        item->setSelected(id == selectedDatum);
        item->setToolTip(0, datumGeometry.contains(id)
                                ? datumLabels.value(id)
                                : "Awaiting exact datum geometry; use Refresh after failure");
    }
    for (auto *item : tree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive)) {
        auto id = item->data(0, Qt::UserRole + 6).toString();
        if (id.isEmpty() || item->data(0, Qt::UserRole + 1).toString() != state().activeDocument)
            continue;
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, visible(id) ? Qt::Checked : Qt::Unchecked);
    }
    bool selected = datumGeometry.contains(selectedDatum);
    bool plane = selected && datumGeometry.value(selectedDatum).kind == "DatumPlane";
    auto *normal = commands.get("view.normalTo");
    normal->setEnabled(part && idle && !editing && plane);
    auto reason =
        selected && !plane
            ? "Select a plane or sketch; an axis has no unique Normal To frame"
            : "Select a current plane or sketch in the model tree or Reference geometry browser";
    normal->setProperty("unavailableReason", normal->isEnabled() ? QString() : reason);
    normal->setToolTip(normal->isEnabled() ? "Align normal to the selected plane; preserve zoom"
                                           : reason);
    commands.get("datum.browser")->setEnabled(part && !editing);
    commands.get("datum.refresh")->setEnabled(part && idle && !editing);
    commands.get("supports.edit")->setEnabled(part && idle && !editing);
    datumTree->setEnabled(part && idle && !editing);
    if (selected) {
        auto d = datumGeometry.value(selectedDatum);
        auto unit = active.value("units").toString();
        double scale = domain::toSI(1, "length", unit);
        auto p = d.frame.origin;
        datumInfo->setText(datumLabels.value(selectedDatum) +
                           QString("\nOrigin: %1, %2, %3 %4\n%5 · display only")
                               .arg(p[0] / scale, 0, 'g', 8)
                               .arg(p[1] / scale, 0, 'g', 8)
                               .arg(p[2] / scale, 0, 'g', 8)
                               .arg(unit)
                               .arg(visible(selectedDatum) ? "Visible" : "Hidden"));
    } else
        datumInfo->setText(
            part ? "Select reference geometry. Exact datums are available after worker inspection."
                 : "Open a Part document.");
    bool anyVisible = false;
    for (auto &d : display)
        anyVisible = anyVisible || d.visible;
    if (part && !editing && (anyVisible || selected)) {
        sketchPanel->hide();
        viewport->show();
        findChild<QLabel *>("welcomePanel")->hide();
        findChild<QToolBar *>("viewportToolbar")->show();
        viewport->setStale(state().stale || !client->ready());
        viewport->setSelection(
            selected ? Selection{datumGeometry.value(selectedDatum).kind, selectedDatum, {}}
                     : state().selection);
        commands.get("fit")->setEnabled(idle);
        for (auto name : {"Isometric", "Front", "Top", "Right"})
            commands.get("view." + QString(name))->setEnabled(idle);
    }
    syncingDatums = false;
}
} // namespace cad

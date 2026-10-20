#include "MainWindow.hpp"
#include "domain/Documents.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
namespace cad {
namespace {
struct NewDocument {
    QString type, name, units;
};
std::optional<NewDocument> askDocument(QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("New document");
    dialog.setObjectName("newDocumentDialog");
    auto *form = new QFormLayout(&dialog);
    auto *type = new QComboBox;
    type->setObjectName("documentTypeInput");
    type->addItems({"Part", "Assembly", "Drawing"});
    auto *name = new QLineEdit("Untitled");
    name->setObjectName("documentNameInput");
    name->setMaxLength(128);
    auto *units = new QComboBox;
    units->setObjectName("documentUnitInput");
    units->addItems({"mm", "cm", "m", "in"});
    form->addRow("Type", type);
    form->addRow("Name", name);
    form->addRow("Display units", units);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return NewDocument{type->currentText(), name->text(), units->currentText()};
}
QCborMap fields(std::initializer_list<std::pair<QString, QCborValue>> list) {
    QCborMap p;
    for (auto f : list)
        p.insert(f.first, f.second);
    return p;
}
} // namespace
bool MainWindow::confirmRecoverable(QString action) {
    if (!state().projectOpen || state().savedRevision == state().revision)
        return true;
    QMessageBox box(
        QMessageBox::Warning, action,
        QString("Revision %1 is recoverable but has not been explicitly saved. Keeping recovery "
                "does not change the saved file. Save first if you need a portable snapshot.")
            .arg(state().revision),
        QMessageBox::NoButton, this);
    box.setObjectName("unsavedProjectWarning");
    auto *keep = box.addButton("Keep recoverable", QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == keep;
}
void MainWindow::chooseSave(bool saveAs) {
    QString path = state().savedPath;
    if (saveAs || path.isEmpty())
        path = QFileDialog::getSaveFileName(
            this, saveAs ? "Save project as new identity" : "Save project", path,
            "CAD projects (*.cadproj)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".cadproj", Qt::CaseInsensitive))
            path += ".cadproj";
        client->saveProject(path, saveAs);
    }
}
void MainWindow::showRecoveries(QCborArray offers) {
    QStringList labels, ids;
    int invalid = 0;
    for (const auto &item : offers) {
        auto r = item.toMap();
        if (r.value("invalid").toBool()) {
            ++invalid;
            continue;
        }
        if (!r.value("newer_than_saved").toBool())
            continue;
        auto id = r.value("project_id").toString();
        ids.append(id);
        labels.append(QString("%1 · recoverable %2 · saved %3")
                          .arg(id)
                          .arg(r.value("revision").toInteger())
                          .arg(r.value("saved_revision").toInteger()));
    }
    if (labels.isEmpty()) {
        if (invalid)
            notice->setText("Some recovery files failed validation; use a verified saved project. "
                            "Originals were preserved.");
        return;
    }
    bool chosen = false;
    auto selection = QInputDialog::getItem(
        this, "Recover project",
        "Newer recoverable work is available. Cancel keeps every file unchanged.", labels, 0, false,
        &chosen);
    if (chosen && confirmRecoverable("Replace active project with recovery")) {
        if (state().projectOpen) {
            notice->setText("Close the current project before choosing a recovery version.");
            return;
        }
        client->recoverProject(ids.value(labels.indexOf(selection)));
    }
}
void MainWindow::buildProjectUI() {
    notice->setTextFormat(Qt::PlainText);
    status->setTextFormat(Qt::PlainText);
    auto *file = menuBar()->actions().first()->menu();
    auto *edit = menuBar()->addMenu("&Edit");
    auto add = [&](QString id, QString text, QMenu *menu, QKeySequence shortcut = {}) {
        auto *action = commands.add(this, id, text, shortcut.toString());
        menu->addAction(action);
        addAction(action);
        return action;
    };
    connect(commands.get("new"), &QAction::triggered, this, [this] {
        if (!confirmRecoverable("New project"))
            return;
        if (auto input = askDocument(this))
            client->newProject(input->type, input->name, input->units, state().projectOpen);
    });
    connect(commands.get("open"), &QAction::triggered, this, [this] {
        if (!confirmRecoverable("Open another project"))
            return;
        auto path =
            QFileDialog::getOpenFileName(this, "Open project", {}, "CAD projects (*.cadproj)");
        if (!path.isEmpty())
            client->inspectProject(path);
    });
    connect(commands.get("save"), &QAction::triggered, this, [this] { chooseSave(); });
    connect(add("saveAs", "Save As…", file, QKeySequence::SaveAs), &QAction::triggered, this,
            [this] { chooseSave(true); });
    connect(add("document.new", "New document…", file), &QAction::triggered, this, [this] {
        if (auto input = askDocument(this))
            client->createDocument(input->type, input->name, input->units);
    });
    connect(add("project.close", "Close project", file), &QAction::triggered, this, [this] {
        if (confirmRecoverable("Close project"))
            client->closeProject();
    });
    connect(add("recover", "Review recovery…", file), &QAction::triggered, client,
            &Client::listRecoveries);
    connect(add("undo", "Undo", edit, QKeySequence::Undo), &QAction::triggered, this,
            [this] { client->projectCommand("Undo"); });
    connect(add("redo", "Redo", edit, QKeySequence::Redo), &QAction::triggered, this,
            [this] { client->projectCommand("Redo"); });
    auto *automatic = add("recoveryPreference", "Offer recovery on startup", edit);
    automatic->setCheckable(true);
    automatic->setChecked(settings->value("startup/offerRecovery", true).toBool());
    connect(automatic, &QAction::toggled, this, [this](bool enabled) {
        settings->setValue("startup/offerRecovery", enabled);
        settings->sync();
    });
    connect(client, &Client::recoveryOffers, this, &MainWindow::showRecoveries);
    connect(client, &Client::projectInspection, this, [this](QCborMap info) {
        QString choice = "Saved";
        if (info.value("recoverable_revision").toInteger() >
            info.value("saved_revision").toInteger()) {
            QMessageBox box(QMessageBox::Question, "Choose project version",
                            QString("Saved revision %1; recoverable revision %2. Choose the "
                                    "version to open; neither is overwritten by this choice.")
                                .arg(info.value("saved_revision").toInteger())
                                .arg(info.value("recoverable_revision").toInteger()),
                            QMessageBox::NoButton, this);
            auto *saved = box.addButton("Open saved", QMessageBox::AcceptRole);
            auto *recovered = box.addButton("Recover newer", QMessageBox::ActionRole);
            box.addButton(QMessageBox::Cancel);
            box.exec();
            if (box.clickedButton() == recovered)
                choice = "Recoverable";
            else if (box.clickedButton() != saved)
                return;
        }
        client->openProject(info.value("path").toString(), choice, state().projectOpen);
    });
    auto *properties = new QWidget;
    auto *form = new QFormLayout(properties);
    documentInfo = new QLabel;
    documentInfo->setTextFormat(Qt::PlainText);
    documentInfo->setWordWrap(true);
    form->addRow(documentInfo);
    documentName = new QLineEdit;
    documentName->setObjectName("documentNameEditor");
    form->addRow("Document name", documentName);
    auto *rename = new QPushButton("Rename document");
    rename->setObjectName("renameDocumentButton");
    form->addRow(rename);
    connect(rename, &QPushButton::clicked, this, [this] {
        client->projectCommand("RenameDocument", fields({{"name", documentName->text()}}));
    });
    documentUnits = new QComboBox;
    documentUnits->setObjectName("documentUnitsEditor");
    documentUnits->addItems({"mm", "cm", "m", "in"});
    form->addRow("Display units", documentUnits);
    auto *applyUnits = new QPushButton("Apply display units");
    form->addRow(applyUnits);
    connect(applyUnits, &QPushButton::clicked, this, [this] {
        client->projectCommand("SetDocumentUnits",
                               fields({{"units", documentUnits->currentText()}}));
    });
    parameterName = new QLineEdit("Depth");
    parameterName->setObjectName("parameterNameInput");
    parameterName->setMaxLength(64);
    parameterDimension = new QComboBox;
    parameterDimension->setObjectName("parameterDimensionInput");
    parameterDimension->addItems({"length", "angle", "mass", "scalar"});
    parameterUnit = new QComboBox;
    parameterUnit->setObjectName("parameterUnitInput");
    parameterValue = new QDoubleSpinBox;
    parameterValue->setObjectName("parameterValueInput");
    parameterValue->setDecimals(8);
    parameterValue->setRange(-1e9, 1e9);
    parameterValue->setValue(10);
    form->addRow("Parameter name", parameterName);
    form->addRow("Dimension", parameterDimension);
    form->addRow("Input unit", parameterUnit);
    form->addRow("Value", parameterValue);
    auto updateUnits = [this] {
        parameterUnit->clear();
        auto kind = parameterDimension->currentText();
        parameterUnit->addItems(kind == "length"  ? QStringList{"mm", "cm", "m", "in"}
                                : kind == "angle" ? QStringList{"deg", "rad"}
                                : kind == "mass"  ? QStringList{"kg", "g"}
                                                  : QStringList{"1"});
    };
    connect(parameterDimension, &QComboBox::currentTextChanged, this, updateUnits);
    updateUnits();
    auto *apply = new QPushButton("Set parameter");
    apply->setObjectName("setParameterButton");
    form->addRow(apply);
    connect(apply, &QPushButton::clicked, this, [this] {
        try {
            auto number = domain::toSI(parameterValue->value(), parameterDimension->currentText(),
                                       parameterUnit->currentText());
            client->projectCommand("SetParameter",
                                   fields({{"name", parameterName->text()},
                                           {"dimension", parameterDimension->currentText()},
                                           {"value_si", number}}));
        } catch (const domain::Error &error) {
            notice->setText(error.code + ": " + QString::fromUtf8(error.what()));
        }
    });
    auto *remove = new QPushButton("Remove parameter");
    form->addRow(remove);
    connect(remove, &QPushButton::clicked, this, [this] {
        client->projectCommand("RemoveParameter", fields({{"name", parameterName->text()}}));
    });
    auto *manager = findChild<QTabWidget *>("managerTabs");
    manager->addTab(properties, "Document");
    properties->setObjectName("documentProperties");
    properties->setEnabled(false);
}
void MainWindow::refreshProjectUI() {
    if (!state().projectOpen) {
        findChild<QWidget *>("documentProperties")->setEnabled(false);
        return;
    }
    auto &s = state();
    bool idle = client->ready() && client->outstanding() == 0;
    syncing = true;
    while (documents->count())
        documents->removeTab(0);
    QCborMap active;
    for (auto item : s.documentRecords) {
        auto d = item.toMap();
        auto id = d.value("id").toString();
        if (id == s.activeDocument)
            active = d;
        if (s.openDocuments.contains(id)) {
            int index = documents->addTab(d.value("name").toString());
            documents->setTabData(index, id);
            if (id == s.activeDocument)
                documents->setCurrentIndex(index);
        }
    }
    if (documents->count() == 0)
        documents->addTab("No open document");
    tree->clear();
    tree->setHeaderLabel("Project documents");
    for (auto item : s.documentRecords) {
        auto d = item.toMap();
        auto *row = new QTreeWidgetItem(
            tree, {d.value("name").toString() + " (" + d.value("type").toString() + ")"});
        row->setData(0, Qt::UserRole + 1, d.value("id").toString());
        row->setSelected(d.value("id").toString() == s.activeDocument);
        for (auto parameter : d.value("parameters").toArray()) {
            auto p = parameter.toMap();
            auto unit = p.value("dimension").toString() == "length"  ? d.value("units").toString()
                        : p.value("dimension").toString() == "angle" ? "rad"
                        : p.value("dimension").toString() == "mass"  ? "kg"
                                                                     : "1";
            double value = p.value("value_si").toDouble() /
                           domain::toSI(1, p.value("dimension").toString(), unit);
            auto *field = new QTreeWidgetItem(row, {p.value("name").toString() + " = " +
                                                    QString::number(value, 'g', 10) + " " + unit});
            field->setData(0, Qt::UserRole + 1, d.value("id").toString());
        }
        row->setExpanded(true);
    }
    syncing = false;
    viewport->hide();
    auto *welcome = findChild<QLabel *>("welcomePanel");
    welcome->setText(active.isEmpty()
                         ? "<h2>No open document</h2><p>Choose a document from the model tree.</p>"
                         : "<h2>" + active.value("name").toString().toHtmlEscaped() + "</h2><p>" +
                               active.value("type").toString() + " document · " +
                               active.value("units").toString() +
                               "</p><p>Document parameters are editable. No sketch or solid "
                               "geometry has been created.</p>");
    welcome->show();
    if (!active.isEmpty() && s.phase == Phase::Ready && idle) {
        documentName->setText(active.value("name").toString());
        documentUnits->setCurrentText(active.value("units").toString());
    }
    documentInfo->setText(QString("Document revision %1\nParameters use typed SI storage; "
                                  "input/display units are explicit.")
                              .arg(active.value("revision").toInteger()));
    findChild<QWidget *>("documentProperties")->setEnabled(idle && !active.isEmpty());
    findChild<QTabWidget *>("managerTabs")->setCurrentIndex(2);
    for (auto id : {"reference", "instances", "fit", "measure"}) {
        commands.get(id)->setEnabled(false);
        commands.get(id)->setToolTip("Close the project to inspect a temporary reference scene");
    }
    for (auto id : {"save", "saveAs", "document.new", "project.close"})
        commands.get(id)->setEnabled(idle);
    commands.get("close")->setEnabled(idle && !active.isEmpty());
    commands.get("close")->setText("Close document");
    commands.get("undo")->setEnabled(idle && s.canUndo);
    commands.get("redo")->setEnabled(idle && s.canRedo);
    status->setText((s.stale ? "STALE · " : "") + s.job + " · " + s.save);
    notice->setText(s.message);
    documents->setEnabled(idle);
}
} // namespace cad

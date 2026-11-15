#include "FeaturePanel.hpp"
#include "HistoryPanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "protocol/Client.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
namespace cad {
void MainWindow::buildExchangeUI() {
    auto *page = new QWidget;
    page->setObjectName("exchangeProperties");
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto *form = new QFormLayout(content);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *info =
        new QLabel("Import STEP/IGES as solid bodies. File units and healing choices are explicit. "
                   "History, names, colors, PMI and assembly structure are omitted; warnings are "
                   "retained with the source.");
    info->setWordWrap(true);
    form->addRow(info);
    auto *format = new QComboBox;
    format->setObjectName("exchangeFormat");
    format->addItems({"STEP", "IGES", "STL"});
    auto *units = new QComboBox;
    units->setObjectName("exchangeUnits");
    units->addItems({"File", "mm", "cm", "m", "in"});
    auto *healing = new QComboBox;
    healing->setObjectName("exchangeHealing");
    healing->addItems({"None", "Fix"});
    auto *detail = new QDoubleSpinBox;
    detail->setObjectName("exchangeDetail");
    detail->setDecimals(2);
    detail->setRange(.01, 5);
    detail->setValue(.2);
    detail->setSuffix(" mm");
    auto *bodies = new QComboBox;
    bodies->setObjectName("exchangeBody");
    bodies->addItem("All current bodies", QString());
    form->addRow("Format", format);
    form->addRow("Units / override", units);
    form->addRow("Import healing", healing);
    form->addRow("STL chord detail", detail);
    form->addRow("Export selection", bodies);
    auto *import = new QPushButton("Import chosen source…");
    import->setObjectName("exchangeImport");
    auto *exportButton = new QPushButton("Export geometry…");
    exportButton->setObjectName("exchangeExport");
    auto *copy = new QPushButton("Create portable part project…");
    copy->setObjectName("exchangeCopy");
    auto *manifest = new QPushButton("Inspect copy dependencies");
    manifest->setObjectName("exchangeInspect");
    for (auto *button : {import, exportButton, copy, manifest})
        form->addRow(button);
    auto *report = new QLabel("Portable copy embeds all project-local Part definitions, imported "
                              "sources and retained history. "
                              "It creates a new project identity and keeps this project open. "
                              "Existing copy destinations are refused. "
                              "Assembly/drawing closure belongs to Modules 7–8.");
    report->setObjectName("exchangeReport");
    report->setTextFormat(Qt::PlainText);
    report->setWordWrap(true);
    form->addRow(report);
    auto *progress = new QProgressBar;
    progress->setObjectName("exchangeProgress");
    progress->setRange(0, 100);
    progress->setValue(0);
    outer->addWidget(progress);
    auto *cancel = new QPushButton("Cancel exchange job");
    cancel->setObjectName("exchangeCancel");
    cancel->setEnabled(false);
    outer->addWidget(cancel);
    propertyPages->addWidget(page);
    auto *action = commands.add(this, "exchange", "Import / export / portable copy");
    addAction(action);
    menuBar()->actions().first()->menu()->addAction(action);
    connect(action, &QAction::triggered, this, [this, page] {
        exchangeShown = true;
        findChild<QTabWidget *>("managerTabs")->setCurrentIndex(1);
        propertyPages->setCurrentWidget(page);
        page->show();
    });
    connect(import, &QPushButton::clicked, this,
            [this, format, units, healing, report, cancel, progress] {
                if (format->currentText() == "STL") {
                    report->setText("STL import is unavailable. Choose STEP or IGES.");
                    return;
                }
                auto kind = format->currentText();
                auto path = QFileDialog::getOpenFileName(this, "Import geometry", {},
                                                         kind == "STEP" ? "STEP (*.step *.stp)"
                                                                        : "IGES (*.iges *.igs)");
                if (path.isEmpty())
                    return;
                progress->setValue(0);
                cancel->setEnabled(true);
                client->importGeometry(path, kind, units->currentText(), healing->currentText(),
                                       {});
            });
    connect(exportButton, &QPushButton::clicked, this,
            [this, format, units, detail, bodies, report, cancel, progress] {
                if (units->currentText() == "File") {
                    report->setText("Select mm, cm, m or in for export. STL stores numeric "
                                    "coordinates without an authoritative unit.");
                    return;
                }
                auto kind = format->currentText();
                auto suffix = kind == "STEP" ? "step" : kind == "IGES" ? "iges" : "stl";
                auto path =
                    QFileDialog::getSaveFileName(this, "Export declared current revision", {},
                                                 QString("%1 (*.%2)").arg(kind, suffix));
                if (path.isEmpty())
                    return;
                if (QFileInfo(path).suffix().isEmpty())
                    path += '.' + QString(suffix);
                bool overwrite = QFileInfo::exists(path);
                // The native save picker obtains the user's explicit overwrite choice.
                progress->setValue(0);
                cancel->setEnabled(true);
                client->exportGeometry(path, kind, units->currentText(), detail->value(),
                                       bodies->currentData().toString(), overwrite);
            });
    connect(copy, &QPushButton::clicked, this, [this] {
        auto path =
            QFileDialog::getSaveFileName(this, "Create a new portable copy (new filename required)",
                                         {}, "CAD project (*.cadproj)");
        if (path.isEmpty())
            return;
        if (QFileInfo(path).suffix().isEmpty())
            path += ".cadproj";
        client->portableCopy(path);
    });
    connect(manifest, &QPushButton::clicked, this,
            [this] { client->projectCommand("InspectClosure"); });
    connect(cancel, &QPushButton::clicked, client, &Client::cancelExchange);
    connect(client, &Client::exchangeProgress, this,
            [report, progress, cancel](QString stage, int percent) {
                report->setText(stage + "\nCancellation waits for the current parser/kernel phase "
                                        "to return; nothing is published before the final check.");
                progress->setValue(percent);
                cancel->setEnabled(true);
            });
    connect(client, &Client::exchangeReport, this, [report, progress, cancel](QCborMap result) {
        QStringList text;
        if (result.contains(QStringLiteral("source_project"))) {
            text << "Copied dependency closure: " + result.value("source_project").toString()
                 << QString("Revision %1 · %2 documents · %3 source objects")
                        .arg(result.value("source_revision").toInteger())
                        .arg(result.value("documents").toArray().size())
                        .arg(result.value("objects").toArray().size())
                 << result.value("policy").toString();
        } else {
            text << QString("Published %1 · revision %2 · units %3")
                        .arg(result.value("format").toString())
                        .arg(result.value("revision").toInteger())
                        .arg(result.value("units").toString());
            for (auto warning : result.value("warnings").toArray())
                text << warning.toString();
        }
        report->setText(text.join('\n'));
        progress->setValue(100);
        cancel->setEnabled(false);
    });
    connect(client, &Client::partResult, this, [bodies](QCborMap result) {
        if (!result.contains(QStringLiteral("bodies")))
            return;
        auto chosen = bodies->currentData();
        bodies->clear();
        bodies->addItem("All current bodies", QString());
        int index = 1;
        for (auto body : result.value("bodies").toArray())
            bodies->addItem(QString("Body %1").arg(index++), body.toString());
        int found = bodies->findData(chosen);
        if (found >= 0)
            bodies->setCurrentIndex(found);
    });
    auto updateAvailability = [this, import, exportButton, copy, manifest, cancel, report, format] {
        bool idle = client->ready() && client->outstanding() == 0;
        bool part = false;
        for (auto value : state().documentRecords)
            if (value.toMap().value("id").toString() == state().activeDocument)
                part = value.toMap().value("type").toString() == "Part";
        bool editing = (sketchPanel && sketchPanel->active()) ||
                       (featurePanel && featurePanel->active()) ||
                       (historyPanel && historyPanel->active());
        bool enabled = idle && state().projectOpen && part && !editing;
        for (auto *button : {exportButton, copy, manifest})
            button->setEnabled(enabled);
        import->setEnabled(enabled && format->currentText() != "STL");
        import->setToolTip(format->currentText() == "STL"
                               ? "Choose STEP or IGES; STL import is unavailable"
                               : "Import a STEP or IGES solid body");
        commands.get("exchange")->setEnabled(state().projectOpen && part && !editing);
        if (idle) {
            cancel->setEnabled(false);
            if (state().phase == Phase::Failed)
                report->setText(state().message);
        }
    };
    connect(client, &Client::changed, this, updateAvailability);
    connect(format, &QComboBox::currentTextChanged, this,
            [updateAvailability](QString) { updateAvailability(); });
    connect(client, &Client::projectChanged, this, [this, report, progress] {
        for (auto value : state().documentRecords) {
            auto d = value.toMap();
            if (d.value("id").toString() != state().activeDocument)
                continue;
            for (auto feature : d.value("features").toArray()) {
                auto f = feature.toMap();
                if (f.value("kind").toString() != "Imported")
                    continue;
                auto p = f.value("parameters").toMap();
                QStringList text;
                text << "Retained source: " + p.value("source_name").toString()
                     << "Units: " + p.value("units").toString() +
                            " · Healing: " + p.value("healing").toString();
                for (auto warning : p.value("report").toMap().value("warnings").toArray())
                    text << warning.toString();
                report->setText(text.join('\n'));
                progress->setValue(100);
            }
        }
    });
}
} // namespace cad

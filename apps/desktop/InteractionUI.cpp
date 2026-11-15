#include "AssemblyPanel.hpp"
#include "DrawingPanel.hpp"
#include "FeaturePanel.hpp"
#include "HistoryPanel.hpp"
#include "MainWindow.hpp"
#include "SketchPanel.hpp"
#include "protocol/Client.hpp"
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
namespace cad {
void MainWindow::nameAccessibleWidgets() {
    for (auto *label : findChildren<QLabel *>())
        if (label->buddy() && label->buddy()->accessibleName().isEmpty())
            label->buddy()->setAccessibleName(label->text().remove('&'));
    for (auto *w : findChildren<QWidget *>()) {
        if (w->accessibleName().isEmpty()) {
            QString name;
            if (auto *b = qobject_cast<QAbstractButton *>(w))
                name = b->text().remove('&');
            if (name.isEmpty() && w->focusPolicy() != Qt::NoFocus) {
                name = w->objectName();
                name.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
            }
            if (!name.isEmpty())
                w->setAccessibleName(name);
        }
        if (w->minimumHeight() > 0 && w->minimumHeight() < 24 && qobject_cast<QAbstractButton *>(w))
            w->setMinimumHeight(24);
    }
    tree->setAccessibleName("Project structure and selected geometry");
    documents->setAccessibleName("Open documents");
    commandTabs->setAccessibleName("Context command tabs");
    search->setAccessibleName("Find a command by name or alias");
    status->setAccessibleName("Project save and recovery status");
    notice->setAccessibleName("Current operation diagnostic");
}
void MainWindow::buildInteractionUI(QString settingsFile) {
    auto *menu = menuBar()->addMenu("&Workspace");
    auto add = [&](QString id, QString label) {
        auto *a = commands.add(this, id, label);
        menu->addAction(a);
        addAction(a);
        return a;
    };
    auto *cancelJob = add("job.cancel", "Cancel running computation");
    connect(cancelJob, &QAction::triggered, client, &Client::cancelExchange);
    auto *stopJob = add("job.stop", "Stop worker - recover durable work");
    connect(stopJob, &QAction::triggered, client, &Client::restart);
    auto updateJobs = [this, cancelJob, stopJob] {
        bool busy = client->outstanding() > 0;
        cancelJob->setEnabled(busy);
        stopJob->setEnabled(busy);
        for (auto *a : {cancelJob, stopJob}) {
            a->setProperty("unavailableReason",
                           busy ? QString() : "No worker operation is running");
            a->setToolTip(
                busy ? "Cancel at a safe boundary, or stop the worker and review durable recovery"
                     : a->property("unavailableReason").toString());
        }
    };
    connect(client, &Client::changed, this, updateJobs);
    updateJobs();
    auto *migrate = add("project.upgrade", "Upgrade older project copy");
    connect(migrate, &QAction::triggered, this, [this] {
        if (client->outstanding())
            return;
        auto source =
            QFileDialog::getOpenFileName(this, "Choose older project - original will be preserved",
                                         {}, "CAD project (*.cadproj)");
        if (source.isEmpty())
            return;
        auto target = QFileDialog::getSaveFileName(this, "Publish upgraded copy to a new file", {},
                                                   "CAD project (*.cadproj)");
        if (target.isEmpty())
            return;
        if (!target.endsWith(".cadproj", Qt::CaseInsensitive))
            target += ".cadproj";
        client->migrateProject(source, target);
    });
    auto *palette = add("commands.find", "Find command");
    palette->setShortcut(QKeySequence::Find);
    connect(palette, &QAction::triggered, this, [this] {
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Find command");
        dialog->setObjectName("commandPalette");
        dialog->resize(540, 420);
        auto *layout = new QVBoxLayout(dialog);
        auto *query = new QLineEdit;
        query->setObjectName("paletteQuery");
        query->setAccessibleName("Command name or alias");
        query->setPlaceholderText("Name or alias, for example boss, mate, PDF");
        auto *list = new QListWidget;
        list->setObjectName("paletteResults");
        list->setAccessibleName("Matching commands and availability reasons");
        layout->addWidget(query);
        layout->addWidget(list);
        auto update = [this, query, list] {
            list->clear();
            auto all = commands.all();
            std::sort(all.begin(), all.end(),
                      [](auto *a, auto *b) { return a->text() < b->text(); });
            for (auto *a : all)
                if (commands.matches(a, query->text())) {
                    auto reason = a->property("unavailableReason").toString();
                    if (!a->isEnabled() && reason.isEmpty())
                        reason = "Unavailable in the current document or while a job is running";
                    auto *row = new QListWidgetItem(
                        a->text() + (a->isEnabled() ? QString() : " — " + reason), list);
                    row->setData(Qt::UserRole, a->objectName());
                    if (!a->isEnabled())
                        row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
                }
            if (list->count())
                list->setCurrentRow(0);
        };
        connect(query, &QLineEdit::textChanged, dialog, update);
        auto run = [this, dialog, list] {
            auto *row = list->currentItem();
            if (!row)
                return;
            auto *a = commands.get(row->data(Qt::UserRole).toString());
            if (!a || !a->isEnabled())
                return;
            dialog->accept();
            activateWindow();
            setFocus();
            QTimer::singleShot(0, a, [a] { a->trigger(); });
        };
        connect(query, &QLineEdit::returnPressed, dialog, run);
        connect(list, &QListWidget::itemActivated, dialog, [run](QListWidgetItem *) { run(); });
        update();
        dialog->show();
        query->setFocus();
    });
    auto *shortcuts = add("commands.shortcuts", "Configure shortcuts");
    connect(shortcuts, &QAction::triggered, this, [this] {
        QDialog dialog(this);
        dialog.setObjectName("shortcutPreferences");
        dialog.setWindowTitle("Configure shortcuts");
        auto *layout = new QVBoxLayout(&dialog);
        auto *table = new QTableWidget(0, 2);
        table->setHorizontalHeaderLabels({"Command", "Shortcut"});
        table->setAccessibleName("Command shortcut configuration");
        table->horizontalHeader()->setStretchLastSection(true);
        QList<QAction *> actions = commands.all();
        std::sort(actions.begin(), actions.end(),
                  [](auto *a, auto *b) { return a->objectName() < b->objectName(); });
        QList<QKeySequenceEdit *> editors;
        for (auto *a : actions) {
            int row = table->rowCount();
            table->insertRow(row);
            auto *item = new QTableWidgetItem(a->text());
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, 0, item);
            auto *edit = new QKeySequenceEdit(a->shortcut());
            edit->setAccessibleName(a->text() + " shortcut");
            edit->setMaximumSequenceLength(1);
            table->setCellWidget(row, 1, edit);
            editors.append(edit);
        }
        layout->addWidget(table);
        auto *message = new QLabel;
        message->setWordWrap(true);
        layout->addWidget(message);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            QSet<QString> used;
            for (auto *edit : editors) {
                auto value = edit->keySequence().toString(QKeySequence::PortableText);
                if (value.isEmpty())
                    continue;
                if (used.contains(value) ||
                    QStringList{"Esc", "Return", "Enter", "Tab", "Backtab"}.contains(value)) {
                    message->setText("Each shortcut must be unique; Enter, Escape and Tab are "
                                     "reserved for native editing.");
                    return;
                }
                used.insert(value);
            }
            for (int i = 0; i < actions.size(); ++i) {
                actions[i]->setShortcut(editors[i]->keySequence());
                settings->setValue("shortcuts/" + actions[i]->objectName(),
                                   editors[i]->keySequence().toString(QKeySequence::PortableText));
            }
            settings->sync();
            dialog.accept();
        });
        dialog.resize(600, 480);
        dialog.exec();
    });
    auto *simple = add("layout.simplified", "Simplified layout");
    simple->setCheckable(true);
    auto changeLayout = [this, simple](bool value) {
        auto previous = value ? "familiar" : "simplified", next = value ? "simplified" : "familiar";
        settings->setValue(QString("layouts/") + previous, saveState(1));
        if (settings->contains(QString("layouts/") + next))
            restoreState(settings->value(QString("layouts/") + next).toByteArray(), 1);
        findChild<QDockWidget *>("resourcesDock")->setVisible(!value);
        findChild<QToolBar *>("taskPaneRail")->setVisible(!value);
        settings->setValue("interaction/simplified", value);
        settings->sync();
        simple->setChecked(value);
    };
    connect(simple, &QAction::toggled, this, changeLayout);
    if (settings->value("interaction/simplified", false).toBool())
        QTimer::singleShot(0, this, [simple] { simple->setChecked(true); });
    for (auto *a : commands.all()) {
        auto key = "shortcuts/" + a->objectName();
        if (settings->contains(key))
            a->setShortcut(
                QKeySequence(settings->value(key).toString(), QKeySequence::PortableText));
    }
    auto root = settingsFile.isEmpty()
                    ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                          "/diagnostics-v1"
                    : QFileInfo(settingsFile).absolutePath() + "/diagnostics-v1";
    journal = std::make_unique<diagnostics::Journal>(root);
    auto *performance = new QLabel("No completed job");
    performance->setObjectName("jobPerformance");
    performance->setAccessibleName("Last job timing and transfer allocation");
    statusBar()->addPermanentWidget(performance);
    connect(client, &Client::jobFinished, this,
            [this, performance](QString command, QString result, qint64 ms, qint64 bytes) {
                journal->record(command, result, ms, bytes);
                performance->setText(
                    QString("%1 · %2 ms · %3 KiB transfer").arg(command).arg(ms).arg(bytes / 1024));
            });
    auto *diagnostics = add("diagnostics.export", "Export local diagnostics");
    connect(diagnostics, &QAction::triggered, this, [this] {
        auto path = QFileDialog::getSaveFileName(
            this, "Export diagnostics - excludes models, paths and document text", {},
            "JSON (*.json)");
        if (path.isEmpty())
            return;
        if (!path.endsWith(".json", Qt::CaseInsensitive))
            path += ".json";
        try {
            journal->exportTo(path);
            notice->setText("Local diagnostics exported to your chosen file");
        } catch (const std::exception &e) {
            notice->setText(QString::fromUtf8(e.what()));
        }
    });
    auto latest = std::make_shared<QStringList>();
    auto *timings = add("diagnostics.features", "Feature and job performance");
    connect(timings, &QAction::triggered, this, [this, latest] {
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Feature performance - last evaluated Part");
        auto *layout = new QVBoxLayout(dialog);
        auto *list = new QListWidget;
        list->setAccessibleName("Feature identifiers, compute time and cache reuse");
        list->addItems(*latest);
        if (latest->isEmpty())
            list->addItem("No Part evaluation has completed in this session");
        layout->addWidget(list);
        dialog->resize(620, 360);
        dialog->show();
    });
    connect(client, &Client::partResult, this, [latest](QCborMap result) {
        auto list = result.value("feature_diagnostics").toArray();
        if (list.empty())
            return;
        latest->clear();
        for (auto value : list) {
            auto m = value.toMap();
            *latest << m.value("id").toString() + ": " +
                           QString::number(m.value("milliseconds").toDouble(), 'f', 3) + " ms" +
                           (m.value("reused").toBool() ? " (cache reused)" : " (computed)");
        }
    });
    for (auto pair : {std::pair<QWidget *, QString>{featurePanel, "acceptFeatureButton"},
                      {historyPanel, "acceptHistoryButton"},
                      {assemblyPanel, "assemblyAccept"},
                      {drawingPanel, "drawingAccept"}}) {
        auto *enter = new QShortcut(QKeySequence(Qt::Key_Return), pair.first);
        enter->setContext(Qt::WidgetWithChildrenShortcut);
        connect(enter, &QShortcut::activated, this, [this, pair] {
            auto *button = pair.first->findChild<QPushButton *>(pair.second);
            if (button && button->isEnabled() && !client->outstanding())
                button->click();
        });
    }
    nameAccessibleWidgets();
}
} // namespace cad

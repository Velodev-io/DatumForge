#include "HistoryPanel.hpp"
#include "evaluation/Plan.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>
namespace cad {
using sketch::fields;
HistoryPanel::HistoryPanel(Client *c) : client(c) {
    setObjectName("historyProperties");
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel("Feature history and reference repair");
    layout->addWidget(title);
    auto *buttons = new QHBoxLayout;
    previewButton = new QPushButton("Preview impact");
    previewButton->setObjectName("previewHistoryButton");
    accept = new QPushButton("Apply history");
    accept->setObjectName("acceptHistoryButton");
    cancelButton = new QPushButton("Cancel");
    cancelButton->setObjectName("cancelHistoryButton");
    buttons->addWidget(previewButton);
    buttons->addWidget(accept);
    buttons->addWidget(cancelButton);
    layout->addLayout(buttons);
    diagnostic = new QLabel;
    diagnostic->setObjectName("historyDiagnostic");
    diagnostic->setTextFormat(Qt::PlainText);
    diagnostic->setWordWrap(true);
    layout->addWidget(diagnostic);
    acknowledge = new QCheckBox("Accept stale history result");
    acknowledge->setObjectName("acknowledgeStale");
    layout->addWidget(acknowledge);
    acknowledge->hide();
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    inputs = new QWidget;
    auto *form = new QFormLayout(inputs);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    scroll->setWidget(inputs);
    feature = new QComboBox;
    feature->setObjectName("historyFeature");
    form->addRow("Feature", feature);
    action = new QComboBox;
    action->setObjectName("historyAction");
    action->addItems({"Rename", "Suppress", "Delete", "Reorder", "Expression"});
    form->addRow("Action", action);
    name = new QLineEdit;
    name->setObjectName("historyName");
    form->addRow("Name", name);
    suppress = new QCheckBox("Suppress feature");
    suppress->setObjectName("historySuppressed");
    form->addRow(suppress);
    cascade = new QCheckBox("Delete dependent features too");
    cascade->setObjectName("historyCascade");
    form->addRow(cascade);
    position = new QSpinBox;
    position->setObjectName("historyPosition");
    form->addRow("New position (starting at 1)", position);
    parameter = new QComboBox;
    parameter->setObjectName("historyParameter");
    form->addRow("Expression parameter", parameter);
    expression = new QLineEdit;
    expression->setObjectName("historyExpression");
    form->addRow("Unit-aware expression", expression);
    auto *inspect = new QPushButton("Inspect missing edge / repair candidates");
    inspect->setObjectName("inspectReferenceButton");
    form->addRow(inspect);
    candidateList = new QComboBox;
    candidateList->setObjectName("referenceCandidates");
    form->addRow("Explicit replacement", candidateList);
    repair = new QPushButton("Replace reference (undoable)");
    repair->setObjectName("replaceReferenceButton");
    repair->setEnabled(false);
    form->addRow(repair);
    for (auto *w : {feature, action, parameter, candidateList}) {
        w->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        w->setMinimumContentsLength(10);
    }
    connect(feature, &QComboBox::currentIndexChanged, this, [this] {
        invalidate();
        for (auto value : document.value("features").toArray()) {
            auto m = value.toMap();
            if (m.value("id").toString() != feature->currentData().toString())
                continue;
            name->setText(m.value("name").toString());
            suppress->setChecked(m.value("suppressed").toBool());
            parameter->clear();
            auto p = m.value("parameters").toMap();
            for (auto key : {"depth", "angle", "size", "diameter", "spacing", "count"})
                if (p.contains(QString(key)))
                    parameter->addItem(key);
        }
    });
    for (auto *w : {action, parameter})
        connect(w, &QComboBox::currentIndexChanged, this, [this, form] {
            invalidate();
            auto a = action->currentText();
            form->setRowVisible(name, a == "Rename");
            form->setRowVisible(suppress, a == "Suppress");
            form->setRowVisible(cascade, a == "Delete");
            form->setRowVisible(position, a == "Reorder");
            form->setRowVisible(parameter, a == "Expression");
            form->setRowVisible(expression, a == "Expression");
        });
    for (auto *w : {name, expression})
        connect(w, &QLineEdit::textChanged, this, [this] { invalidate(); });
    for (auto *w : {suppress, cascade})
        connect(w, &QCheckBox::toggled, this, [this] { invalidate(); });
    connect(position, &QSpinBox::valueChanged, this, [this] { invalidate(); });
    connect(previewButton, &QPushButton::clicked, this, &HistoryPanel::preview);
    connect(accept, &QPushButton::clicked, this, &HistoryPanel::finish);
    connect(cancelButton, &QPushButton::clicked, this, &HistoryPanel::discard);
    connect(inspect, &QPushButton::clicked, this, &HistoryPanel::inspectReference);
    connect(repair, &QPushButton::clicked, this, &HistoryPanel::replaceReference);
    connect(client, &Client::partResult, this, [this](QCborMap result) {
        if (!editing || !proposalCurrent || !result.contains(QStringLiteral("history_impact")))
            return;
        auto impact = result.value("history_impact").toMap();
        QStringList lines;
        auto label = [&](QString id) {
            for (auto v : document.value("features").toArray())
                if (v.toMap().value("id").toString() == id)
                    return v.toMap().value("name").toString();
            return QString("New feature");
        };
        for (auto key : {"affected", "deleted", "unaffected"}) {
            QStringList names;
            for (auto id : impact.value(QString(key)).toArray())
                names.append(label(id.toString()));
            lines.append(QString(key) + ": " + names.join(", "));
        }
        bool stale = result.value("part_stale").toBool();
        for (auto e : result.value("errors").toArray())
            lines.append(e.toMap().value("code").toString() + ": " +
                         e.toMap().value("message").toString());
        diagnostic->setText(
            (stale ? "STALE preview: last valid display retained\n" : "Valid history preview\n") +
            lines.join('\n'));
        acknowledge->setVisible(stale);
        acknowledge->setChecked(false);
        previewReady = true;
        accept->setEnabled(!stale);
    });
    connect(acknowledge, &QCheckBox::toggled, this, [this](bool checked) {
        accept->setEnabled(previewReady && !client->outstanding() &&
                           (!acknowledge->isVisible() || checked));
    });
    connect(client, &Client::referenceResolution, this, [this](QCborMap r) {
        if (!editing)
            return;
        resolution = r;
        candidateList->clear();
        for (auto token : r.value("candidates").toArray())
            candidateList->addItem(QString("Candidate %1").arg(candidateList->count() + 1),
                                   token.toString());
        auto role = r.value("former_role").toMap();
        auto center = role.value("center").toArray();
        QString former = "Former edge role";
        if (center.size() == 3)
            former = QString("Former edge: center (%1, %2, %3) mm; length %4 mm")
                         .arg(center[0].toDouble(), 0, 'g', 6)
                         .arg(center[1].toDouble(), 0, 'g', 6)
                         .arg(center[2].toDouble(), 0, 'g', 6)
                         .arg(role.value("measure").toDouble(), 0, 'g', 6);
        diagnostic->setToolTip(r.value("token").toString());
        diagnostic->setText("Reference: " + r.value("state").toString() + "\nOwner: " +
                            feature->currentText() + "\n" + former + "\nAffected dependents: " +
                            QString::number(r.value("affected").toArray().size()) +
                            "\nPreview of upstream source geometry; final model is STALE. Select a "
                            "replacement explicitly.");
        repair->setEnabled(candidateList->count() > 0);
    });
    connect(candidateList, &QComboBox::currentIndexChanged, this, [this] {
        if (!client->mesh())
            return;
        auto token = candidateList->currentData().toString();
        for (auto edge : client->mesh()->edgeIds)
            if (edge.contains("/" + token + "/segment/")) {
                client->select({"Edge", edge, "part"});
                break;
            }
    });
    connect(client, &Client::projectChanged, this, [this] {
        if (pending) {
            pending = false;
            editing = false;
            emit editingChanged();
        }
    });
    connect(client, &Client::changed, this, [this] {
        if (!editing)
            return;
        bool idle = !client->outstanding();
        inputs->setEnabled(idle && !pending);
        if (pending)
            accept->setEnabled(false);
        previewButton->setEnabled(idle && !pending);
        cancelButton->setEnabled(!pending);
        if (client->state().phase == Phase::Failed) {
            pending = false;
            previewReady = false;
            accept->setEnabled(false);
            diagnostic->setText(client->state().message);
        }
    });
}
QCborArray HistoryPanel::profiles() const {
    QSet<QString> wanted;
    for (auto &f : features::parse(document.value("features").toArray()))
        wanted.insert(f.parameters.value("sketch").toString());
    QCborArray out;
    for (auto value : client->state().documentRecords) {
        auto d = value.toMap();
        for (auto s : d.value("sketches").toArray())
            if (d.value("id") == document.value("id") ||
                wanted.contains(s.toMap().value("id").toString()))
                out.append(s);
    }
    return out;
}
void HistoryPanel::invalidate() {
    proposalCurrent = false;
    previewReady = false;
    accept->setEnabled(false);
    candidate = {};
    resolution = {};
    repair->setEnabled(false);
}
void HistoryPanel::begin(QCborMap d, QString selected) {
    if (editing || client->outstanding())
        return;
    document = d;
    feature->clear();
    for (auto v : d.value("features").toArray()) {
        auto f = v.toMap();
        feature->addItem(f.value("name").toString(), f.value("id").toString());
    }
    int index = feature->findData(selected);
    feature->setCurrentIndex(index >= 0 ? index : feature->count() - 1);
    position->setRange(1, std::max(1, feature->count()));
    action->setCurrentIndex(-1);
    action->setCurrentIndex(0);
    editing = true;
    pending = false;
    inputs->setEnabled(true);
    cancelButton->setEnabled(true);
    invalidate();
    previewButton->setEnabled(true);
    acknowledge->hide();
    diagnostic->setText("Preview impact before committing. Missing or ambiguous references require "
                        "an explicit replacement.");
    emit editingChanged();
}
void HistoryPanel::preview() {
    try {
        auto a = action->currentText();
        auto before = document.value("features").toArray();
        QCborMap options;
        if (a == "Expression") {
            auto all = features::parse(before);
            for (auto &f : all)
                if (f.id == feature->currentData().toString()) {
                    auto key = parameter->currentText();
                    if (expression->text().trimmed().isEmpty())
                        f.expressions.remove(key);
                    else
                        f.expressions[key] = expression->text();
                }
            candidate = features::encode(all);
        } else {
            if (a == "Rename")
                options = fields({{"name", name->text()}});
            if (a == "Suppress")
                options = fields({{"suppressed", suppress->isChecked()}});
            if (a == "Delete")
                options = fields({{"cascade", cascade->isChecked()}});
            if (a == "Reorder")
                options = fields({{"index", position->value() - 1}});
            candidate = evaluation::change(profiles(), before, a, feature->currentData().toString(),
                                           options);
        }
        proposalCurrent = true;
        previewReady = false;
        accept->setEnabled(false);
        client->projectCommand("PreviewHistory", fields({{"features", candidate}}));
    } catch (const std::exception &e) {
        diagnostic->setText(QString::fromUtf8(e.what()));
    }
}
void HistoryPanel::finish() {
    if (!proposalCurrent || !previewReady || client->outstanding() ||
        (acknowledge->isVisible() && !acknowledge->isChecked()))
        return;
    pending = true;
    client->projectCommand("CommitHistory", fields({{"features", candidate}}));
}
void HistoryPanel::discard() {
    if (!editing || pending)
        return;
    editing = false;
    client->cancelPartPreview();
    emit editingChanged();
}
void HistoryPanel::inspectReference() {
    auto all = features::parse(document.value("features").toArray());
    for (auto &f : all)
        if (f.id == feature->currentData().toString()) {
            auto edges = f.parameters.value("edges").toArray();
            if (edges.empty()) {
                diagnostic->setText("Select a fillet/chamfer with an edge role to repair.");
                return;
            }
            client->projectCommand("ReferenceCandidates",
                                   fields({{"owner", f.id}, {"token", edges[0]}}));
        }
}
void HistoryPanel::replaceReference() {
    if (resolution.empty() || candidateList->currentIndex() < 0 || client->outstanding())
        return;
    pending = true;
    client->projectCommand(
        "ResolveReference",
        fields({{"owner", resolution.value("owner")},
                {"token", resolution.value("token")},
                {"replacement", candidateList->currentData().toString()},
                {"source_fingerprint", resolution.value("source_fingerprint")}}));
}
} // namespace cad

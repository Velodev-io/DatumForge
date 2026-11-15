#include "DrawingPanel.hpp"
#include "drawing/Paint.hpp"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QSignalBlocker>
#include <QVBoxLayout>
namespace cad {
using sketch::fields;
DrawingCanvas::DrawingCanvas(QWidget *parent) : QWidget(parent) {
    setObjectName("drawingCanvas");
    setAccessibleName("Engineering drawing sheet");
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(300, 200);
}
void DrawingCanvas::install(QCborMap value) {
    sheet = value;
    update();
}
void DrawingCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    if (sheet.empty())
        return;
    double w = sheet.value("width_mm").toDouble(), h = sheet.value("height_mm").toDouble(),
           s = qMin((width() - 20) / w, (height() - 20) / h);
    p.translate((width() - w * s) / 2, (height() - h * s) / 2);
    p.scale(s, s);
    drawing::paint(p, sheet);
}
DrawingPanel::DrawingPanel(Client *c, DrawingCanvas *v) : client(c), canvas(v) {
    setObjectName("drawingProperties");
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *body = new QWidget;
    auto *form = new QFormLayout(body);
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
    auto combo = [&](QString id, QString label, QStringList values) {
        auto *q = new QComboBox;
        q->setObjectName(id);
        q->setAccessibleName(label);
        q->addItems(values);
        form->addRow(label, q);
        return q;
    };
    auto line = [&](QString id, QString label) {
        auto *q = new QLineEdit;
        q->setObjectName(id);
        q->setAccessibleName(label);
        form->addRow(label, q);
        return q;
    };
    auto number = [&](QString id, QString label, double low, double high, double value) {
        auto *q = new QDoubleSpinBox;
        q->setObjectName(id);
        q->setAccessibleName(label);
        q->setRange(low, high);
        q->setDecimals(3);
        q->setValue(value);
        form->addRow(label, q);
        return q;
    };
    auto button = [&](QString id, QString label, auto action) {
        auto *b = new QPushButton(label);
        b->setObjectName(id);
        form->addRow(b);
        connect(b, &QPushButton::clicked, this, action);
        return b;
    };
    message = new QLabel("Select a source model, create views, preview, then accept. Coordinates "
                         "are physical sheet mm.");
    message->setWordWrap(true);
    message->setObjectName("drawingStatus");
    form->addRow(message);
    beginButton = button("drawingBegin", "Edit drawing", [this] { begin(); });
    sheets = combo("drawingSheets", "Sheet", {});
    name = line("drawingSheetName", "Sheet name");
    width = number("drawingWidth", "Width (mm)", 100, 2000, 297);
    height = number("drawingHeight", "Height (mm)", 100, 2000, 210);
    scale = number("drawingScale", "Model to paper scale", .001, 100, .5);
    units = combo("drawingUnits", "Dimension units", {"mm", "cm", "m", "in"});
    projection = combo("drawingProjection", "Projection", {"third-angle", "first-angle"});
    button("drawingSaveSheet", "Apply sheet settings", [this] { saveSheet(); });
    button("drawingAddSheet", "Add sheet", [this] {
        if (!editing)
            return;
        saveSheet();
        draft.sheets.push_back({});
        populate();
        sheets->setCurrentIndex(int(draft.sheets.size()) - 1);
    });
    button("drawingRemoveSheet", "Remove sheet", [this] {
        if (editing && sheet()) {
            draft.sheets.erase(draft.sheets.begin() + sheets->currentIndex());
            populate();
        }
    });
    source = combo("drawingSource", "Source model", {});
    orientation =
        combo("drawingOrientation", "View orientation", {"Front", "Top", "Right", "Isometric"});
    x = number("drawingX", "Placement X (mm)", 0, 2000, 35);
    y = number("drawingY", "Placement Y (mm)", 0, 2000, 100);
    hiddenLines = new QCheckBox("Show hidden lines");
    hiddenLines->setObjectName("drawingHiddenLines");
    hiddenLines->setChecked(true);
    form->addRow(hiddenLines);
    projected = new QCheckBox("Project from an existing Front base");
    projected->setObjectName("drawingProjected");
    form->addRow(projected);
    button("drawingAddView", "Add view", [this] {
        if (!editing || !sheet() || source->currentIndex() < 0)
            return;
        saveSheet();
        drawing::View v;
        v.source = source->currentData().toString();
        v.orientation = orientation->currentText();
        v.x = x->value();
        v.y = y->value();
        v.hidden = hiddenLines->isChecked();
        if (projected->isChecked())
            for (auto &base : sheet()->views)
                if (base.source == v.source && base.orientation == "Front") {
                    v.parent = base.id;
                    break;
                }
        sheet()->views.push_back(v);
        populate();
        views->setCurrentIndex(int(sheet()->views.size()) - 1);
    });
    views = combo("drawingViews", "Annotation view", {});
    button("drawingRemoveView", "Remove selected view", [this] {
        if (!editing || !sheet() || views->currentIndex() < 0)
            return;
        auto id = sheet()->views[size_t(views->currentIndex())].id;
        auto &a = sheet()->annotations;
        a.erase(std::remove_if(a.begin(), a.end(), [&](auto &q) { return q.view == id; }), a.end());
        sheet()->views.erase(sheet()->views.begin() + views->currentIndex());
        populate();
    });
    button("drawingAnchors", "List exact edge anchors", [this] {
        if (!sheet() || views->currentIndex() < 0)
            return;
        client->projectCommand(
            "DrawingAnchors",
            fields({{"source", sheet()->views[size_t(views->currentIndex())].source}}));
    });
    anchors = combo("drawingAnchor", "First exact edge", {});
    second = combo("drawingSecondAnchor", "Second edge (angle)", {});
    kind = combo("drawingAnnotationKind", "Annotation",
                 {"Linear", "Angular", "Radius", "Diameter", "Center", "Text"});
    text = line("drawingText", "Sheet text");
    overrideText = line("drawingOverride", "Display override (optional)");
    button("drawingAddAnnotation", "Add annotation", [this] {
        if (!editing || !sheet())
            return;
        drawing::Annotation a;
        a.kind = kind->currentText();
        a.x = x->value();
        a.y = y->value();
        a.text = text->text();
        a.overrideText = overrideText->text();
        if (views->currentIndex() >= 0)
            a.view = sheet()->views[size_t(views->currentIndex())].id;
        auto anchor = anchors->currentData().value<QCborMap>();
        a.body = anchor.value("body").toString();
        a.token = anchor.value("token").toString();
        a.secondToken = second->currentData().value<QCborMap>().value("token").toString();
        sheet()->annotations.push_back(a);
        populate();
        message->setText(
            "Annotation added to draft; preview validates anchors and measures geometry.");
    });
    annotationRows = combo("drawingAnnotations", "Existing annotation", {});
    button("drawingRepairAnnotation", "Replace selected annotation anchor", [this] {
        if (!editing || !sheet() || annotationRows->currentIndex() < 0)
            return;
        auto index = size_t(annotationRows->currentIndex());
        if (index >= sheet()->annotations.size())
            return;
        auto &a = sheet()->annotations[index];
        auto anchor = anchors->currentData().value<QCborMap>();
        a.body = anchor.value("body").toString();
        a.token = anchor.value("token").toString();
        a.secondToken = second->currentData().value<QCborMap>().value("token").toString();
        a.overrideText = overrideText->text();
        a.text = text->text();
        message->setText("Explicit replacement staged with the same annotation ID; Preview and "
                         "Accept to commit");
        populate();
    });
    button("drawingRemoveAnnotation", "Remove selected annotation", [this] {
        if (editing && sheet() && annotationRows->currentIndex() >= 0) {
            sheet()->annotations.erase(sheet()->annotations.begin() +
                                       annotationRows->currentIndex());
            populate();
        }
    });
    button("drawingClearAnnotations", "Remove annotations on this sheet", [this] {
        if (editing && sheet())
            sheet()->annotations.clear();
    });
    button("drawingBOM", "Add BOM from source assembly", [this] {
        if (editing && sheet())
            sheet()->bomSource = source->currentData().toString();
    });
    previewButton = button("drawingPreview", "Preview", [this] {
        if (editing) {
            saveSheet();
            client->projectCommand("PreviewDrawing", fields({{"drawing", draft.map()}}));
        }
    });
    applyButton = button("drawingAccept", "Accept", [this] {
        if (!editing)
            return;
        saveSheet();
        try {
            drawing::Model::parse(draft.map());
            submittedRevision = client->state().revision;
            submitting = true;
            client->projectCommand("SetDrawing", fields({{"drawing", draft.map()}}));
            emit editingChanged();
        } catch (const std::exception &e) {
            message->setText(QString::fromUtf8(e.what()));
        }
    });
    cancelButton = button("drawingCancel", "Cancel", [this] { discard(); });
    button("drawingUpdate", "Update source revisions", [this] {
        if (!editing)
            client->projectCommand("UpdateDrawing");
    });
    auto *footer = new QHBoxLayout;
    for (auto *b : {previewButton, applyButton, cancelButton}) {
        form->removeWidget(b);
        footer->addWidget(b);
    }
    outer->addLayout(footer);
    connect(sheets, &QComboBox::currentIndexChanged, this, [this] {
        if (!syncing)
            populate();
    });
    connect(client, &Client::drawingInformation, this, [this](QCborMap result) {
        latest = result;
        auto all = result.value("sheets").toArray();
        int i = qMax(0, sheets->currentIndex());
        canvas->install(i < all.size() ? all[i].toMap() : QCborMap{});
        message->setText(result.value("current").toBool()
                             ? "Current exact views / measured annotations"
                             : "Stale, unresolved or dangling: inspect views and repair anchors; "
                               "PDF requires current views");
    });
    connect(client, &Client::drawingAnchors, this, [this](QCborArray list) {
        anchors->clear();
        second->clear();
        for (auto a : list) {
            auto m = a.toMap();
            auto label = m.value("kind").toString() + " " + m.value("token").toString();
            anchors->addItem(label, QVariant::fromValue(m));
            second->addItem(label, QVariant::fromValue(m));
        }
    });
}
drawing::Sheet *DrawingPanel::sheet() {
    int i = sheets->currentIndex();
    return i >= 0 && size_t(i) < draft.sheets.size() ? &draft.sheets[size_t(i)] : nullptr;
}
void DrawingPanel::saveSheet() {
    if (!editing || !sheet())
        return;
    auto &s = *sheet();
    s.name = name->text();
    s.width = width->value();
    s.height = height->value();
    s.scale = scale->value();
    s.units = units->currentText();
    s.projection = projection->currentText();
}
void DrawingPanel::populate() {
    if (syncing)
        return;
    syncing = true;
    int i = qMax(0, sheets->currentIndex());
    sheets->clear();
    for (auto &s : draft.sheets)
        sheets->addItem(s.name);
    sheets->setCurrentIndex(qMin(i, sheets->count() - 1));
    views->clear();
    annotationRows->clear();
    if (auto *s = sheet()) {
        name->setText(s->name);
        width->setValue(s->width);
        height->setValue(s->height);
        scale->setValue(s->scale);
        units->setCurrentText(s->units);
        projection->setCurrentText(s->projection);
        for (auto &v : s->views)
            views->addItem(v.orientation + " " + v.source, v.id);
        for (auto &a : s->annotations)
            annotationRows->addItem(a.kind + " " + a.id, a.id);
    }
    auto all = latest.value("sheets").toArray();
    i = sheets->currentIndex();
    canvas->install(i >= 0 && i < all.size() ? all[i].toMap() : QCborMap{});
    syncing = false;
}
void DrawingPanel::install(QCborMap record, QCborArray documents, bool idle) {
    auto id = record.value("id").toString();
    if (id != doc) {
        editing = false;
        submitting = false;
        doc = id;
        latest = {};
        canvas->install({});
    }
    if (submitting && idle) {
        submitting = false;
        if (client->state().revision > submittedRevision && client->state().phase != Phase::Failed)
            editing = false;
        else
            message->setText(client->state().message);
    }
    if (!editing) {
        draft = record.contains(QStringLiteral("drawing"))
                    ? drawing::Model::parse(record.value("drawing").toMap())
                    : drawing::Model{};
        populate();
    }
    QString selected = source->currentData().toString();
    {
        QSignalBlocker b(source);
        source->clear();
        for (auto v : documents) {
            auto d = v.toMap();
            if (d.value("type") == "Part" || d.value("type") == "Assembly")
                source->addItem(d.value("name").toString(), d.value("id").toString());
        }
        source->setCurrentIndex(qMax(0, source->findData(selected)));
    }
    for (auto *b : findChildren<QPushButton *>()) {
        bool readAction = b == beginButton || b->objectName() == "drawingUpdate";
        b->setEnabled(idle && !doc.isEmpty() && (readAction ? !editing : editing));
        b->setToolTip(readAction ? b->text()
                                 : "Start Edit drawing to change this draft; Preview, Accept or "
                                   "Cancel when finished");
    }
    for (QWidget *field : QList<QWidget *>{name, width, height, scale, units, projection, source,
                                           orientation, x, y, views, kind, anchors, second, text,
                                           overrideText, annotationRows, hiddenLines, projected})
        field->setEnabled(idle && editing);
    sheets->setEnabled(idle && !doc.isEmpty());
    beginButton->setEnabled(idle && !doc.isEmpty() && !editing);
    applyButton->setEnabled(idle && editing);
    previewButton->setEnabled(idle && editing);
    cancelButton->setEnabled(idle && editing);
}
void DrawingPanel::begin() {
    if (doc.isEmpty() || editing)
        return;
    editing = true;
    if (draft.sheets.empty())
        draft.sheets.push_back({});
    populate();
    emit editingChanged();
}
void DrawingPanel::discard() {
    editing = false;
    submitting = false;
    latest = {};
    client->projectCommand("DisplayDrawing");
    emit editingChanged();
}
} // namespace cad

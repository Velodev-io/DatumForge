#include "SketchPanel.hpp"
#include "domain/Documents.hpp"
#include <QSignalBlocker>
namespace cad {
void SketchPanel::clearLineEdit() {
    bool pending = !lineEditBefore.empty();
    lineEditBefore.clear();
    lineEditCandidate.clear();
    lineEditDisclosure->clear();
    lineEditConsent->setChecked(false);
    lineEditApply->setEnabled(false);
    if (pending) {
        client->discardSketchPreview();
        if (editing && !draft.empty())
            view->showSketch(current(), selectedId, true);
    }
}
void SketchPanel::refreshLineEdit() {
    QSignalBlocker sourceBlock(lineEditSource), boundaryBlock(lineEditBoundaries);
    auto source = lineEditSource->currentData().toString();
    QStringList boundaries;
    for (auto *item : lineEditBoundaries->selectedItems())
        boundaries.append(item->data(Qt::UserRole).toString());
    lineEditSource->clear();
    lineEditBoundaries->clear();
    if (!(editing ? draft : accepted).empty()) {
        int number = 0;
        for (auto &e : current().entities) {
            ++number;
            if (e.kind == "Point")
                continue;
            auto label = e.kind + " " + QString::number(number);
            if (e.kind == "Line")
                lineEditSource->addItem(label, e.id);
            auto *item = new QListWidgetItem(label, lineEditBoundaries);
            item->setData(Qt::UserRole, e.id);
            item->setToolTip(e.id);
            item->setSelected(boundaries.contains(e.id));
        }
    }
    int at = lineEditSource->findData(source);
    if (at >= 0)
        lineEditSource->setCurrentIndex(at);
    lineEditGroup->setVisible(editing && tool->currentText() == "Trim / extend");
    lineEditPreview->setEnabled(editing);
    if (lineEditCandidate.empty())
        lineEditApply->setEnabled(false);
}
void SketchPanel::requestLineEdit() {
    domain::check(editing && !committing && client->ready(), "InvalidInput",
                  "Edit a sketch before trimming");
    bool xOK, yOK;
    double x = lineEditX->text().toDouble(&xOK), y = lineEditY->text().toDouble(&yOK);
    domain::check(xOK && yOK, "InvalidInput", "Numeric pick coordinates required");
    auto units = property("units").toString();
    QCborArray boundaries;
    for (auto *item : lineEditBoundaries->selectedItems())
        boundaries.append(item->data(Qt::UserRole).toString());
    auto edit = sketch::fields({{"mode", lineEditMode->currentText()},
                                {"source", lineEditSource->currentData().toString()},
                                {"boundaries", boundaries},
                                {"pick_x", domain::toSI(x, "length", units)},
                                {"pick_y", domain::toSI(y, "length", units)}});
    clearLineEdit();
    lineEditBefore = sketch::encode(draft);
    client->projectCommand(
        "PreviewSketchEdit",
        sketch::fields({{"sketches", lineEditBefore}, {"sketch", current().id}, {"edit", edit}}));
}
} // namespace cad

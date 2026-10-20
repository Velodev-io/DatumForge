#include "SketchView.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>
namespace cad {
SketchView::SketchView(QWidget *p) : QWidget(p) {
    setObjectName("sketchViewport");
    setAccessibleName("Planar sketch viewport");
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(240, 200);
}
void SketchView::showSketch(sketch::Sketch s, QString selected, bool editing) {
    drawing = std::move(s);
    selectedId = selected;
    editable = editing;
    update();
}
QPointF SketchView::screen(sketch::Vec2 p) const {
    return {width() * .2 + offset.x() + p.x * scale, height() * .75 + offset.y() - p.y * scale};
}
sketch::Vec2 SketchView::local(QPointF p) const {
    return {(p.x() - width() * .2 - offset.x()) / scale,
            -(p.y() - height() * .75 - offset.y()) / scale};
}
void SketchView::fit() {
    scale = 6000;
    offset = {};
    double max = 0;
    for (auto &e : drawing.entities)
        if (e.kind == "Point")
            max = std::max(max, std::max(std::abs(e.point.x), std::abs(e.point.y)));
    if (max > 0)
        scale = std::clamp(std::min(width(), height()) * .6 / max, 1.0, 1e9);
    update();
}
void SketchView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(23, 30, 40));
    p.setPen(QPen(QColor(58, 68, 82), 1));
    auto o = screen({0, 0});
    p.drawLine(QPointF(0, o.y()), QPointF(width(), o.y()));
    p.drawLine(QPointF(o.x(), 0), QPointF(o.x(), height()));
    for (auto &e : drawing.entities) {
        p.setPen(QPen(e.id == selectedId ? QColor(255, 197, 74) : QColor(105, 210, 245),
                      e.id == selectedId ? 3 : 2, e.construction ? Qt::DashLine : Qt::SolidLine));
        if (e.kind == "Point") {
            auto q = screen(e.point);
            p.drawEllipse(q, 3, 3);
            continue;
        }
        auto a = screen(drawing.entity(e.a).point);
        if (e.kind == "Line")
            p.drawLine(a, screen(drawing.entity(e.b).point));
        else {
            double r = e.radius * scale;
            QRectF box(a.x() - r, a.y() - r, 2 * r, 2 * r);
            if (e.kind == "Circle")
                p.drawEllipse(box);
            else
                p.drawArc(box, int(e.start * 180 / 3.141592653589793 * 16),
                          int(e.sweep * 180 / 3.141592653589793 * 16));
        }
    }
    p.setPen(QColor(255, 218, 140));
    for (auto &c : drawing.constraints)
        if (QStringList{"Length", "Distance", "Angle", "Radius"}.contains(c.kind)) {
            auto &e = drawing.entity(c.a);
            auto pos = screen(e.kind == "Point" ? e.point : drawing.entity(e.a).point);
            if (e.kind == "Line") {
                auto end = screen(drawing.entity(e.b).point);
                pos = (pos + end) / 2;
            }
            QString unit = c.kind == "Angle" ? "deg" : property("units").toString();
            if (unit.isEmpty())
                unit = "mm";
            double n = c.value / domain::toSI(1, c.kind == "Angle" ? "angle" : "length", unit);
            QString label = c.kind + " " + QString::number(n, 'g', 7) + " " + unit +
                            (c.expression.isEmpty() ? "" : " [" + c.expression + "]");
            double labelWidth = p.fontMetrics().horizontalAdvance(label);
            pos.setX(std::clamp(pos.x() + 8, 12.0, std::max(12.0, width() - labelWidth - 12)));
            pos.setY(std::clamp(pos.y() - 12, 65.0, std::max(65.0, height() - 35.0)));
            p.drawText(pos, label);
        }
    p.setPen(Qt::white);
    p.drawText(QRect(12, 10, width() - 24, 48), Qt::TextWordWrap,
               drawing.name + " · support plane coordinates · " +
                   (editable ? "PREVIEW / draft" : "Accepted sketch"));
    p.drawText(
        12, height() - 12,
        "Wheel: zoom · middle drag: pan · click: draw/select · drag point: draft · Escape: cancel");
}
QString SketchView::pick(QPointF p) const {
    QString best;
    double d = 9;
    // Endpoints have priority over adjacent curves within the pick radius.
    for (auto &e : drawing.entities)
        if (e.kind == "Point") {
            double distance = QLineF(p, screen(e.point)).length();
            if (distance < d) {
                d = distance;
                best = e.id;
            }
        }
    if (!best.isEmpty())
        return best;
    for (auto &e : drawing.entities) {
        double distance = 1e20;
        if (e.kind == "Point")
            distance = QLineF(p, screen(e.point)).length();
        else {
            auto a = screen(drawing.entity(e.a).point);
            if (e.kind == "Line") {
                auto b = screen(drawing.entity(e.b).point);
                auto ab = b - a;
                double t = std::clamp(QPointF::dotProduct(p - a, ab) / QPointF::dotProduct(ab, ab),
                                      0.0, 1.0);
                distance = QLineF(p, a + t * ab).length();
            } else {
                distance = std::abs(QLineF(p, a).length() - e.radius * scale);
                if (e.kind == "Arc") {
                    auto v = local(p);
                    auto c = drawing.entity(e.a).point;
                    double t =
                        std::fmod(std::atan2(v.y - c.y, v.x - c.x) - e.start + 12.566370614359172,
                                  6.283185307179586);
                    if (t > e.sweep)
                        distance = 1e20;
                }
            }
        }
        if (distance < d) {
            d = distance;
            best = e.id;
        }
    }
    return best;
}
void SketchView::mousePressEvent(QMouseEvent *e) {
    setFocus();
    previous = e->pos();
    if (e->button() != Qt::LeftButton)
        return;
    if (editable && property("drawing").toBool()) {
        auto p = local(e->position());
        emit clicked(p.x, p.y);
        return;
    }
    auto id = pick(e->position());
    if (!id.isEmpty()) {
        emit selected(id);
        if (editable && drawing.entity(id).kind == "Point")
            dragging = id;
    } else {
        auto p = local(e->position());
        emit clicked(p.x, p.y);
    }
}
void SketchView::mouseMoveEvent(QMouseEvent *e) {
    if (e->buttons() & Qt::MiddleButton) {
        offset += e->pos() - previous;
        update();
    }
    previous = e->pos();
    if (!dragging.isEmpty() && (e->buttons() & Qt::LeftButton)) {
        auto p = local(e->position());
        emit dragged(dragging, p.x, p.y);
    }
}
void SketchView::mouseReleaseEvent(QMouseEvent *) {
    dragging.clear();
}
void SketchView::wheelEvent(QWheelEvent *e) {
    auto before = local(e->position());
    scale = std::clamp(scale * std::exp(e->angleDelta().y() / 1000.0), 1.0, 1e9);
    offset += e->position() - screen(before);
    update();
}
void SketchView::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape)
        emit canceled();
    else
        QWidget::keyPressEvent(e);
}
} // namespace cad

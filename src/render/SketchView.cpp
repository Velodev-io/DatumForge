#include "SketchView.hpp"
#include "sketch/Editing.hpp"
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>
namespace cad {
SketchView::SketchView(QWidget *p) : QWidget(p) {
    setObjectName("sketchViewport");
    setAccessibleName("Planar sketch viewport");
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(240, 200);
}
void SketchView::showSketch(sketch::Sketch s, QString selected, bool editing) {
    drawing = std::move(s);
    selectedId = selected;
    editable = editing;
    present = true;
    setAccessibleDescription(drawing.name + (editable ? " · Sketch draft" : " · Accepted sketch"));
    update();
}
void SketchView::clearSketch() {
    drawing = {};
    editable = present = false;
    setAccessibleDescription("No sketch · choose a support plane and New sketch");
    cancelPointerInput();
    update();
}
void SketchView::setTool(QString name) {
    cancelPointerInput();
    tool = std::move(name);
    update();
}
void SketchView::setInputAnchor(std::optional<sketch::Vec2> anchor) {
    if (anchor && !inputAnchor)
        cursor = *anchor;
    inputAnchor = anchor;
    update();
}
bool SketchView::cancelPointerInput() {
    bool active = circleCenter.has_value() || inputAnchor.has_value();
    circleCenter.reset();
    inputAnchor.reset();
    primitivePressed = false;
    if (!dragging.isEmpty()) {
        dragging.clear();
        emit dragCanceled();
        active = true;
    }
    update();
    return active;
}
QPointF SketchView::screen(sketch::Vec2 p) const {
    return {width() / 2. + (p.x - cameraCenter.x) * scale,
            height() / 2. - (p.y - cameraCenter.y) * scale};
}
sketch::Vec2 SketchView::local(QPointF p) const {
    return {cameraCenter.x + (p.x() - width() / 2.) / scale,
            cameraCenter.y - (p.y() - height() / 2.) / scale};
}
sketch::Vec2 SketchView::snapPoint(sketch::Vec2 p) const {
    return drawing.snap(p, 8. / scale, property("gridEnabled").toBool() ? .001 : 0);
}
void SketchView::fit() {
    if (drawing.entities.empty()) {
        cameraCenter = {};
        scale = 6000;
        update();
        return;
    }
    double left = 1e100, right = -1e100, bottom = 1e100, top = -1e100;
    auto include = [&](sketch::Vec2 p, double r = 0.) {
        left = std::min(left, p.x - r);
        right = std::max(right, p.x + r);
        bottom = std::min(bottom, p.y - r);
        top = std::max(top, p.y + r);
    };
    for (auto &e : drawing.entities)
        if (e.kind == "Point")
            include(e.point);
        else if (e.kind == "Circle" || e.kind == "Arc")
            include(drawing.entity(e.a).point, e.radius);
    cameraCenter = {(left + right) / 2., (bottom + top) / 2.};
    scale = std::clamp(std::min(std::max(40, width() - 96) / std::max(right - left, 1e-9),
                                std::max(40, height() - 144) / std::max(top - bottom, 1e-9)),
                       .01, 1e12);
    update();
}
void SketchView::zoomAt(QPointF at, double factor) {
    auto before = local(at);
    scale = std::clamp(scale * factor, .01, 1e12);
    auto after = local(at);
    cameraCenter.x += before.x - after.x;
    cameraCenter.y += before.y - after.y;
    if (circleCenter || inputAnchor)
        cursor = snapPoint(local(previous));
    update();
}
bool SketchView::event(QEvent *e) {
    if (e->type() == QEvent::FocusOut || e->type() == QEvent::WindowDeactivate) {
        spaceDown = false;
        unsetCursor();
    }
    if (e->type() == QEvent::NativeGesture) {
        auto *g = static_cast<QNativeGestureEvent *>(e);
        if (g->gestureType() == Qt::ZoomNativeGesture)
            zoomAt(g->position(), std::clamp(1. + g->value(), .1, 10.));
        else if (g->gestureType() == Qt::PanNativeGesture) {
            cameraCenter.x -= g->delta().x() / scale;
            cameraCenter.y += g->delta().y() / scale;
            update();
        }
        e->accept();
        return true;
    }
    return QWidget::event(e);
}
void SketchView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    QColor background = dark ? QColor(23, 30, 40) : QColor(246, 248, 251);
    QColor ink = dark ? QColor(105, 210, 245) : QColor(38, 112, 174);
    p.fillRect(rect(), background);
    p.setPen(QPen(dark ? QColor(58, 68, 82) : QColor(205, 214, 225), 1));
    auto o = screen({0, 0});
    p.drawLine(QPointF(0, o.y()), QPointF(width(), o.y()));
    p.drawLine(QPointF(o.x(), 0), QPointF(o.x(), height()));
    for (auto &e : drawing.entities) {
        p.setPen(QPen(e.id == selectedId ? QColor(255, 197, 74) : ink, e.id == selectedId ? 3 : 2,
                      e.construction ? Qt::DashLine : Qt::SolidLine));
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
    if (tool == "Select / drag" && !selectedId.isEmpty()) {
        auto found = std::find_if(drawing.entities.begin(), drawing.entities.end(),
                                  [&](auto &e) { return e.id == selectedId; });
        if (found != drawing.entities.end()) {
            p.setPen(QPen(QColor(255, 197, 74), 1.5));
            p.setBrush(background);
            auto handle = [&](sketch::Vec2 q) {
                auto at = screen(q);
                p.drawRect(QRectF(at.x() - 4, at.y() - 4, 8, 8));
            };
            if (auto r = sketch::rectangleOf(drawing, selectedId))
                for (auto &id : r->points)
                    handle(drawing.entity(id).point);
            else if (found->kind == "Circle" || found->kind == "Arc") {
                auto c = drawing.entity(found->a).point;
                if (found->kind == "Circle")
                    for (auto d : std::array<sketch::Vec2, 4>{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}})
                        handle({c.x + d.x * found->radius, c.y + d.y * found->radius});
            } else if (found->kind == "Line") {
                handle(drawing.entity(found->a).point);
                handle(drawing.entity(found->b).point);
            } else
                handle(found->point);
            p.setBrush(Qt::NoBrush);
        }
    }
    p.setPen(QPen(QColor(255, 197, 74), 2, Qt::DashLine));
    if (circleCenter) {
        double r = sketch::length(*circleCenter, cursor);
        p.drawEllipse(screen(*circleCenter), r * scale, r * scale);
        QString units = property("units").toString();
        if (units.isEmpty())
            units = "mm";
        auto at = screen(cursor);
        at.setX(std::clamp(at.x() + 12, 14., std::max(14., width() - 190.)));
        at.setY(std::clamp(at.y() - 15, 48., std::max(48., height() - 35.)));
        p.drawText(at, "R " + QString::number(r / domain::toSI(1, "length", units), 'g', 7) + " " +
                           units);

    } else if (inputAnchor) {
        if (tool == "Rectangle")
            p.drawRect(QRectF(screen(*inputAnchor), screen(cursor)).normalized());
        else
            p.drawLine(screen(*inputAnchor), screen(cursor));
    }
    p.setPen(dark ? QColor(255, 218, 140) : QColor(142, 86, 11));
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
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRect(14, 10, width() - 28, 30), Qt::TextWordWrap,
               present ? drawing.name + (editable ? " · Editing" : "") : "Choose Sketch to start");
    if (editable && drawing.entities.empty() && !circleCenter && !inputAnchor)
        p.drawText(QRect(14, height() - 32, width() - 28, 24),
                   tool == "Select / drag" ? "Choose a sketch tool above"
                                           : "Click to place geometry · Esc to select");
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
    if (e->button() != Qt::LeftButton || spaceDown)
        return;
    pressedAt = e->pos();
    primitivePressed = false;
    if (editable && tool != "Select / drag") {
        if (tool == "Circle") {
            secondPress = circleCenter.has_value();
            primitivePressed = true;
            if (secondPress)
                finishCircle(e->position());
            else {
                circleCenter = snapPoint(local(e->position()));
                cursor = *circleCenter;
                update();
            }
        } else {
            secondPress = inputAnchor.has_value();
            primitivePressed = tool == "Rectangle" || tool == "Line";
            auto p = snapPoint(local(e->position()));
            emit clicked(p.x, p.y);
        }
        return;
    }
    auto id = pick(e->position());
    emit selected(id);
    if (editable && !id.isEmpty()) {
        dragging = id;
        auto p = local(e->position());
        emit dragStarted(id, p.x, p.y);
    }
}
void SketchView::mouseMoveEvent(QMouseEvent *e) {
    if ((e->buttons() & Qt::MiddleButton) || (spaceDown && (e->buttons() & Qt::LeftButton))) {
        auto d = e->position() - previous;
        cameraCenter.x -= d.x() / scale;
        cameraCenter.y += d.y() / scale;
        update();
    }
    previous = e->pos();
    if (editable && (circleCenter || inputAnchor)) {
        cursor = snapPoint(local(e->position()));
        update();
    }
    if (!dragging.isEmpty() && (e->buttons() & Qt::LeftButton)) {
        auto p = local(e->position());
        emit dragged(dragging, p.x, p.y);
    }
}
void SketchView::finishCircle(QPointF at) {
    if (!circleCenter)
        return;
    auto center = *circleCenter, end = snapPoint(local(at));
    double r = sketch::length(center, end);
    if (r > 1e-9) {
        circleCenter.reset();
        emit circleCreated(center.x, center.y, r);
    }
    update();
}
void SketchView::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton)
        return;
    if (primitivePressed && !secondPress &&
        QLineF(pressedAt, e->position()).length() >= QApplication::startDragDistance()) {
        if (tool == "Circle")
            finishCircle(e->position());
        else if (inputAnchor) {
            auto p = snapPoint(local(e->position()));
            emit clicked(p.x, p.y);
        }
    }
    primitivePressed = false;
    if (!dragging.isEmpty()) {
        auto p = local(e->position());
        emit dragged(dragging, p.x, p.y);
        dragging.clear();
        emit dragFinished();
    }
}
void SketchView::wheelEvent(QWheelEvent *e) {
    if (!e->pixelDelta().isNull() && !(e->modifiers() & Qt::ControlModifier)) {
        cameraCenter.x -= e->pixelDelta().x() / scale;
        cameraCenter.y += e->pixelDelta().y() / scale;
        if (circleCenter || inputAnchor)
            cursor = snapPoint(local(previous));
        update();
    } else
        zoomAt(e->position(), std::exp(e->angleDelta().y() / 1000.));
    e->accept();
}
void SketchView::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        emit editRequested(pick(e->position()));
}
void SketchView::contextMenuEvent(QContextMenuEvent *e) {
    auto id = pick(e->pos());
    emit selected(id);
    QMenu menu(this);
    auto *edit = menu.addAction("Edit sketch");
    bool rectangle = !id.isEmpty() && sketch::rectangleOf(drawing, id).has_value();
    auto *remove = menu.addAction(rectangle ? "Delete rectangle" : "Delete selection");
    remove->setEnabled(editable && !id.isEmpty());
    auto *fitAction = menu.addAction("Zoom to fit");
    auto *chosen = menu.exec(e->globalPos());
    if (chosen == edit)
        emit editRequested(id);
    else if (chosen == remove)
        emit deleteRequested();
    else if (chosen == fitAction)
        fit();
}
void SketchView::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape)
        emit canceled();
    else if (e->key() == Qt::Key_Space) {
        spaceDown = true;
        setCursor(Qt::OpenHandCursor);
        e->accept();
    } else if (e->key() == Qt::Key_F)
        fit();
    else if (editable && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace))
        emit deleteRequested();
    else
        QWidget::keyPressEvent(e);
}
void SketchView::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) {
        spaceDown = false;
        unsetCursor();
        e->accept();
    } else
        QWidget::keyReleaseEvent(e);
}
} // namespace cad

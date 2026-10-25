#include "SketchView.hpp"
#include "sketch/Constructions.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Editing.hpp"
#include "sketch/Slots.hpp"
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QWheelEvent>
#include <cmath>
namespace cad {
namespace {
constexpr double pi = 3.14159265358979323846, tau = 2 * pi;
double angle(sketch::Vec2 center, sketch::Vec2 at) {
    return std::atan2(at.y - center.y, at.x - center.x);
}
} // namespace
SketchView::SketchView(QWidget *p) : QWidget(p) {
    setObjectName("sketchViewport");
    setAccessibleName("Planar sketch viewport");
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(240, 200);
}
void SketchView::showSketch(sketch::Sketch s, QString selected, bool editing) {
    bool changed = !present || drawing.map() != s.map();
    drawing = std::move(s);
    if (changed) {
        curveDisplay.clear();
        // At most 256 entities, 129 local display points each; no exact objects.
        for (auto &e : drawing.entities)
            if (sketch::advancedCurve(e.kind)) {
                auto [first, last] = sketch::curveRange(e);
                auto &points = curveDisplay[e.id];
                points.reserve(129);
                for (int i = 0; i <= 128; ++i)
                    points.push_back(
                        sketch::curveValue(drawing, e, first + (last - first) * i / 128));
            }
    }
    selectedId = selected;
    editable = editing;
    present = true;
    setAccessibleDescription(drawing.name + (editable ? " · Sketch draft" : " · Accepted sketch") +
                             (editable && arcTool() ? " · " + arcPrompt() : ""));
    if (editable && tool == "Rectangle")
        setRectangleInput(rectangleMethod, rectangleInputs);
    if (editable && tool == "Slot")
        setSlotInput(slotMethod, slotInputs);
    update();
}
void SketchView::clearSketch() {
    drawing = {};
    curveDisplay.clear();
    editable = present = false;
    setAccessibleDescription("No sketch · choose a support plane and New sketch");
    cancelPointerInput();
    update();
}
void SketchView::setTool(QString name) {
    cancelPointerInput();
    tool = std::move(name);
    if (arcTool())
        setAccessibleDescription(arcPrompt());
    update();
}
void SketchView::setInputAnchor(std::optional<sketch::Vec2> anchor) {
    if (anchor && !inputAnchor)
        cursor = *anchor;
    inputAnchor = anchor;
    update();
}
void SketchView::setRectangleInput(QString method, std::vector<sketch::Vec2> inputs) {
    rectangleMethod = std::move(method);
    rectangleInputs = std::move(inputs);
    if (tool == "Rectangle") {
        QString next =
            rectangleInputs.empty()
                ? (rectangleMethod.contains("Center") ? "center" : "first corner")
            : rectangleInputs.size() == 1 && sketch::rectangleInputCount(rectangleMethod) == 3
                ? (rectangleMethod.contains("Center") ? "edge midpoint" : "second corner")
                : "final corner";
        setAccessibleDescription(rectangleMethod + " Rectangle · choose " + next);
    }
    update();
}
void SketchView::setSlotInput(QString method, std::vector<sketch::Vec2> inputs) {
    slotMethod = std::move(method);
    slotInputs = std::move(inputs);
    if (tool == "Slot") {
        QString next = slotInputs.empty()
                           ? (slotMethod.contains("Centerpoint") ? "center" : "start center")
                       : int(slotInputs.size()) == sketch::slotInputCount(slotMethod) - 1 ? "width"
                       : slotInputs.size() == 1      ? "end center"
                       : slotMethod == "3 Point Arc" ? "arc bend"
                                                     : "end angle";
        setAccessibleDescription(slotMethod + " Slot · choose " + next + " · Esc to cancel");
    }
    update();
}
void SketchView::setCurveInput(QString kind, std::vector<sketch::Vec2> inputs, int degree,
                               bool closed) {
    curveInputs = std::move(inputs);
    curveDegree = degree;
    curveClosed = closed;
    if (sketch::advancedCurve(kind)) {
        QStringList stages =
            kind == "Parabola" ? QStringList{"focus", "vertex", "first extent", "last extent"}
                               : QStringList{"center", "major axis", "minor axis", "start", "end"};
        setAccessibleDescription(
            kind == "Spline"
                ? QString("Spline · add control points · Enter to complete · Esc to cancel")
                : kind + " · choose " +
                      stages[std::min(int(curveInputs.size()), int(stages.size() - 1))] +
                      " · Esc to cancel");
    }
    update();
}
bool SketchView::cancelPointerInput() {
    bool active = circleCenter.has_value() || inputAnchor.has_value() || arcCenter.has_value();
    circleCenter.reset();
    inputAnchor.reset();
    rectangleInputs.clear();
    slotInputs.clear();
    curveInputs.clear();
    arcCenter.reset();
    arcStart.reset();
    arcSweep = 0;
    tangentSeed.reset();
    tangentDirection.reset();
    arcPressed = false;
    normalIntent = false;
    continuation.reset();
    continuationAway = false;
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
        else if (sketch::advancedCurve(e.kind)) {
            for (auto point : curveDisplay.value(e.id))
                include(point);
        }
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
    if (circleCenter || inputAnchor || arcCenter)
        cursor = snapPoint(local(previous));
    if (arcCenter)
        moveArc(cursor);
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
        if (sketch::advancedCurve(e.kind)) {
            QPolygonF poly;
            for (auto point : curveDisplay.value(e.id))
                poly << screen(point);
            p.drawPolyline(poly);
        } else if (e.kind == "Line")
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
            else if (sketch::advancedCurve(found->kind)) {
                QPolygonF polygon;
                for (auto id : sketch::controlPoints(*found)) {
                    auto q = drawing.entity(id).point;
                    handle(q);
                    polygon << screen(q);
                }
                p.drawPolyline(polygon);
            } else if (found->kind == "Circle" || found->kind == "Arc") {
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

    } else if (arcCenter && tool == "Arc") {
        auto center = screen(*arcCenter);
        double radius = sketch::length(*arcCenter, arcStart ? *arcStart : cursor);
        auto start = arcStart ? *arcStart : cursor;
        auto first = screen(start);
        QRectF box(center.x() - radius * scale, center.y() - radius * scale, radius * scale * 2,
                   radius * scale * 2);
        p.drawLine(center, first);
        p.drawEllipse(center, 3, 3);
        p.drawEllipse(first, 3, 3);
        if (!arcStart)
            p.drawEllipse(box);
        else if (sketch::length(*arcCenter, cursor) > 1e-9) {
            double end = angle(*arcCenter, cursor);
            p.drawLine(center, screen({arcCenter->x + radius * std::cos(end),
                                       arcCenter->y + radius * std::sin(end)}));
            p.setPen(QPen(QColor(255, 197, 74), 2));
            p.drawArc(box, qRound(angle(*arcCenter, *arcStart) * 180 / pi * 16),
                      qRound(arcSweep * 180 / pi * 16));
        }
        QString units = property("units").toString();
        if (units.isEmpty())
            units = "mm";
        auto at = screen(cursor);
        QString label =
            "R " + QString::number(radius / domain::toSI(1, "length", units), 'g', 7) + " " + units;
        if (arcStart)
            label += " | " + QString::number(std::abs(arcSweep) * 180 / pi, 'g', 6) +
                     QChar(0x00b0) + (arcSweep < 0 ? " CW" : " CCW");
        at.setX(
            std::clamp(at.x() + 12, 14.,
                       std::max(14., width() - p.fontMetrics().horizontalAdvance(label) - 14.)));
        at.setY(std::clamp(at.y() - 15, 48., std::max(48., height() - 55.)));
        p.drawText(at, label);
    } else if (arcCenter) {
        p.drawEllipse(screen(*arcCenter), 3, 3);
        if (arcStart) {
            p.drawLine(screen(*arcCenter), screen(*arcStart));
            p.drawEllipse(screen(*arcStart), 3, 3);
        } else if (tool == "3 Point Arc")
            p.drawLine(screen(*arcCenter), screen(cursor));
        if (auto arc = arcPreview()) {
            auto center = screen(arc->center);
            double r = arc->radius * scale;
            p.setPen(QPen(QColor(255, 197, 74), 2));
            p.drawArc(QRectF(center.x() - r, center.y() - r, 2 * r, 2 * r),
                      qRound(arc->start * 180 / pi * 16), qRound(arc->sweep * 180 / pi * 16));
            auto units = property("units").toString();
            if (units.isEmpty())
                units = "mm";
            QString label =
                "R " + QString::number(arc->radius / domain::toSI(1, "length", units), 'g', 7) +
                " " + units + " | " + QString::number(arc->sweep * 180 / pi, 'g', 6) +
                QChar(0x00b0);
            if (tangentSeed)
                label += normalIntent ? " Normal" : " Tangent";
            auto at = screen(cursor);
            at.setX(std::clamp(
                at.x() + 12, 14.,
                std::max(14., width() - p.fontMetrics().horizontalAdvance(label) - 14.)));
            at.setY(std::clamp(at.y() - 15, 48., std::max(48., height() - 55.)));
            p.drawText(at, label);
        }
    } else if (inputAnchor) {
        if (sketch::advancedCurve(tool)) {
            auto inputs = curveInputs;
            inputs.push_back(cursor);
            try {
                sketch::Sketch preview;
                QString id;
                if (tool == "Spline" && inputs.size() >= size_t(curveDegree + 1))
                    id = sketch::createSpline(preview, inputs, curveDegree, curveClosed);
                else if (tool != "Spline" && int(inputs.size()) == sketch::curveInputCount(tool))
                    id = sketch::createCurveInput(preview, tool, inputs);
                else if (tool == "EllipticalArc" && inputs.size() >= 3)
                    id = sketch::createEllipse(preview, inputs[0], inputs[1], inputs[2]);
                if (!id.isEmpty()) {
                    auto &e = preview.entity(id);
                    auto [a, b] = sketch::curveRange(e);
                    QPolygonF polygon;
                    for (int i = 0; i <= 96; ++i)
                        polygon << screen(sketch::curveValue(preview, e, a + (b - a) * i / 96));
                    p.drawPolyline(polygon);
                }
            } catch (const domain::Error &) {
            }
            QPolygonF controls;
            for (auto q : inputs)
                controls << screen(q);
            p.drawPolyline(controls);
        } else if (tool == "Slot") {
            auto inputs = slotInputs;
            inputs.push_back(cursor);
            if (int(inputs.size()) == sketch::slotInputCount(slotMethod)) {
                try {
                    sketch::Sketch preview;
                    sketch::createSlot(preview, slotMethod, inputs);
                    for (auto &e : preview.entities)
                        if (e.kind == "Line")
                            p.drawLine(screen(preview.entity(e.a).point),
                                       screen(preview.entity(e.b).point));
                        else if (e.kind == "Arc") {
                            auto c = screen(preview.entity(e.a).point);
                            double r = e.radius * scale;
                            p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r),
                                      qRound(e.start * 180 / sketch::arcPi * 16),
                                      qRound(e.sweep * 180 / sketch::arcPi * 16));
                        }
                } catch (const domain::Error &) {
                }
            } else
                p.drawLine(screen(*inputAnchor), screen(cursor));
        } else if (tool == "Rectangle") {
            auto inputs = rectangleInputs;
            inputs.push_back(cursor);
            if (int(inputs.size()) == sketch::rectangleInputCount(rectangleMethod)) {
                try {
                    auto corners = sketch::rectangleVertices(rectangleMethod, inputs);
                    QPolygonF polygon;
                    for (auto q : corners)
                        polygon << screen(q);
                    p.drawPolygon(polygon);
                    if (rectangleMethod.contains("Center")) {
                        p.drawLine(screen(corners[0]), screen(corners[2]));
                        p.drawLine(screen(corners[1]), screen(corners[3]));
                    }
                } catch (const domain::Error &) {
                }
            } else
                p.drawLine(screen(*inputAnchor), screen(cursor));
        } else
            p.drawLine(screen(*inputAnchor), screen(cursor));
    }
    if (editable && (tool == "Rectangle" || tool == "Slot" || sketch::advancedCurve(tool))) {
        p.setPen(dark ? QColor(255, 218, 140) : QColor(142, 86, 11));
        p.drawText(QPointF(14, 52), accessibleDescription());
    }
    p.setPen(dark ? QColor(255, 218, 140) : QColor(142, 86, 11));
    QVector<QRectF> dimensionLabels;
    static const QRegularExpression literal("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?["
                                            "0-9]+)?(?:\\s+(?:mm|cm|m|in|deg|rad))?$");
    for (auto &c : drawing.constraints)
        if (QStringList{"Length", "Distance", "Angle", "Radius", "Diameter", "SlotLength", "Sweep"}
                .contains(c.kind)) {
            auto &e = drawing.entity(c.a);
            auto pos = screen(e.kind == "Point" ? e.point : drawing.entity(e.a).point);
            if (e.kind == "Line") {
                auto end = screen(drawing.entity(e.b).point);
                pos = (pos + end) / 2;
            }
            QString unit =
                (c.kind == "Angle" || c.kind == "Sweep") ? "deg" : property("units").toString();
            if (unit.isEmpty())
                unit = "mm";
            double n = c.value /
                       domain::toSI(
                           1, (c.kind == "Angle" || c.kind == "Sweep") ? "angle" : "length", unit);
            QString expression = c.expression.trimmed();
            bool named = !expression.isEmpty() && !literal.match(expression).hasMatch();
            QString label = c.kind + " " + QString::number(n, 'g', 7) + " " + unit;
            if (named)
                label += " [" + p.fontMetrics().elidedText(expression, Qt::ElideRight, 180) + "]";
            double labelWidth = p.fontMetrics().horizontalAdvance(label);
            double labelHeight = p.fontMetrics().height();
            auto anchor = pos;
            pos.setX(std::clamp(pos.x() + 8, 12.0, std::max(12.0, width() - labelWidth - 12)));
            pos.setY(std::clamp(pos.y() - 12, 65.0, std::max(65.0, height() - 35.0)));
            QRectF bounds(pos.x() - 3, pos.y() - p.fontMetrics().ascent() - 2, labelWidth + 6,
                          labelHeight + 4);
            auto base = bounds;
            int rows = std::max(1, int(height() / (labelHeight + 8)));
            for (int row = 0; row < 2 * rows; ++row) {
                auto candidate =
                    base.translated(0, (row % 2 ? 1 : -1) * ((row + 1) / 2) * (labelHeight + 8));
                if (candidate.top() < 48 || candidate.bottom() > height() - 30)
                    continue;
                bool overlaps =
                    std::any_of(dimensionLabels.begin(), dimensionLabels.end(), [&](auto &placed) {
                        return placed.adjusted(-2, -2, 2, 2).intersects(candidate);
                    });
                if (!overlaps) {
                    bounds = candidate;
                    break;
                }
            }
            dimensionLabels.push_back(bounds);
            p.save();
            p.setPen(QPen(dark ? QColor(255, 218, 140, 110) : QColor(142, 86, 11, 110), 1));
            p.drawLine(anchor, bounds.topLeft() + QPointF(0, bounds.height() / 2));
            p.restore();
            p.fillRect(bounds, dark ? QColor(25, 31, 40, 235) : QColor(250, 250, 250, 235));
            p.drawText(bounds.topLeft() + QPointF(3, p.fontMetrics().ascent() + 2), label);
        }
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRect(14, 10, width() - 28, 30), Qt::TextWordWrap,
               present ? drawing.name + (editable ? " · Editing" : "") : "Choose Sketch to start");
    if (editable && arcTool()) {
        p.drawText(QRect(14, height() - 42, width() - 28, 38), Qt::TextWordWrap, arcPrompt());
    } else if (editable && drawing.entities.empty() && !circleCenter && !inputAnchor)
        p.drawText(QRect(14, height() - 32, width() - 28, 24),
                   tool == "Select / drag" ? "Choose a sketch tool above"
                                           : "Click to place geometry · Esc to select");
}
bool SketchView::arcTool() const {
    return tool == "Arc" || tool == "3 Point Arc" || tool == "Tangent Arc";
}
QString SketchView::arcPrompt() const {
    if (tool == "3 Point Arc") {
        if (!arcCenter)
            return "3 Point Arc: click the start point · Esc to select";
        if (!arcStart)
            return "Move/click the end point, then move to bend the arc · Esc to cancel";
        return arcPreview() ? "Move to size the bend/radius, then click to finish · Esc to cancel"
                            : "Move off the chord to make a nondegenerate arc · Esc to cancel";
    }
    if (tool == "Tangent Arc") {
        if (!tangentSeed)
            return "Tangent Arc: choose a line/arc endpoint (select the source curve at shared "
                   "endpoints)";
        return arcPreview()
                   ? "Click or release to finish · Return to the start to change tangent/normal "
                     "intent · Esc to cancel"
                   : "Move away along the tangent or normal, then bend the arc · Esc to cancel";
    }
    if (!arcCenter)
        return "Centerpoint arc: click the center · Esc to select";
    if (!arcStart)
        return "Move/click the start point to size radius/angle, or drag/release · Esc to cancel";
    if (!arcPreview())
        return "Move around the center to choose an end point (between 0 and 360 degrees) · Esc to "
               "cancel";
    return "Click the end point to finish the arc · Esc to cancel";
}
std::optional<sketch::ArcConstruction> SketchView::arcPreview() const {
    if (!arcCenter)
        return {};
    if (tool == "3 Point Arc")
        return arcStart ? sketch::threePointArc(*arcCenter, *arcStart, cursor) : std::nullopt;
    if (tool == "Tangent Arc")
        return tangentDirection ? sketch::tangentArc(*arcCenter, *tangentDirection, cursor)
                                : std::nullopt;
    if (!arcStart || sketch::length(*arcCenter, cursor) <= 1e-9 || std::abs(arcSweep) <= 1e-8 ||
        std::abs(arcSweep) >= tau - 1e-8)
        return {};
    double start = angle(*arcCenter, *arcStart);
    if (arcSweep < 0)
        start += arcSweep;
    return sketch::ArcConstruction{*arcCenter, sketch::length(*arcCenter, *arcStart),
                                   std::remainder(start, tau), std::abs(arcSweep),
                                   arcSweep < 0 ? 1 : 0};
}
std::optional<SketchView::Endpoint> SketchView::endpointAt(QPointF at) const {
    std::optional<Endpoint> found;
    double best = 8.;
    bool ambiguous = false;
    for (auto &e : drawing.entities)
        if (e.kind == "Line" || e.kind == "Arc")
            for (int end = 0; end < 2; ++end) {
                auto point = sketch::curveEnd(drawing, e, end);
                double distance = QLineF(at, screen(point)).length();
                if (distance > 8)
                    continue;
                if (e.id == selectedId)
                    return Endpoint{e.id, end, point, sketch::outgoing(drawing, e, end)};
                if (distance < best - 0.1) {
                    found = Endpoint{e.id, end, point, sketch::outgoing(drawing, e, end)};
                    best = distance;
                    ambiguous = false;
                } else if (found && std::abs(distance - best) <= 0.1 && found->curve != e.id)
                    ambiguous = true;
            }
    return ambiguous ? std::nullopt : found;
}
void SketchView::beginTangent(QString curve, int end) {
    auto &e = drawing.entity(curve);
    tangentSeed =
        Endpoint{curve, end, sketch::curveEnd(drawing, e, end), sketch::outgoing(drawing, e, end)};
    arcCenter = tangentSeed->point;
    cursor = *arcCenter;
    tangentDirection.reset();
    normalIntent = false;
    setAccessibleDescription(arcPrompt());
    update();
}
void SketchView::continueFrom(QString curve, int end) {
    auto &e = drawing.entity(curve);
    continuation =
        Endpoint{curve, end, sketch::curveEnd(drawing, e, end), sketch::outgoing(drawing, e, end)};
    continuationAway = false;
}
void SketchView::finishArc() {
    auto arc = arcPreview();
    if (!arc)
        return;
    auto source = tangentSeed;
    bool normal = normalIntent;
    cancelPointerInput();
    if (source)
        emit arcJoined(arc->center.x, arc->center.y, arc->radius, arc->start, arc->sweep,
                       source->curve, arc->originalStart, source->end, normal);
    else
        emit arcCreated(arc->center.x, arc->center.y, arc->radius, arc->start, arc->sweep);
}
void SketchView::moveArc(sketch::Vec2 at) {
    cursor = at;
    if (tool == "Tangent Arc" && tangentSeed) {
        if (sketch::length(*arcCenter, at) <= 8. / scale) {
            tangentDirection.reset();
            normalIntent = false;
        } else if (!tangentDirection) {
            auto t = tangentSeed->direction;
            sketch::Vec2 delta{at.x - arcCenter->x, at.y - arcCenter->y}, n{-t.y, t.x};
            double along = delta.x * t.x + delta.y * t.y, across = delta.x * n.x + delta.y * n.y;
            normalIntent = std::abs(across) > std::abs(along);
            auto v = normalIntent ? n : t;
            double sign = (normalIntent ? across : along) < 0 ? -1 : 1;
            tangentDirection = sketch::Vec2{v.x * sign, v.y * sign};
        }
    } else if (tool == "Arc" && arcStart && sketch::length(*arcCenter, cursor) > 1e-9) {
        double next = angle(*arcCenter, cursor);
        arcSweep = std::clamp(arcSweep + std::remainder(next - arcLastAngle, tau), -tau, tau);
        arcLastAngle = next;
    }
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
            if (sketch::advancedCurve(e.kind)) {
                auto points = curveDisplay.constFind(e.id);
                if (points == curveDisplay.cend() || points->empty())
                    continue;
                auto a = screen(points->front());
                for (size_t i = 1; i < points->size(); ++i) {
                    auto b = screen((*points)[i]), v = b - a;
                    double n = QPointF::dotProduct(v, v);
                    double t = n > 0 ? std::clamp(QPointF::dotProduct(p - a, v) / n, 0., 1.) : 0.;
                    distance = std::min(distance, QLineF(p, a + t * v).length());
                    a = b;
                }
            } else if (e.kind == "Line") {
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
        if (arcTool()) {
            arcPressed = true;
            arcPressStage = arcCenter ? (arcStart ? 2 : 1) : 0;
            auto at = snapPoint(local(e->position()));
            cursor = at;
            if (tool == "Tangent Arc") {
                if (!tangentSeed) {
                    tangentSeed = endpointAt(e->position());
                    if (tangentSeed) {
                        arcCenter = tangentSeed->point;
                        cursor = *arcCenter;
                    }
                } else {
                    moveArc(at);
                    finishArc();
                }
            } else if (!arcCenter) {
                arcCenter = at;
                arcSweep = 0;
            } else if (!arcStart) {
                if (sketch::length(*arcCenter, at) > 1e-9) {
                    arcStart = at;
                    arcLastAngle = angle(*arcCenter, at);
                    arcSweep = 0;
                }
            } else {
                moveArc(at);
                finishArc();
            }
            setAccessibleDescription(arcPrompt());
            update();
        } else if (tool == "Circle") {
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
            primitivePressed = tool == "Rectangle" || tool == "Line" || tool == "Slot" ||
                               sketch::advancedCurve(tool);
            auto p = snapPoint(local(e->position()));
            if (!((sketch::advancedCurve(tool) && !curveInputs.empty() &&
                   sketch::length(p, curveInputs.back()) < 1e-9) ||
                  (tool == "Rectangle" && rectangleInputs.size() == 2 &&
                   sketch::length(p, rectangleInputs.back()) < 1e-9) ||
                  (tool == "Slot" && !slotInputs.empty() &&
                   sketch::length(p, slotInputs.back()) < 1e-9)))
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
    if (editable && continuation && (tool == "Line" || tool == "Polyline") &&
        !(e->buttons() & Qt::MiddleButton) && !spaceDown) {
        double distance = QLineF(e->position(), screen(continuation->point)).length();
        if (distance > 20)
            continuationAway = true;
        else if (continuationAway && distance <= 8) {
            auto next = *continuation;
            continuation.reset();
            emit tangentTransition(next.curve, next.end);
        }
    }
    if (editable && arcCenter) {
        moveArc(snapPoint(local(e->position())));
        if (accessibleDescription() != arcPrompt())
            setAccessibleDescription(arcPrompt());
        update();
    }
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
    if (e->button() == Qt::LeftButton && arcPressed) {
        arcPressed = false;
        if (arcCenter &&
            QLineF(pressedAt, e->position()).length() >= QApplication::startDragDistance()) {
            auto at = snapPoint(local(e->position()));
            if (tool == "Tangent Arc") {
                moveArc(at);
                finishArc();
            } else if (arcPressStage == 0 && !arcStart && sketch::length(*arcCenter, at) > 1e-9) {
                arcStart = at;
                arcLastAngle = angle(*arcCenter, at);
                arcSweep = 0;
                cursor = at;
            }
        }
        setAccessibleDescription(arcPrompt());
        update();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    if (primitivePressed &&
        (!secondPress || tool == "Rectangle" || tool == "Slot" || sketch::advancedCurve(tool)) &&
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
    else if (editable && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) &&
             tool == "Spline")
        emit completePrimitive();
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

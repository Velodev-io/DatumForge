#include "Editing.hpp"
#include "Constructions.hpp"
#include "Curves.hpp"
#include "Slots.hpp"
#include <QSet>
namespace cad::sketch {
std::optional<Rectangle> rectangleOf(const Sketch &s, QString id) {
    if (auto g = constructionOf(s, id); g && g->kind == "Rectangle") {
        Rectangle r;
        for (int i = 0; i < 4; ++i) {
            r.points[i] = g->vertices[i];
            r.lines[i] = g->edges[i];
        }
        return r;
    }
    auto &selected = s.entity(id);
    QString first = selected.kind == "Point"  ? id
                    : selected.kind == "Line" ? selected.a
                                              : QString();
    if (first.isEmpty())
        return {};
    Rectangle r;
    QString current = first, previous;
    for (int i = 0; i < 4; ++i) {
        r.points[i] = current;
        std::vector<const Entity *> edges;
        for (auto &e : s.entities)
            if (e.kind == "Line" && (e.a == current || e.b == current))
                edges.push_back(&e);
        if (edges.size() != 2)
            return {};
        auto *line = edges[0]->id == previous ? edges[1] : edges[0];
        r.lines[i] = line->id;
        current = line->a == current ? line->b : line->a;
        previous = line->id;
        if (i < 3 && current == first)
            return {};
    }
    if (current != first)
        return {};
    for (int i = 0; i < 4; ++i) {
        auto p = s.entity(r.points[i]).point, q = s.entity(r.points[(i + 1) % 4]).point;
        bool horizontal = std::abs(p.y - q.y) < 1e-10, vertical = std::abs(p.x - q.x) < 1e-10;
        if (horizontal == vertical)
            return {};
        auto next = s.entity(r.points[(i + 2) % 4]).point;
        if (horizontal == (std::abs(q.y - next.y) < 1e-10))
            return {};
    }
    return r;
}
namespace {
void movable(const Sketch &s, const QSet<QString> &ids) {
    for (auto &c : s.constraints)
        if (c.driving && c.kind != "Horizontal" && c.kind != "Vertical" &&
            (ids.contains(c.a) || ids.contains(c.b) || ids.contains(c.third)))
            throw domain::Error("ConstrainedGeometry",
                                "Controlled by " + c.kind +
                                    ". Edit its dimension or relation in Properties.");
}
} // namespace
Sketch dragEntity(const Sketch &s, QString id, Vec2 from, Vec2 to) {
    auto &e = s.entity(id);
    // A selection click must not reconstruct geometry or evaluate drag controls.
    if (from.x == to.x && from.y == to.y)
        return s;
    if (auto g = constructionOf(s, id))
        return g->kind == "Slot" ? dragSlot(s, *g, {to.x - from.x, to.y - from.y})
                                 : dragConstruction(s, *g, id, {to.x - from.x, to.y - from.y});
    auto result = s;
    Vec2 delta{to.x - from.x, to.y - from.y};
    QSet<QString> affected{id};
    if (auto rect = rectangleOf(s, id)) {
        for (auto &p : rect->points)
            affected.insert(p);
        for (auto &l : rect->lines)
            affected.insert(l);
        movable(s, affected);
        if (e.kind == "Point") {
            auto p = e.point;
            for (auto &point : rect->points) {
                auto q = s.entity(point).point;
                if (std::abs(q.x - p.x) < 1e-10)
                    result.entity(point).point.x = q.x + delta.x;
                if (std::abs(q.y - p.y) < 1e-10)
                    result.entity(point).point.y = q.y + delta.y;
            }
            for (auto &l : rect->lines) {
                auto &line = s.entity(l);
                auto a = s.entity(line.a).point, b = s.entity(line.b).point;
                auto c = result.entity(line.a).point, d = result.entity(line.b).point;
                domain::check((b.x - a.x) * (d.x - c.x) + (b.y - a.y) * (d.y - c.y) > 0,
                              "InvalidGeometry",
                              "Keep this corner on its current side of the opposite corner");
            }
        } else
            for (auto &p : rect->points) {
                auto q = s.entity(p).point;
                result.entity(p).point = {q.x + delta.x, q.y + delta.y};
            }
    } else if (advancedCurve(e.kind)) {
        auto controls = controlPoints(e);
        QSet<QString> points(controls.begin(), controls.end());
        affected.unite(points);
        movable(s, affected);
        for (auto id : points) {
            auto p = s.entity(id).point;
            result.entity(id).point = {p.x + delta.x, p.y + delta.y};
        }
    } else if (e.kind == "Circle" || e.kind == "Arc") {
        movable(s, affected);
        auto p = s.entity(e.a).point;
        result.entity(id).radius = e.radius + length(p, to) - length(p, from);
    } else {
        QSet<QString> points;
        if (e.kind == "Point")
            points.insert(id);
        else {
            points.insert(e.a);
            points.insert(e.b);
        }
        for (auto &curve : s.entities)
            for (auto point : controlPoints(curve))
                if (points.contains(point))
                    affected.insert(curve.id);
        affected.unite(points);
        movable(s, affected);
        for (auto &p : points) {
            auto q = s.entity(p).point;
            result.entity(p).point = {q.x + delta.x, q.y + delta.y};
        }
    }
    result.validate();
    return result;
}
Sketch eraseEntity(const Sketch &s, QString id) {
    if (auto g = constructionOf(s, id))
        return eraseConstruction(s, *g);
    auto result = s;
    if (s.entity(id).kind == "Point")
        for (auto &curve : s.entities)
            if (advancedCurve(curve.kind) && controlPoints(curve).contains(id))
                throw domain::Error("ReferenceMissing",
                                    "This point controls an exact curve; edit its control geometry "
                                    "or delete the curve explicitly");
    QSet<QString> removed{id}, support;
    if (auto r = rectangleOf(s, id))
        for (auto &l : r->lines)
            removed.insert(l);
    for (auto &e : s.entities)
        if (e.a == id || e.b == id)
            removed.insert(e.id);
    for (auto &e : s.entities)
        if (removed.contains(e.id)) {
            if (advancedCurve(e.kind))
                for (auto id : controlPoints(e))
                    support.insert(id);
            if (!e.a.isEmpty())
                support.insert(e.a);
            if (!e.b.isEmpty())
                support.insert(e.b);
        }
    for (auto &e : s.entities)
        if (!removed.contains(e.id)) {
            if (advancedCurve(e.kind))
                for (auto id : controlPoints(e))
                    support.remove(id);
            support.remove(e.a);
            support.remove(e.b);
        }
    removed.unite(support);
    std::erase_if(result.entities, [&](auto &e) { return removed.contains(e.id); });
    std::erase_if(result.constraints, [&](auto &c) {
        return removed.contains(c.a) || removed.contains(c.b) || removed.contains(c.third);
    });
    result.validate();
    return result;
}
} // namespace cad::sketch

#include "sketch/Solver.hpp"
#include "sketch/ArcGeometry.hpp"
#include "support/check.hpp"
using namespace cad::sketch;
Constraint add(Sketch &s, QString kind, QString a, QString b = {}, double value = 0,
               double second = 0) {
    Constraint c;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.value = value;
    c.second = second;
    s.constraints.push_back(c);
    return c;
}
Sketch rectangle() {
    Sketch s;
    auto lines = s.rectangle({0, 0}, {.08, .05});
    add(s, "Fixed", s.entity(lines[0]).a);
    for (int i = 0; i < 4; ++i)
        add(s, i % 2 ? "Vertical" : "Horizontal", lines[i]);
    add(s, "Length", lines[0], {}, .08);
    add(s, "Length", lines[1], {}, .05);
    return s;
}
int main() {
    return run([] {
        auto s = rectangle();
        auto r = solve(s);
        CHECK(r.valid);
        CHECK(r.status == "FullyDefined");
        CHECK(r.freedom == 0);
        near(r.sketch.entities[2].point.x, .08, 1e-10);
        near(r.sketch.entities[2].point.y, .05, 1e-10);
        auto under = s;
        under.constraints.pop_back();
        r = solve(under);
        CHECK(r.valid);
        CHECK(r.freedom == 1);
        Sketch circle;
        auto center = circle.point({0, 0});
        auto c = circle.circle(center, .004);
        add(circle, "Fixed", center);
        add(circle, "Radius", c, {}, .002);
        r = solve(circle);
        CHECK(r.valid);
        CHECK(r.freedom == 0);
        near(r.sketch.entity(c).radius, .002, 1e-11);
        Sketch free;
        free.arc(free.point({0, 0}), .01, 0, 1.5);
        r = solve(free);
        CHECK(r.valid);
        CHECK(r.freedom == 5);
        Sketch joined;
        auto joinLine = joined.line(joined.point({-.02, 0}), joined.point({0, 0}));
        auto ja = joined.arc(joined.point({0, .01}), .01, -1.5707963267948966, 1.5707963267948966);
        add(joined, "ArcTangent", ja, joinLine, 0, 1);
        add(joined, "Radius", ja, {}, .01);
        r = solve(joined);
        CHECK(r.valid);
        Sketch storedJoin;
        auto support = storedJoin.line(storedJoin.point({-.02, 0}), storedJoin.point({0, 0}));
        auto storedCenter = storedJoin.point({0, .01});
        auto storedArc = storedJoin.arc(storedCenter, .01, -arcPi / 2, arcPi / 2);
        add(storedJoin, "Fixed", storedJoin.entity(support).a, {}, -.02, 0);
        add(storedJoin, "Fixed", storedJoin.entity(support).b);
        add(storedJoin, "Fixed", storedCenter, {}, 0, .01);
        add(storedJoin, "Radius", storedArc, {}, .01);
        add(storedJoin, "ArcTangent", storedArc, support, 0, 1);
        auto storedResult = solve(storedJoin);
        CHECK(storedResult.valid);
        storedJoin = storedResult.sketch;
        validateAccepted(encode({storedJoin}));
        storedJoin.entity(storedArc).start += .1;
        rejects([&] { validateAccepted(encode({storedJoin})); });
        auto switchedJoin = joined;
        switchedJoin.entity(switchedJoin.entity(ja).a).point = {0, -.01};
        switchedJoin.entity(ja).start = arcPi / 2;
        CHECK(!solve(switchedJoin, &joined).valid);
        for (double factor : {.001, 1., 1000.})
            for (bool sourceArc : {false, true})
                for (bool normal : {false, true})
                    for (bool clockwise : {false, true}) {
                        Sketch g;
                        QString source;
                        if (sourceArc)
                            source = g.arc(g.point({0, 0}), .01 * factor, 0, arcPi / 2);
                        else
                            source = g.line(g.point({-.02 * factor, 0}), g.point({0, 0}));
                        auto point = curveEnd(g, g.entity(source), 1);
                        auto direction = outgoing(g, g.entity(source), 1);
                        if (normal)
                            direction = {-direction.y, direction.x};
                        double side = clockwise ? -1 : 1;
                        Vec2 end{point.x + .01 * factor * (direction.x - side * direction.y),
                                 point.y + .01 * factor * (direction.y + side * direction.x)};
                        auto construction = tangentArc(point, direction, end);
                        CHECK(construction.has_value());
                        auto arc = g.arc(g.point(construction->center), construction->radius,
                                         construction->start, construction->sweep);
                        auto relation = add(g, normal ? "ArcNormal" : "ArcTangent", arc, source,
                                            construction->originalStart, 1);
                        auto rr = solve(Sketch::parse(g.map()));
                        CHECK(rr.valid);
                        near(length(curveEnd(rr.sketch, rr.sketch.entity(arc), int(relation.value)),
                                    curveEnd(rr.sketch, rr.sketch.entity(source), 1)),
                             0, 1e-8 * factor);
                        auto a = outgoing(rr.sketch, rr.sketch.entity(arc), int(relation.value)),
                             b = outgoing(rr.sketch, rr.sketch.entity(source), 1);
                        near(normal ? a.x * b.x + a.y * b.y : a.x * b.y - a.y * b.x, 0, 1e-7);
                        // Geometry changes still resolve through the stored endpoint graph.
                        add(g, "Radius", arc, {}, construction->radius * 1.1);
                        rr = solve(g);
                        CHECK(rr.valid);
                        near(rr.sketch.entity(arc).radius, construction->radius * 1.1,
                             1e-8 * factor);
                        g.constraints[0].value = 2;
                        CHECK(!solve(g).valid);
                    }
        Sketch endpointLimit;
        QStringList manyArcs;
        for (int i = 0; i < 40; ++i)
            manyArcs.append(endpointLimit.arc(endpointLimit.point({0, 0}), .001, 0, arcPi));
        for (int i = 0; i < 40; ++i)
            add(endpointLimit, "ArcCoincident", manyArcs[i], manyArcs[(i + 1) % 40], 0, 0);
        auto limited = solve(endpointLimit);
        CHECK(!limited.valid);
        CHECK(limited.message.contains("ResourceLimit"));
        auto duplicate = s;
        duplicate.constraints.push_back(s.constraints.back());
        duplicate.constraints.back().id = cad::newId();
        r = solve(duplicate);
        CHECK(r.valid);
        CHECK(r.status == "Redundant");
        CHECK(!r.redundant.empty());
        auto conflict = s;
        add(conflict, "Length", s.constraints.back().a, {}, .06);
        r = solve(conflict);
        CHECK(!r.valid);
        CHECK(r.status == "Inconsistent");
        CHECK(!r.conflicting.empty());
        CHECK(conflict.map() != s.map());
        auto bad = circle;
        bad.entity(c).radius = -1;
        CHECK(!solve(bad).valid);
        bad = circle;
        bad.entity(center).point.x = INFINITY;
        CHECK(!solve(bad).valid);
        auto dragged = under;
        for (auto &e : dragged.entities)
            if (e.kind == "Point" && e.point.y > 0)
                e.point.y = .07;
        r = solve(dragged, &under);
        CHECK(r.valid);
        near(r.sketch.entities[2].point.y, .07, 1e-10);
        auto flipped = s;
        for (auto &e : flipped.entities)
            if (e.kind == "Point")
                e.point.x = -e.point.x;
        CHECK(!solve(flipped, &s).valid);
        auto moved = s;
        moved.entities[0].point = {.001, .001};
        r = solve(moved, &s);
        CHECK(r.valid);
        near(r.sketch.entities[0].point.x, 0, 1e-10);
        near(r.sketch.entities[0].point.y, 0, 1e-10);
        for (double factor : {.001, 1.0, 1000.0}) {
            auto scaled = s;
            for (auto &e : scaled.entities)
                if (e.kind == "Point") {
                    e.point.x *= factor;
                    e.point.y *= factor;
                }
            for (auto &c : scaled.constraints)
                if (c.kind == "Length")
                    c.value *= factor;
            r = solve(scaled);
            CHECK(r.valid);
            CHECK(r.freedom == 0);
            near(r.sketch.entities[2].point.y, .05 * factor, 1e-9);
        }
        for (QString kind : {"Parallel", "Perpendicular", "Equal", "Angle"}) {
            Sketch two;
            auto a = two.line(two.point({0, 0}), two.point({.01, 0}));
            auto b = two.line(two.point({0, .02}), two.point({.012, .025}));
            add(two, kind, a, b, .7);
            r = solve(two);
            CHECK(r.valid);
            CHECK(r.maximumResidual < 1e-7);
        }
        Sketch tangent;
        auto line = tangent.line(tangent.point({0, 0}), tangent.point({.02, 0}));
        auto disk = tangent.circle(tangent.point({.01, .007}), .005);
        add(tangent, "Tangent", line, disk);
        r = solve(tangent);
        CHECK(r.valid);
        CHECK(r.maximumResidual < 1e-7);
        Sketch branch;
        auto ap = branch.point({0, 0}), bp = branch.point({.02, 0});
        auto axis = branch.line(ap, bp);
        auto cp = branch.point({.01, .005});
        auto disk2 = branch.circle(cp, .005);
        add(branch, "Fixed", ap);
        add(branch, "Fixed", bp, {}, .02, 0);
        add(branch, "Radius", disk2, {}, .005);
        add(branch, "Tangent", axis, disk2);
        auto branchResult = solve(branch);
        CHECK(branchResult.valid);
        auto otherSide = branchResult.sketch;
        otherSide.entity(cp).point.y = -.005;
        CHECK(!solve(otherSide, &branchResult.sketch).valid);
        Sketch equal;
        auto ca = equal.circle(equal.point({0, 0}), .003),
             cb = equal.circle(equal.point({.02, 0}), .004);
        add(equal, "Equal", ca, cb);
        CHECK(solve(equal).valid);
        Sketch coincident;
        auto pa = coincident.point({0, 0}), pb = coincident.point({.002, .003});
        add(coincident, "Coincident", pa, pb);
        r = solve(coincident);
        CHECK(r.valid);
        CHECK(r.freedom == 2);
        near(length(r.sketch.entity(pa).point, r.sketch.entity(pb).point), 0, 1e-10);
        Sketch distance;
        auto da = distance.point({0, 0}), db = distance.point({.003, .004});
        add(distance, "Distance", da, db, .01);
        r = solve(distance);
        CHECK(r.valid);
        near(length(r.sketch.entity(da).point, r.sketch.entity(db).point), .01, 1e-10);
        add(distance, "Distance", da, db, 0);
        CHECK(!solve(distance).valid);
        auto singular = s;
        singular.entity(singular.entities[4].id).b = singular.entities[4].a;
        CHECK(!solve(singular).valid);
        auto unsupported = circle;
        add(unsupported, "Parallel", c, center);
        CHECK(!solve(unsupported).valid);
        auto named = s;
        named.names.insert("W", "90 mm");
        named.constraints[5].expression = "W";
        r = solve(named);
        CHECK(r.valid);
        near(r.sketch.entities[2].point.x, .09, 1e-10);
        named.constraints[5].expression = "3 deg";
        CHECK(!solve(named).valid);
        named.constraints[5].expression = "-3 mm";
        CHECK(!solve(named).valid);
        std::cout << "3.2b: redundancy/conflict IDs, independent residual/rank checks, drag "
                     "continuity, anchors, singular/degenerate and multi-scale corpus\n";
        std::cout << "3.2a: real pinned PlaneGCS rectangle/circle, normalized SI variables, fixed "
                     "anchors, rank-based freedom, free arc angles\n";
    });
}

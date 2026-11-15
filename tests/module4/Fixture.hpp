#pragma once
#include "features/Model.hpp"
namespace cad::fixture4 {
using sketch::fields;
struct B1 {
    sketch::Sketch outer, cut;
    features::Feature base, cutFeature, hole, x, y, xy;
    B1() {
        outer.name = "B1 outer";
        outer.rectangle({0, 0}, {.08, .05});
        cut.name = "B1 cut";
        cut.rectangle({.02, .02}, {.04, .04});
        base.name = "Base";
        base.kind = "Extrude";
        base.parameters = fields({{"sketch", outer.id},
                                  {"operation", "NewBody"},
                                  {"target", ""},
                                  {"depth", .01},
                                  {"extent", "Forward"}});
        cutFeature.name = "Cut";
        cutFeature.kind = "Extrude";
        cutFeature.parameters = fields({{"sketch", cut.id},
                                        {"operation", "Cut"},
                                        {"target", base.id},
                                        {"depth", .01},
                                        {"extent", "Forward"}});
        hole.name = "Hole";
        hole.kind = "Hole";
        hole.parameters = fields({{"target", base.id},
                                  {"origin", QCborArray{.01, .01, .01}},
                                  {"direction", QCborArray{0., 0., -1.}},
                                  {"diameter", .004},
                                  {"depth", .01},
                                  {"through", true}});
        x.name = "Hole X";
        x.kind = "LinearPattern";
        x.parameters = fields({{"source", hole.id},
                               {"target", base.id},
                               {"operation", "Cut"},
                               {"direction", QCborArray{1., 0., 0.}},
                               {"spacing", .05},
                               {"count", 2}});
        y = x;
        y.id = newId();
        y.name = "Hole Y";
        y.parameters.insert(QStringLiteral("direction"), QCborArray{0., 1., 0.});
        y.parameters.insert(QStringLiteral("spacing"), .03);
        xy = y;
        xy.id = newId();
        xy.name = "Hole XY";
        xy.parameters.insert(QStringLiteral("source"), x.id);
    }
    QCborArray sketches() const {
        return sketch::encode({outer, cut});
    }
    QCborArray features() const {
        return features::encode({base, cutFeature, hole, x, y, xy});
    }
    features::Feature fillet(QString edge) const {
        features::Feature f;
        f.name = "B1 fillet";
        f.kind = "Fillet";
        f.parameters = fields(
            {{"target", base.id}, {"source", xy.id}, {"edges", QCborArray{edge}}, {"size", .002}});
        return f;
    }
};
inline QString filletEdge(QCborArray topology) {
    for (auto t : topology) {
        auto m = t.toMap();
        auto c = m.value("center_si").toArray();
        if (c.size() == 3 && m.value("token").toString().startsWith("edge/") &&
            std::abs(c[0].toDouble()) < 1e-10 && std::abs(c[1].toDouble()) < 1e-10 &&
            std::abs(c[2].toDouble() - .005) < 1e-10)
            return m.value("token").toString();
    }
    return {};
}
} // namespace cad::fixture4

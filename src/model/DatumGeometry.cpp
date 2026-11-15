#include "DatumGeometry.hpp"
#include "domain/Documents.hpp"
#include <QSet>
namespace cad::model {
std::vector<DatumGeometry> DatumGeometry::parse(const QCborArray &values) {
    domain::check(values.size() <= 132, "ResourceLimit", "Datum display admission exceeded");
    QSet<QString> seen;
    std::vector<DatumGeometry> result;
    for (auto value : values) {
        wire::require(value.isMap(), "Datum geometry map required");
        auto m = value.toMap();
        DatumGeometry d;
        d.id = domain::id(wire::text(m, "id"));
        d.kind = wire::text(m, "kind");
        wire::require(!seen.contains(d.id), "Duplicate datum display identity");
        seen.insert(d.id);
        auto vector = [&](QString key, bool position) {
            auto v = m.value(key);
            wire::require(v.isArray() && v.toArray().size() == 3, "Datum vector required");
            auto a = v.toArray();
            for (auto n : a)
                wire::require((n.isDouble() || n.isInteger()) && std::isfinite(n.toDouble()) &&
                                  std::abs(n.toDouble()) <= (position ? 1000. : 1.000001),
                              "Finite bounded datum vector required");
            return sketch::Vec3{a[0].toDouble(), a[1].toDouble(), a[2].toDouble()};
        };
        d.frame.origin = vector("origin", true);
        if (d.kind == "DatumPlane") {
            wire::keys(m, {"id", "kind", "origin", "u", "v"});
            d.frame.u = vector("u", false);
            d.frame.v = vector("v", false);
            d.frame.validate();
        } else {
            wire::require(d.kind == "DatumAxis", "Typed datum display kind required");
            wire::keys(m, {"id", "kind", "origin", "direction"});
            d.direction = vector("direction", false);
            auto p = d.direction;
            wire::require(std::abs(p[0] * p[0] + p[1] * p[1] + p[2] * p[2] - 1) < 1e-6,
                          "Unit datum direction required");
        }
        result.push_back(d);
    }
    return result;
}
} // namespace cad::model

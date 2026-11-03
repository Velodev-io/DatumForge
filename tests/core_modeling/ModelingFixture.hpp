#pragma once
#include "SupportFixture.hpp"
#include "features/ModelingIntent.hpp"
namespace modeling_fixture {
using namespace cad;
using sketch::fields;
inline QCborMap blind(double depth = .01, double draft = 0, bool outward = false) {
    return fields({{"mode", "Blind"},
                   {"value", depth},
                   {"draft", fields({{"angle", draft}, {"outward", outward}})}});
}
inline QCborMap extrude(QString sketch, QString operation = "NewBody",
                        QCborValue target = nullptr) {
    return fields({{"version", 1},
                   {"id", newId()},
                   {"kind", "Extrude"},
                   {"operation", operation},
                   {"target", target},
                   {"profile",
                    fields({{"sketch", sketch}, {"mode", "AllClosed"}, {"regions", QCborArray{}}})},
                   {"start", fields({{"mode", "SketchPlane"}})},
                   {"direction1", blind()},
                   {"direction2", QCborValue(nullptr)},
                   {"reverse", false},
                   {"thin", fields({{"mode", "Solid"}})}});
}
inline QCborMap body(QString stage, QString id = {}) {
    return fields({{"source", support_fixture::ref(stage, "Object").map()},
                   {"body", id.isEmpty() ? stage : id}});
}
inline model::SupportDefinition base(const std::vector<sketch::Sketch> &sketches) {
    model::SupportDefinition d;
    d.sketches = sketch::encode(sketches);
    for (auto &s : sketches)
        d.history.append(s.id);
    return d;
}
} // namespace modeling_fixture

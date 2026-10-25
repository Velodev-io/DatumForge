#include "LineEditing.hpp"
#include <QSet>
namespace cad::sketch {
QStringList lineEditModes() {
    return {"Trim to closest", "Trim away inside", "Trim away outside", "Dynamic extend",
            "Extend to apparent intersection"};
}
void validateEditHistory(const Sketch &s, QSet<QString> &ids) {
    domain::check(s.edits.size() <= 64, "ResourceLimit", "At most 64 sketch edit records");
    for (auto value : s.edits) {
        domain::check(value.isMap(), "InvalidInput", "Sketch edit record required");
        auto m = value.toMap();
        wire::keys(m, {"id", "mode", "source", "descendants", "removed_constraints"});
        auto id = wire::text(m, "id");
        domain::id(id);
        domain::check(!ids.contains(id), "InvalidInput", "Duplicate sketch edit identity");
        ids.insert(id);
        domain::id(wire::text(m, "source"));
        domain::check(lineEditModes().contains(wire::text(m, "mode")), "InvalidInput",
                      "Unsupported sketch edit mode");
        for (auto key : {"descendants", "removed_constraints"}) {
            auto v = m.value(QLatin1String(key));
            domain::check(v.isArray() &&
                              v.toArray().size() <= (QString(key) == "descendants" ? 2 : 512),
                          "ResourceLimit", "Bounded sketch edit mappings required");
            domain::check(QString(key) != "descendants" || !v.toArray().empty(), "InvalidInput",
                          "Line edit requires a surviving descendant");
            QSet<QString> seen;
            for (auto item : v.toArray()) {
                domain::check(item.isString(), "InvalidInput", "Mapped semantic ID required");
                domain::id(item.toString());
                domain::check(!seen.contains(item.toString()), "InvalidInput",
                              "Duplicate edit mapping");
                seen.insert(item.toString());
            }
        }
    }
}
} // namespace cad::sketch

#include "Model.hpp"
#include <numbers>
namespace cad::features {
using domain::check;
double number(const QCborMap &p, QString key, double low, double high) {
    auto n = p.value(key);
    check((n.isDouble() || n.isInteger()) && std::isfinite(n.toDouble()) && n.toDouble() >= low &&
              n.toDouble() <= high,
          "InvalidInput", "Invalid SI feature parameter: " + key);
    return n.toDouble();
}
sketch::Vec3 vector(const QCborMap &p, QString key, bool direction) {
    auto a = p.value(key).toArray();
    check(a.size() == 3, "InvalidInput", "Three coordinates required: " + key);
    sketch::Vec3 v;
    double norm = 0;
    for (int i = 0; i < 3; ++i) {
        check((a[i].isDouble() || a[i].isInteger()) && std::isfinite(a[i].toDouble()) &&
                  std::abs(a[i].toDouble()) <= (direction ? 1 : 1000),
              "InvalidInput", "Invalid vector: " + key);
        v[i] = a[i].toDouble();
        norm += v[i] * v[i];
    }
    check(!direction || std::abs(norm - 1) <= 1e-10, "InvalidInput",
          "Direction must be a unit vector: " + key);
    return v;
}
QCborArray array(sketch::Vec3 v) {
    return {v[0], v[1], v[2]};
}
QCborMap Feature::map() const {
    auto result = sketch::fields({{"id", id},
                                  {"name", name},
                                  {"kind", kind},
                                  {"version", suppressed || !expressions.empty() ? 2 : 1},
                                  {"parameters", parameters}});
    if (suppressed || !expressions.empty()) {
        result.insert(QStringLiteral("suppressed"), suppressed);
        QCborMap bindings;
        for (auto it = expressions.cbegin(); it != expressions.cend(); ++it)
            bindings.insert(it.key(), it.value());
        result.insert(QStringLiteral("expressions"), bindings);
    }
    return result;
}

Feature Feature::parse(const QCborMap &m) {
    auto version = wire::integer(m, "version");
    wire::keys(m, version == 2 ? QSet<QString>{"id", "name", "kind", "version", "parameters",
                                               "suppressed", "expressions"}
                               : QSet<QString>{"id", "name", "kind", "version", "parameters"});
    check((version == 1 || version == 2) && m.value("parameters").isMap(), "IncompatibleSchema",
          "Unsupported feature version");
    Feature f;
    f.id = domain::id(wire::text(m, "id"));
    f.name = domain::label(wire::text(m, "name"));
    f.kind = wire::text(m, "kind");
    f.parameters = m.value("parameters").toMap();
    if (version == 2) {
        check(m.value("suppressed").isBool() && m.value("expressions").isMap(), "InvalidInput",
              "Invalid feature state");
        f.suppressed = m.value("suppressed").toBool();
        auto e = m.value("expressions").toMap();
        check(e.size() <= 8, "ResourceLimit", "At most eight feature expression bindings");
        for (auto it = e.cbegin(); it != e.cend(); ++it) {
            auto key = it.key().toString();
            check(QStringList{"depth", "angle", "size", "diameter", "spacing", "count"}.contains(
                      key) &&
                      f.parameters.contains(key) && it.value().isString() &&
                      it.value().toString().toUtf8().size() <= 4096,
                  "InvalidInput", "Unsupported feature expression binding");
            f.expressions[key] = it.value().toString();
        }
    }
    auto &p = f.parameters;
    auto choice = [&](QString key, QStringList choices) {
        check(choices.contains(wire::text(p, key)), "UnsupportedOperation",
              "Unsupported feature option: " + key);
    };
    auto ref = [&](QString key, bool optional = false) {
        auto v = wire::text(p, key);
        if (!optional || !v.isEmpty())
            domain::id(v);
    };
    if (f.kind == "Imported") {
        wire::keys(p, {"source_sha256", "format", "units", "healing", "source_name", "report",
                       "operation"});
        check(QRegularExpression("^[0-9a-f]{64}$").match(wire::text(p, "source_sha256")).hasMatch(),
              "InvalidInput", "Invalid imported source identity");
        choice("format", {"STEP", "IGES"});
        choice("units", {"File", "mm", "cm", "m", "in"});
        choice("healing", {"None", "Fix"});
        choice("operation", {"NewBody"});
        domain::label(wire::text(p, "source_name"));
        check(p.value("report").isMap() && QCborValue(p.value("report")).toCbor().size() <= 8192,
              "InvalidInput", "Bounded import report required");
        check(f.expressions.empty(), "InvalidInput",
              "Imported sources have no editable feature parameters");
    } else if (f.kind == "Extrude" || f.kind == "Revolve") {
        bool revolve = f.kind == "Revolve";
        wire::keys(p, revolve ? QSet<QString>{"sketch", "operation", "target", "angle", "direction",
                                              "axis_origin", "axis_direction"}
                              : QSet<QString>{"sketch", "operation", "target", "depth", "extent"});
        ref("sketch");
        choice("operation", {"NewBody", "Add", "Cut"});
        ref("target", wire::text(p, "operation") == "NewBody");
        if (wire::text(p, "operation") == "NewBody")
            check(wire::text(p, "target").isEmpty(), "InvalidInput", "New body has no target");
        if (revolve) {
            number(p, "angle", 1e-8, 2 * std::numbers::pi);
            choice("direction", {"Forward", "Reverse"});
            vector(p, "axis_origin");
            vector(p, "axis_direction", true);
        } else {
            number(p, "depth", 1e-8, 1000);
            choice("extent", {"Forward", "Reverse", "Symmetric"});
        }
    } else if (f.kind == "Fillet" || f.kind == "Chamfer") {
        wire::keys(p, {"target", "source", "edges", "size"});
        ref("target");
        ref("source");
        number(p, "size", 1e-8, 1000);
        auto a = p.value("edges").toArray();
        check(p.value("edges").isArray() && !a.empty() && a.size() <= 32, "InvalidInput",
              "Select 1–32 current edges");
        QSet<QString> seen;
        for (auto token : a) {
            auto s = token.toString();
            check(token.isString() &&
                      QRegularExpression("^edge/[0-9a-f]{64}$").match(s).hasMatch() &&
                      !seen.contains(s),
                  "InvalidInput", "Invalid or repeated edge token");
            seen.insert(s);
        }
    } else if (f.kind == "Hole") {
        wire::keys(p, {"target", "origin", "direction", "diameter", "depth", "through"});
        ref("target");
        vector(p, "origin");
        vector(p, "direction", true);
        number(p, "diameter", 2e-8, 1000);
        number(p, "depth", 1e-8, 1000);
        check(p.value("through").isBool(), "InvalidInput", "Choose blind or through-all");
    } else if (f.kind == "LinearPattern" || f.kind == "CircularPattern" || f.kind == "Mirror") {
        auto keys = QSet<QString>{"source", "target", "operation"};
        if (f.kind == "LinearPattern")
            keys.unite({"direction", "spacing", "count"});
        else if (f.kind == "CircularPattern")
            keys.unite({"axis_origin", "axis_direction", "angle", "count", "distribution"});
        else
            keys.unite({"origin", "normal"});
        wire::keys(p, keys);
        ref("source");
        choice("operation", {"NewBody", "Add", "Cut"});
        ref("target", wire::text(p, "operation") == "NewBody");
        if (wire::text(p, "operation") == "NewBody")
            check(wire::text(p, "target").isEmpty(), "InvalidInput", "New bodies have no target");
        if (f.kind != "Mirror") {
            auto count = wire::integer(p, "count", 32);
            check(count >= 1, "InvalidInput", "Pattern count must be 1–32 including seed");
        }
        if (f.kind == "LinearPattern") {
            vector(p, "direction", true);
            number(p, "spacing", 1e-8, 1000);
        } else if (f.kind == "CircularPattern") {
            vector(p, "axis_origin");
            vector(p, "axis_direction", true);
            number(p, "angle", 1e-8, 2 * std::numbers::pi);
            choice("distribution", {"Spacing", "Full"});
        } else {
            vector(p, "origin");
            vector(p, "normal", true);
        }
    } else
        throw domain::Error("UnsupportedOperation", "Unsupported exact feature: " + f.kind);
    return f;
}
QCborArray encode(const std::vector<Feature> &features) {
    QCborArray a;
    for (auto &f : features)
        a.append(f.map());
    return a;
}
std::vector<Feature> parse(const QCborArray &a) {
    check(a.size() <= 100 && QCborValue(a).toCbor().size() <= 131072, "ResourceLimit",
          "Feature admission is 100 definitions / 128 KiB per document");
    std::vector<Feature> out;
    QSet<QString> ids;
    for (auto item : a) {
        check(item.isMap(), "InvalidInput", "Feature definition required");
        auto f = Feature::parse(item.toMap());
        check(!ids.contains(f.id), "InvalidInput", "Duplicate feature identity");
        ids.insert(f.id);
        out.push_back(f);
    }
    return out;
}
} // namespace cad::features

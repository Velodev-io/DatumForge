#include "Catalogs.hpp"
#include "protocol/Envelope.hpp"
#include <cmath>
#include <numbers>

namespace cad::features {

using domain::check;

namespace {
void checkKeys(const QCborMap &map, const QSet<QString> &required,
               const QSet<QString> &optional = {}) {
    for (const auto &req : required) {
        check(map.contains(req), "InvalidInput", "missing required field: " + req);
    }
    for (auto it = map.begin(); it != map.end(); ++it) {
        check(it.key().isString(), "InvalidInput", "field key must be string");
        QString k = it.key().toString();
        check(required.contains(k) || optional.contains(k), "InvalidInput", "unknown field: " + k);
    }
}

double optNumber(const QCborMap &m, const QString &k, double minVal, double maxVal,
                 double defaultVal = 0.0) {
    if (!m.contains(k))
        return defaultVal;
    auto val = m.value(k);
    check(val.isDouble() || val.isInteger(), "InvalidInput", "Numeric value required for " + k);
    double d = val.toDouble();
    check(std::isfinite(d) && d >= minVal && d <= maxVal, "InvalidInput",
          "Value for " + k + " out of valid range");
    return d;
}

std::optional<double> optDouble(const QCborMap &m, const QString &k, double minVal, double maxVal) {
    if (!m.contains(k) || m.value(k).isNull())
        return std::nullopt;
    auto val = m.value(k);
    check(val.isDouble() || val.isInteger(), "InvalidInput", "Numeric value required for " + k);
    double d = val.toDouble();
    check(std::isfinite(d) && d >= minVal && d <= maxVal, "InvalidInput",
          "Value for " + k + " out of valid range");
    return d;
}

std::array<double, 3> parseVec3(const QCborValue &val, const QString &fieldName) {
    check(val.isArray(), "InvalidInput", fieldName + " must be a 3-element array");
    auto arr = val.toArray();
    check(arr.size() == 3, "InvalidInput", fieldName + " must have 3 elements");
    std::array<double, 3> res;
    for (int i = 0; i < 3; ++i) {
        check(arr[i].isDouble() || arr[i].isInteger(), "InvalidInput",
              fieldName + " elements must be numeric");
        res[i] = arr[i].toDouble();
        check(std::isfinite(res[i]), "InvalidInput", fieldName + " elements must be finite");
    }
    return res;
}

std::array<float, 4> parseVec4f(const QCborValue &val, const QString &fieldName) {
    check(val.isArray(), "InvalidInput", fieldName + " must be a 4-element array");
    auto arr = val.toArray();
    check(arr.size() == 4, "InvalidInput", fieldName + " must have 4 elements");
    std::array<float, 4> res;
    for (int i = 0; i < 4; ++i) {
        check(arr[i].isDouble() || arr[i].isInteger(), "InvalidInput",
              fieldName + " elements must be numeric");
        double d = arr[i].toDouble();
        check(std::isfinite(d) && d >= 0.0 && d <= 1.0, "InvalidInput",
              fieldName + " elements must be in [0, 1]");
        res[i] = static_cast<float>(d);
    }
    return res;
}
} // namespace

// ============================================================================
// 1. Hole Catalog Resolution Implementation
// ============================================================================

HoleCatalogQuery HoleCatalogQuery::parse(const QCborMap &map) {
    HoleCatalogQuery q;
    checkKeys(map, {"version", "dataset", "standard", "type", "size", "fit"}, {"custom_overrides"});
    q.version = wire::integer(map, "version");
    check(q.version == 1, "InvalidInput", "Hole catalog query version must be 1");
    q.dataset = wire::text(map, "dataset");
    domain::label(q.dataset);
    q.standard = wire::text(map, "standard");
    q.type = wire::text(map, "type");
    q.size = wire::text(map, "size");
    q.fit = wire::text(map, "fit");
    if (map.contains(QStringLiteral("custom_overrides"))) {
        check(map.value("custom_overrides").isMap(), "InvalidInput",
              "custom_overrides must be a map");
        q.custom_overrides = map.value("custom_overrides").toMap();
    }
    return q;
}

QCborMap HoleCatalogQuery::map() const {
    QCborMap m;
    m.insert(QStringLiteral("version"), version);
    m.insert(QStringLiteral("dataset"), dataset);
    m.insert(QStringLiteral("standard"), standard);
    m.insert(QStringLiteral("type"), type);
    m.insert(QStringLiteral("size"), size);
    m.insert(QStringLiteral("fit"), fit);
    if (!custom_overrides.isEmpty())
        m.insert(QStringLiteral("custom_overrides"), custom_overrides);
    return m;
}

QCborMap HoleCatalogResolved::map() const {
    QCborMap m;
    m.insert(QStringLiteral("valid"), valid);
    m.insert(QStringLiteral("dataset"), dataset);
    m.insert(QStringLiteral("standard"), standard);
    m.insert(QStringLiteral("type"), type);
    m.insert(QStringLiteral("size"), size);
    m.insert(QStringLiteral("fit"), fit);
    m.insert(QStringLiteral("family"), family);
    m.insert(QStringLiteral("diameter"), diameter);
    if (counterbore_diameter)
        m.insert(QStringLiteral("counterbore_diameter"), *counterbore_diameter);
    if (counterbore_depth)
        m.insert(QStringLiteral("counterbore_depth"), *counterbore_depth);
    if (countersink_diameter)
        m.insert(QStringLiteral("countersink_diameter"), *countersink_diameter);
    if (countersink_angle)
        m.insert(QStringLiteral("countersink_angle"), *countersink_angle);
    if (drill_angle)
        m.insert(QStringLiteral("drill_angle"), *drill_angle);
    if (tap_drill_diameter)
        m.insert(QStringLiteral("tap_drill_diameter"), *tap_drill_diameter);
    if (pitch)
        m.insert(QStringLiteral("pitch"), *pitch);
    m.insert(QStringLiteral("cosmetic_thread"), cosmetic_thread);
    if (!thread_designation.isEmpty())
        m.insert(QStringLiteral("thread_designation"), thread_designation);
    if (!diagnostic.isEmpty())
        m.insert(QStringLiteral("diagnostic"), diagnostic);
    return m;
}

HoleCatalogResolved HoleCatalogResolved::parse(const QCborMap &map) {
    HoleCatalogResolved r;
    checkKeys(map,
              {"valid", "dataset", "standard", "type", "size", "fit", "family", "diameter",
               "cosmetic_thread"},
              {"counterbore_diameter", "counterbore_depth", "countersink_diameter",
               "countersink_angle", "drill_angle", "tap_drill_diameter", "pitch",
               "thread_designation", "diagnostic"});
    r.valid = map.value("valid").toBool();
    r.dataset = wire::text(map, "dataset");
    r.standard = wire::text(map, "standard");
    r.type = wire::text(map, "type");
    r.size = wire::text(map, "size");
    r.fit = wire::text(map, "fit");
    r.family = wire::text(map, "family");
    r.diameter = optNumber(map, "diameter", 1e-8, 1000.0);
    r.counterbore_diameter = optDouble(map, "counterbore_diameter", 1e-8, 1000.0);
    r.counterbore_depth = optDouble(map, "counterbore_depth", 1e-8, 1000.0);
    r.countersink_diameter = optDouble(map, "countersink_diameter", 1e-8, 1000.0);
    r.countersink_angle = optDouble(map, "countersink_angle", 1e-4, std::numbers::pi - 1e-4);
    r.drill_angle = optDouble(map, "drill_angle", 1e-4, std::numbers::pi - 1e-4);
    r.tap_drill_diameter = optDouble(map, "tap_drill_diameter", 1e-8, 1000.0);
    r.pitch = optDouble(map, "pitch", 1e-8, 1000.0);
    r.cosmetic_thread = map.value("cosmetic_thread").toBool();
    if (map.contains(QStringLiteral("thread_designation")))
        r.thread_designation = map.value("thread_designation").toString();
    if (map.contains(QStringLiteral("diagnostic")))
        r.diagnostic = map.value("diagnostic").toString();
    return r;
}

QStringList availableHoleDatasets() {
    return {"ansi_inch", "ansi_metric", "iso", "din", "bsi", "jis", "gb", "gost", "custom"};
}

QStringList availableHoleStandards(const QString &dataset) {
    QString ds = dataset.toLower();
    if (ds == "ansi_metric")
        return {"B18.3.4M", "B18.3.1M", "B18.6.7M", "MetricTapped", "ClearanceHole"};
    if (ds == "ansi_inch")
        return {"B18.3", "B18.6.3", "B18.2.1", "InchTapped", "ASME_B1.20.1"};
    if (ds == "iso")
        return {"ISO 4762", "ISO 10642", "ISO 273", "ISO 965-1"};
    if (ds == "din")
        return {"DIN 912", "DIN 7991", "DIN 74", "DIN 13-1"};
    if (ds == "bsi")
        return {"BS 4168", "BS 4183", "BS 3643", "ClearanceHole"};
    if (ds == "jis")
        return {"JIS B 1176", "JIS B 1194", "JIS B 1001", "JIS B 0205"};
    if (ds == "gb")
        return {"GB/T 70.1", "GB/T 70.3", "GB/T 5277", "GB/T 193"};
    if (ds == "gost")
        return {"GOST 11738", "GOST 17475", "GOST 11284", "GOST 8724"};
    return {"Custom"};
}

QStringList availableHoleTypes(const QString &dataset, const QString &standard) {
    QString ds = dataset.toLower();
    if (standard == "B18.3.4M")
        return {"SocketButtonHeadCapScrew"};
    if (standard == "B18.3.1M" || standard == "B18.3" || standard == "ISO 4762" ||
        standard == "DIN 912" || standard == "BS 4168" || standard == "JIS B 1176" ||
        standard == "GB/T 70.1" || standard == "GOST 11738")
        return {"SocketHeadCapScrew"};
    if (standard == "B18.6.7M" || standard == "B18.6.3" || standard == "ISO 10642" ||
        standard == "DIN 7991" || standard == "BS 4183" || standard == "JIS B 1194" ||
        standard == "GB/T 70.3" || standard == "GOST 17475")
        return {"CountersunkHeadScrew", "CountersunkFlatHead"};
    if (standard == "B18.2.1")
        return {"HexBolt", "ClearanceHole"};
    if (standard.contains("Tapped", Qt::CaseInsensitive) || standard == "ISO 965-1" ||
        standard == "DIN 13-1" || standard == "BS 3643" || standard == "JIS B 0205" ||
        standard == "GB/T 193" || standard == "GOST 8724")
        return {"TappedHole"};
    if (standard == "ASME_B1.20.1")
        return {"PipeTap"};
    return {"ClearanceHole", "Simple"};
}

QStringList availableHoleSizes(const QString &dataset, const QString &standard, const QString &type) {
    QString ds = dataset.toLower();
    if (ds == "ansi_inch" || standard == "B18.3" || standard == "B18.6.3" || standard == "B18.2.1" || standard == "InchTapped")
        return {"1/4-20", "#4-40", "#6-32", "#8-32", "#10-24", "5/16-18", "3/8-16", "1/2-13"};
    if (standard == "ASME_B1.20.1" || type == "PipeTap")
        return {"1/4-18 NPT", "1/8-27", "1/4-18", "3/8-18", "1/2-14", "3/4-14"};
    return {"M8", "M6", "M5", "M4", "M3", "M10", "M12", "M16", "M20"};
}

QStringList availableHoleFits(const QString &dataset, const QString &standard, const QString &type, const QString &size) {
    return {"Close", "Normal", "Loose"};
}

HoleCatalogResolved resolveHoleCatalog(const HoleCatalogQuery &query) {
    QString ds = query.dataset.toLower();
    check(availableHoleDatasets().contains(ds), "UnsupportedOperation",
          "Unsupported hole catalog dataset: " + query.dataset);

    HoleCatalogResolved res;
    res.dataset = query.dataset;
    res.standard = query.standard;
    res.type = query.type;
    res.size = query.size;
    res.fit = query.fit;
    res.valid = true;

    // Default drill angle: 118 degrees in radians
    constexpr double defaultDrillAngle = 118.0 * std::numbers::pi / 180.0;
    res.drill_angle = defaultDrillAngle;

    bool recognized = false;

    if (ds == "ansi_metric") {
        if (query.standard == "B18.3.4M" && query.type == "SocketButtonHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter =
                    (query.fit == "Close" ? 0.0084 : (query.fit == "Loose" ? 0.0100 : 0.0090));
                res.counterbore_diameter = 0.0150;
                res.counterbore_depth = 0.0088;
            } else if (query.size == "M6") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter =
                    (query.fit == "Close" ? 0.0064 : (query.fit == "Loose" ? 0.0070 : 0.0066));
                res.counterbore_diameter = 0.0118;
                res.counterbore_depth = 0.0065;
            } else if (query.size == "M5") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter =
                    (query.fit == "Close" ? 0.0053 : (query.fit == "Loose" ? 0.0058 : 0.0055));
                res.counterbore_diameter = 0.0100;
                res.counterbore_depth = 0.0055;
            }
        } else if (query.standard == "B18.3.1M" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0090;
                res.counterbore_diameter = 0.0145;
                res.counterbore_depth = 0.0085;
            } else if (query.size == "M6") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0066;
                res.counterbore_diameter = 0.0110;
                res.counterbore_depth = 0.0065;
            }
        } else if (query.standard == "B18.6.7M" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0165;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "MetricTapped" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068; // 6.8 mm tap drill
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125; // 1.25 mm coarse pitch
                res.cosmetic_thread = true;
                res.thread_designation = "M8x1.25";
            } else if (query.size == "M6") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0050; // 5.0 mm tap drill
                res.tap_drill_diameter = 0.0050;
                res.pitch = 0.0010;
                res.cosmetic_thread = true;
                res.thread_designation = "M6x1.0";
            }
        } else if (query.type == "ClearanceHole" || query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter =
                    (query.fit == "Close" ? 0.0084 : (query.fit == "Loose" ? 0.0100 : 0.0090));
            }
        }
    } else if (ds == "ansi_inch") {
        constexpr double in2m = 0.0254;
        if (query.standard == "B18.3" && query.type == "SocketHeadCapScrew") {
            if (query.size == "1/4-20" || query.size == "1/4") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter =
                    (query.fit == "Close" ? 0.2570 : (query.fit == "Loose" ? 0.2810 : 0.2660)) *
                    in2m;
                res.counterbore_diameter = 0.4375 * in2m;
                res.counterbore_depth = 0.2500 * in2m;
            }
        } else if (query.standard == "B18.6.3" && query.type == "CountersunkFlatHead") {
            if (query.size == "1/4-20" || query.size == "1/4") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.2660 * in2m;
                res.countersink_diameter = 0.5310 * in2m;
                res.countersink_angle = 82.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "B18.2.1" &&
                   (query.type == "HexBolt" || query.type == "ClearanceHole")) {
            if (query.size == "1/4-20" || query.size == "1/4") {
                recognized = true;
                res.family = "Simple";
                res.diameter = 0.2810 * in2m;
            }
        } else if (query.standard == "InchTapped" || query.type == "TappedHole") {
            if (query.size == "1/4-20") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.2010 * in2m; // #7 drill
                res.tap_drill_diameter = 0.2010 * in2m;
                res.pitch = (1.0 / 20.0) * in2m;
                res.cosmetic_thread = true;
                res.thread_designation = "1/4-20 UNC";
            }
        } else if (query.standard == "ASME_B1.20.1" || query.type == "PipeTap") {
            if (query.size == "1/4-18 NPT" || query.size == "1/4 NPT") {
                recognized = true;
                res.family = "PipeTap";
                res.diameter = 0.4380 * in2m;
                res.tap_drill_diameter = 0.4380 * in2m;
                res.cosmetic_thread = true;
                res.thread_designation = "1/4-18 NPT";
            }
        }
    } else if (ds == "iso") {
        if (query.standard == "ISO 4762" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter =
                    (query.fit == "Fine" ? 0.0084 : (query.fit == "Coarse" ? 0.0100 : 0.0090));
                res.counterbore_diameter = 0.0150;
                res.counterbore_depth = 0.0085;
            }
        } else if (query.standard == "ISO 10642" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0160;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "ISO 273" || query.type == "ClearanceHole" ||
                   query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter =
                    (query.fit == "Fine" ? 0.0084 : (query.fit == "Coarse" ? 0.0100 : 0.0090));
            }
        } else if (query.standard == "ISO 965-1" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068;
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125;
                res.cosmetic_thread = true;
                res.thread_designation = "M8";
            }
        }
    } else if (ds == "din") {
        if (query.standard == "DIN 912" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0090;
                res.counterbore_diameter = 0.0150;
                res.counterbore_depth = 0.0085;
            }
        } else if (query.standard == "DIN 7991" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0160;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "DIN 74" || query.type == "ClearanceHole" ||
                   query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter = 0.0090;
            }
        } else if (query.standard == "DIN 13-1" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068;
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125;
                res.cosmetic_thread = true;
                res.thread_designation = "M8";
            }
        }
    } else if (ds == "bsi") {
        if (query.standard == "BS 4168" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0090;
                res.counterbore_diameter = 0.0150;
                res.counterbore_depth = 0.0085;
            }
        } else if (query.standard == "BS 4183" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0160;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "BS 3643" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068;
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125;
                res.cosmetic_thread = true;
                res.thread_designation = "M8";
            }
        } else if (query.type == "ClearanceHole" || query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter = 0.0090;
            }
        }
    } else if (ds == "jis") {
        if (query.standard == "JIS B 1176" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0090;
                res.counterbore_diameter = 0.0145;
                res.counterbore_depth = 0.0085;
            }
        } else if (query.standard == "JIS B 1194" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0160;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "JIS B 1001" || query.type == "ClearanceHole" ||
                   query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter = 0.0090;
            }
        } else if (query.standard == "JIS B 0205" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068;
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125;
                res.cosmetic_thread = true;
                res.thread_designation = "M8";
            }
        }
    } else if (ds == "gb") {
        if (query.standard == "GB/T 70.1" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0090;
                res.counterbore_diameter = 0.0150;
                res.counterbore_depth = 0.0085;
            }
        } else if (query.standard == "GB/T 70.3" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0160;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "GB/T 5277" || query.type == "ClearanceHole" ||
                   query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter = 0.0090;
            }
        } else if (query.standard == "GB/T 193" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068;
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125;
                res.cosmetic_thread = true;
                res.thread_designation = "M8";
            }
        }
    } else if (ds == "gost") {
        if (query.standard == "GOST 11738" && query.type == "SocketHeadCapScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Counterbore";
                res.diameter = 0.0090;
                res.counterbore_diameter = 0.0150;
                res.counterbore_depth = 0.0085;
            }
        } else if (query.standard == "GOST 17475" && query.type == "CountersunkHeadScrew") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Countersink";
                res.diameter = 0.0085;
                res.countersink_diameter = 0.0160;
                res.countersink_angle = 90.0 * std::numbers::pi / 180.0;
            }
        } else if (query.standard == "GOST 11284" || query.type == "ClearanceHole" ||
                   query.type == "Simple") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Simple";
                res.diameter = 0.0090;
            }
        } else if (query.standard == "GOST 8724" || query.type == "TappedHole") {
            if (query.size == "M8") {
                recognized = true;
                res.family = "Tapped";
                res.diameter = 0.0068;
                res.tap_drill_diameter = 0.0068;
                res.pitch = 0.00125;
                res.cosmetic_thread = true;
                res.thread_designation = "M8";
            }
        }
    } else if (ds == "custom") {
        recognized = true;
        res.family = query.type.isEmpty() ? "Simple" : query.type;
        res.diameter = 0.0100;
    }

    check(recognized, "UnsupportedOperation",
          QString("Unsupported hole specification: dataset=%1 standard=%2 type=%3 size=%4 fit=%5")
              .arg(query.dataset, query.standard, query.type, query.size, query.fit));

    if (!query.custom_overrides.isEmpty()) {
        const auto &ov = query.custom_overrides;
        if (ov.contains(QStringLiteral("diameter"))) {
            res.diameter = optNumber(ov, "diameter", 1e-8, 1000.0);
        }
        if (ov.contains(QStringLiteral("counterbore_diameter"))) {
            res.counterbore_diameter = optDouble(ov, "counterbore_diameter", 1e-8, 1000.0);
        }
        if (ov.contains(QStringLiteral("counterbore_depth"))) {
            res.counterbore_depth = optDouble(ov, "counterbore_depth", 1e-8, 1000.0);
        }
        if (ov.contains(QStringLiteral("countersink_diameter"))) {
            res.countersink_diameter = optDouble(ov, "countersink_diameter", 1e-8, 1000.0);
        }
        if (ov.contains(QStringLiteral("countersink_angle"))) {
            res.countersink_angle =
                optDouble(ov, "countersink_angle", 1e-4, std::numbers::pi - 1e-4);
        }
        if (ov.contains(QStringLiteral("drill_angle"))) {
            res.drill_angle = optDouble(ov, "drill_angle", 1e-4, std::numbers::pi - 1e-4);
        }
        if (ov.contains(QStringLiteral("tap_drill_diameter"))) {
            res.tap_drill_diameter = optDouble(ov, "tap_drill_diameter", 1e-8, 1000.0);
        }
        if (ov.contains(QStringLiteral("pitch"))) {
            res.pitch = optDouble(ov, "pitch", 1e-8, 1000.0);
        }
    }

    return res;
}

// ============================================================================
// 2. Materials & Physical Properties Implementation
// ============================================================================

QCborMap MaterialProperties::map() const {
    QCborMap m;
    m.insert(QStringLiteral("density_kg_m3"), density_kg_m3);
    if (elastic_modulus_pa)
        m.insert(QStringLiteral("elastic_modulus_pa"), *elastic_modulus_pa);
    if (poissons_ratio)
        m.insert(QStringLiteral("poissons_ratio"), *poissons_ratio);
    if (shear_modulus_pa)
        m.insert(QStringLiteral("shear_modulus_pa"), *shear_modulus_pa);
    if (thermal_conductivity_w_mk)
        m.insert(QStringLiteral("thermal_conductivity_w_mk"), *thermal_conductivity_w_mk);
    if (specific_heat_j_kgk)
        m.insert(QStringLiteral("specific_heat_j_kgk"), *specific_heat_j_kgk);
    if (yield_strength_pa)
        m.insert(QStringLiteral("yield_strength_pa"), *yield_strength_pa);
    if (tensile_strength_pa)
        m.insert(QStringLiteral("tensile_strength_pa"), *tensile_strength_pa);
    return m;
}

MaterialProperties MaterialProperties::parse(const QCborMap &map) {
    MaterialProperties p;
    checkKeys(map, {"density_kg_m3"},
              {"elastic_modulus_pa", "poissons_ratio", "shear_modulus_pa",
               "thermal_conductivity_w_mk", "specific_heat_j_kgk", "yield_strength_pa",
               "tensile_strength_pa"});
    p.density_kg_m3 = optNumber(map, "density_kg_m3", 1e-8, 1e6);
    p.elastic_modulus_pa = optDouble(map, "elastic_modulus_pa", 1e-8, 1e15);
    p.poissons_ratio = optDouble(map, "poissons_ratio", -0.9999, 0.4999);
    p.shear_modulus_pa = optDouble(map, "shear_modulus_pa", 1e-8, 1e15);
    p.thermal_conductivity_w_mk = optDouble(map, "thermal_conductivity_w_mk", 1e-8, 1e5);
    p.specific_heat_j_kgk = optDouble(map, "specific_heat_j_kgk", 1e-8, 1e6);
    p.yield_strength_pa = optDouble(map, "yield_strength_pa", 1e-8, 1e12);
    p.tensile_strength_pa = optDouble(map, "tensile_strength_pa", 1e-8, 1e12);
    return p;
}

QCborMap Material::map() const {
    QCborMap m;
    m.insert(QStringLiteral("version"), version);
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("name"), name);
    m.insert(QStringLiteral("family"), family);
    m.insert(QStringLiteral("properties"), properties.map());
    if (!appearance.isEmpty())
        m.insert(QStringLiteral("appearance"), appearance);
    return m;
}

Material Material::parse(const QCborMap &map) {
    Material m;
    checkKeys(map, {"version", "id", "name", "family", "properties"}, {"appearance"});
    m.version = wire::integer(map, "version");
    check(m.version == 1, "InvalidInput", "Material version must be 1");
    m.id = wire::text(map, "id");
    domain::label(m.id);
    m.name = wire::text(map, "name");
    check(!m.name.isEmpty(), "InvalidInput", "Material name cannot be empty");
    m.family = wire::text(map, "family");
    check(QStringList{"Steel", "Aluminum", "Copper", "Titanium", "Iron", "Plastics", "Custom"}
              .contains(m.family),
          "InvalidInput", "Unsupported material family: " + m.family);
    check(map.value("properties").isMap(), "InvalidInput", "Material properties required");
    m.properties = MaterialProperties::parse(map.value("properties").toMap());
    if (map.contains(QStringLiteral("appearance"))) {
        check(map.value("appearance").isMap(), "InvalidInput", "Appearance must be a map");
        m.appearance = map.value("appearance").toMap();
    }
    return m;
}

std::vector<Material> standardMaterials() {
    std::vector<Material> list;

    auto makeMat = [](QString id, QString name, QString family, double density, double E, double nu,
                      double yieldStr, double tensileStr, double k) {
        Material m;
        m.id = id;
        m.name = name;
        m.family = family;
        m.properties.density_kg_m3 = density;
        m.properties.elastic_modulus_pa = E;
        m.properties.poissons_ratio = nu;
        m.properties.shear_modulus_pa = E / (2.0 * (1.0 + nu));
        m.properties.yield_strength_pa = yieldStr;
        m.properties.tensile_strength_pa = tensileStr;
        m.properties.thermal_conductivity_w_mk = k;
        return m;
    };

    // Steel
    list.push_back(makeMat("steel_1020", "Plain Carbon Steel 1020", "Steel", 7870.0, 205e9, 0.29,
                           350e6, 420e6, 51.9));
    list.push_back(makeMat("steel_4140", "Alloy Steel 4140", "Steel", 7850.0, 210e9, 0.28, 415e6,
                           655e6, 42.6));
    list.push_back(makeMat("steel_ss304", "Stainless Steel 304", "Steel", 8000.0, 193e9, 0.29,
                           215e6, 505e6, 16.2));
    list.push_back(makeMat("steel_ss316", "Stainless Steel 316", "Steel", 8000.0, 193e9, 0.30,
                           290e6, 580e6, 16.3));

    // Aluminum
    list.push_back(makeMat("al_6061_t6", "Aluminum 6061-T6", "Aluminum", 2700.0, 68.9e9, 0.33,
                           276e6, 310e6, 167.0));
    list.push_back(makeMat("al_7075_t6", "Aluminum 7075-T6", "Aluminum", 2810.0, 71.7e9, 0.33,
                           503e6, 572e6, 130.0));

    // Copper
    list.push_back(makeMat("brass_c36000", "Brass C36000 Free-Cutting", "Copper", 8500.0, 97.0e9,
                           0.31, 310e6, 400e6, 115.0));
    list.push_back(makeMat("bronze_c51000", "Phosphor Bronze C51000", "Copper", 8860.0, 110.0e9,
                           0.34, 550e6, 620e6, 69.0));

    // Titanium
    list.push_back(makeMat("ti_6al_4v", "Titanium Ti-6Al-4V (Grade 5)", "Titanium", 4430.0, 113.8e9,
                           0.34, 880e6, 950e6, 6.7));

    // Iron
    list.push_back(makeMat("cast_iron_gray", "Gray Cast Iron Class 30", "Iron", 7200.0, 100.0e9,
                           0.26, 214e6, 310e6, 46.0));

    // Plastics
    list.push_back(
        makeMat("plastic_abs", "ABS Molded", "Plastics", 1040.0, 2.3e9, 0.35, 40e6, 45e6, 0.17));
    list.push_back(
        makeMat("plastic_nylon66", "Nylon 6/6", "Plastics", 1140.0, 2.8e9, 0.40, 75e6, 85e6, 0.25));
    list.push_back(makeMat("plastic_pc", "Polycarbonate (PC)", "Plastics", 1200.0, 2.4e9, 0.37,
                           62e6, 70e6, 0.20));
    list.push_back(makeMat("plastic_pom", "POM (Acetal / Delrin)", "Plastics", 1420.0, 3.1e9, 0.35,
                           69e6, 72e6, 0.31));
    list.push_back(makeMat("plastic_ptfe", "PTFE (Teflon)", "Plastics", 2200.0, 0.5e9, 0.46, 23e6,
                           25e6, 0.25));

    return list;
}

std::optional<Material> findMaterial(const QString &id) {
    for (const auto &m : standardMaterials()) {
        if (m.id == id)
            return m;
    }
    return std::nullopt;
}

QStringList availableMaterialFamilies() {
    return {"Steel", "Aluminum", "Copper", "Titanium", "Iron", "Plastics", "Custom"};
}

// ============================================================================
// 3. Exact Mass Properties Evaluation Implementation
// ============================================================================

QCborMap MassPropertiesRequest::map() const {
    QCborMap m;
    m.insert(QStringLiteral("revision"), revision);
    m.insert(QStringLiteral("scope_type"), scopeType);
    if (!targetId.isEmpty())
        m.insert(QStringLiteral("target_id"), targetId);
    m.insert(QStringLiteral("frame"), frame);
    if (overrideDensity)
        m.insert(QStringLiteral("override_density"), *overrideDensity);
    return m;
}

MassPropertiesRequest MassPropertiesRequest::parse(const QCborMap &map) {
    MassPropertiesRequest r;
    checkKeys(map, {"revision", "scope_type", "frame"}, {"target_id", "override_density"});
    r.revision = wire::integer(map, "revision");
    check(r.revision >= 0, "InvalidInput", "Revision must be non-negative");
    r.scopeType = wire::text(map, "scope_type");
    check(QStringList{"Part", "Body", "AssemblyOccurrence"}.contains(r.scopeType), "InvalidInput",
          "Invalid mass properties scope: " + r.scopeType);
    if (map.contains(QStringLiteral("target_id")))
        r.targetId = wire::text(map, "target_id");
    if (r.scopeType == "Body" || r.scopeType == "AssemblyOccurrence") {
        check(!r.targetId.isEmpty(), "InvalidInput",
              "Target ID required for body or occurrence scope");
    }
    r.frame = wire::text(map, "frame");
    check(QStringList{"Part", "CenterOfMass"}.contains(r.frame), "InvalidInput",
          "Invalid evaluation frame: " + r.frame);
    r.overrideDensity = optDouble(map, "override_density", 1e-8, 1e6);
    return r;
}

QCborMap MassPropertiesResult::map() const {
    QCborMap m;
    m.insert(QStringLiteral("revision"), revision);
    m.insert(QStringLiteral("valid"), valid);
    if (!reason.isEmpty())
        m.insert(QStringLiteral("reason"), reason);
    m.insert(QStringLiteral("solids_count"), solids_count);
    m.insert(QStringLiteral("volume_m3"), volume_m3);
    m.insert(QStringLiteral("surface_area_m2"), surface_area_m2);
    m.insert(QStringLiteral("density_missing"), density_missing);
    if (density_kg_m3)
        m.insert(QStringLiteral("density_kg_m3"), *density_kg_m3);
    if (mass_kg)
        m.insert(QStringLiteral("mass_kg"), *mass_kg);

    QCborArray com;
    for (double v : center_of_mass_m)
        com.append(v);
    m.insert(QStringLiteral("center_of_mass_m"), com);

    QCborArray inertia;
    for (double v : inertia_tensor_kg_m2)
        inertia.append(v);
    m.insert(QStringLiteral("inertia_tensor_kg_m2"), inertia);

    QCborArray principal;
    for (double v : principal_moments_kg_m2)
        principal.append(v);
    m.insert(QStringLiteral("principal_moments_kg_m2"), principal);

    QCborArray axes;
    for (const auto &row : principal_axes) {
        QCborArray r;
        for (double v : row)
            r.append(v);
        axes.append(r);
    }
    m.insert(QStringLiteral("principal_axes"), axes);

    return m;
}

MassPropertiesResult MassPropertiesResult::parse(const QCborMap &map) {
    MassPropertiesResult r;
    checkKeys(map,
              {"revision", "valid", "solids_count", "volume_m3", "surface_area_m2",
               "density_missing", "center_of_mass_m", "inertia_tensor_kg_m2",
               "principal_moments_kg_m2", "principal_axes"},
              {"reason", "density_kg_m3", "mass_kg"});
    r.revision = wire::integer(map, "revision");
    r.valid = map.value("valid").toBool();
    if (map.contains(QStringLiteral("reason")))
        r.reason = map.value("reason").toString();
    r.solids_count = wire::integer(map, "solids_count");
    r.volume_m3 = optNumber(map, "volume_m3", 0.0, 1e12);
    r.surface_area_m2 = optNumber(map, "surface_area_m2", 0.0, 1e12);
    r.density_missing = map.value("density_missing").toBool();
    r.density_kg_m3 = optDouble(map, "density_kg_m3", 1e-8, 1e6);
    r.mass_kg = optDouble(map, "mass_kg", 0.0, 1e12);

    r.center_of_mass_m = parseVec3(map.value("center_of_mass_m"), "center_of_mass_m");

    auto inArr = map.value("inertia_tensor_kg_m2").toArray();
    check(inArr.size() == 6, "InvalidInput", "inertia_tensor_kg_m2 must have 6 components");
    for (int i = 0; i < 6; ++i)
        r.inertia_tensor_kg_m2[i] = inArr[i].toDouble();

    auto pmArr = map.value("principal_moments_kg_m2").toArray();
    check(pmArr.size() == 3, "InvalidInput", "principal_moments_kg_m2 must have 3 components");
    for (int i = 0; i < 3; ++i)
        r.principal_moments_kg_m2[i] = pmArr[i].toDouble();

    auto axArr = map.value("principal_axes").toArray();
    check(axArr.size() == 3, "InvalidInput", "principal_axes must have 3 rows");
    for (int i = 0; i < 3; ++i) {
        auto row = axArr[i].toArray();
        check(row.size() == 3, "InvalidInput", "principal_axes rows must have 3 elements");
        for (int j = 0; j < 3; ++j)
            r.principal_axes[i][j] = row[j].toDouble();
    }
    return r;
}

// ============================================================================
// 4. Exact Planar Section Properties Evaluation Implementation
// ============================================================================

QCborMap SectionPropertiesRequest::map() const {
    QCborMap m;
    m.insert(QStringLiteral("revision"), revision);
    m.insert(QStringLiteral("target_type"), targetType);
    if (targetType == "Sketch") {
        m.insert(QStringLiteral("sketch"), sketchId);
        m.insert(QStringLiteral("mode"), sketchMode);
    } else {
        m.insert(QStringLiteral("face"), faceReference.map());
    }
    return m;
}

SectionPropertiesRequest SectionPropertiesRequest::parse(const QCborMap &map) {
    SectionPropertiesRequest r;
    checkKeys(map, {"revision", "target_type"}, {"sketch", "mode", "face"});
    r.revision = wire::integer(map, "revision");
    check(r.revision >= 0, "InvalidInput", "Revision must be non-negative");
    r.targetType = wire::text(map, "target_type");
    check(r.targetType == "Sketch" || r.targetType == "Face", "InvalidInput",
          "target_type must be Sketch or Face");
    if (r.targetType == "Sketch") {
        check(map.contains(QStringLiteral("sketch")), "InvalidInput", "sketch id required");
        r.sketchId = wire::text(map, "sketch");
        domain::label(r.sketchId);
        r.sketchMode = wire::text(map, "mode");
        check(r.sketchMode == "AllClosed" || r.sketchMode == "SelectedRegions",
              "UnsupportedOperation", "Section properties support AllClosed or SelectedRegions");
    } else {
        check(map.contains(QStringLiteral("face")), "InvalidInput", "face reference required");
        check(map.value("face").isMap(), "InvalidInput", "face reference must be map");
        r.faceReference = domain::GeometryReference::parse(map.value("face").toMap());
        check(r.faceReference.role == "Face", "InvalidInput", "Face reference role must be Face");
    }
    return r;
}

QCborMap SectionPropertiesResult::map() const {
    QCborMap m;
    m.insert(QStringLiteral("revision"), revision);
    m.insert(QStringLiteral("valid"), valid);
    if (!reason.isEmpty())
        m.insert(QStringLiteral("reason"), reason);
    m.insert(QStringLiteral("area_m2"), area_m2);
    m.insert(QStringLiteral("perimeter_m"), perimeter_m);

    QCborArray cl;
    for (double v : centroid_local_m)
        cl.append(v);
    m.insert(QStringLiteral("centroid_local_m"), cl);

    QCborArray cp;
    for (double v : centroid_part_m)
        cp.append(v);
    m.insert(QStringLiteral("centroid_part_m"), cp);

    QCborArray sm;
    for (double v : second_moments_m4)
        sm.append(v);
    m.insert(QStringLiteral("second_moments_m4"), sm);

    m.insert(QStringLiteral("polar_moment_m4"), polar_moment_m4);

    QCborArray pm;
    for (double v : principal_moments_m4)
        pm.append(v);
    m.insert(QStringLiteral("principal_moments_m4"), pm);

    m.insert(QStringLiteral("principal_angle_rad"), principal_angle_rad);
    return m;
}

SectionPropertiesResult SectionPropertiesResult::parse(const QCborMap &map) {
    SectionPropertiesResult r;
    checkKeys(map,
              {"revision", "valid", "area_m2", "perimeter_m", "centroid_local_m", "centroid_part_m",
               "second_moments_m4", "polar_moment_m4", "principal_moments_m4",
               "principal_angle_rad"},
              {"reason"});
    r.revision = wire::integer(map, "revision");
    r.valid = map.value("valid").toBool();
    if (map.contains(QStringLiteral("reason")))
        r.reason = map.value("reason").toString();
    r.area_m2 = optNumber(map, "area_m2", 0.0, 1e12);
    r.perimeter_m = optNumber(map, "perimeter_m", 0.0, 1e12);

    auto clArr = map.value("centroid_local_m").toArray();
    check(clArr.size() == 2, "InvalidInput", "centroid_local_m must have 2 elements");
    r.centroid_local_m[0] = clArr[0].toDouble();
    r.centroid_local_m[1] = clArr[1].toDouble();

    r.centroid_part_m = parseVec3(map.value("centroid_part_m"), "centroid_part_m");

    auto smArr = map.value("second_moments_m4").toArray();
    check(smArr.size() == 3, "InvalidInput", "second_moments_m4 must have 3 elements");
    for (int i = 0; i < 3; ++i)
        r.second_moments_m4[i] = smArr[i].toDouble();

    r.polar_moment_m4 = optNumber(map, "polar_moment_m4", 0.0, 1e12);

    auto pmArr = map.value("principal_moments_m4").toArray();
    check(pmArr.size() == 2, "InvalidInput", "principal_moments_m4 must have 2 elements");
    r.principal_moments_m4[0] = pmArr[0].toDouble();
    r.principal_moments_m4[1] = pmArr[1].toDouble();

    r.principal_angle_rad =
        optNumber(map, "principal_angle_rad", -std::numbers::pi, std::numbers::pi);
    return r;
}

// ============================================================================
// 5. Appearance Precedence and Overrides Implementation
// ============================================================================

bool Appearance::operator==(const Appearance &other) const {
    return color_rgba == other.color_rgba && std::abs(roughness - other.roughness) < 1e-6f &&
           std::abs(metalness - other.metalness) < 1e-6f &&
           std::abs(transparency - other.transparency) < 1e-6f;
}

QCborMap Appearance::map() const {
    QCborMap m;
    QCborArray col;
    for (float v : color_rgba)
        col.append(v);
    m.insert(QStringLiteral("color_rgba"), col);
    m.insert(QStringLiteral("roughness"), roughness);
    m.insert(QStringLiteral("metalness"), metalness);
    m.insert(QStringLiteral("transparency"), transparency);
    return m;
}

Appearance Appearance::parse(const QCborMap &map) {
    Appearance a;
    checkKeys(map, {"color_rgba", "roughness", "metalness", "transparency"});
    a.color_rgba = parseVec4f(map.value("color_rgba"), "color_rgba");
    a.roughness = static_cast<float>(optNumber(map, "roughness", 0.0, 1.0, 0.5));
    a.metalness = static_cast<float>(optNumber(map, "metalness", 0.0, 1.0, 0.0));
    a.transparency = static_cast<float>(optNumber(map, "transparency", 0.0, 1.0, 0.0));
    return a;
}

QCborMap AppearanceOverride::map() const {
    QCborMap m;
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("level"), level);
    if (level == "Face") {
        m.insert(QStringLiteral("target"), faceReference.map());
    } else if (!target.isEmpty()) {
        m.insert(QStringLiteral("target"), target);
    }
    m.insert(QStringLiteral("appearance"), appearance.map());
    return m;
}

AppearanceOverride AppearanceOverride::parse(const QCborMap &map) {
    AppearanceOverride o;
    checkKeys(map, {"id", "level", "appearance"}, {"target"});
    o.id = wire::text(map, "id");
    domain::label(o.id);
    o.level = wire::text(map, "level");
    check(QStringList{"Face", "Feature", "Body", "Part"}.contains(o.level), "InvalidInput",
          "Invalid appearance override level: " + o.level);
    if (o.level == "Face") {
        check(map.contains(QStringLiteral("target")), "InvalidInput",
              "Face override requires target");
        check(map.value("target").isMap(), "InvalidInput",
              "Face override target must be a geometry reference");
        o.faceReference = domain::GeometryReference::parse(map.value("target").toMap());
        check(o.faceReference.role == "Face", "InvalidInput",
              "Face override reference role must be Face");
    } else if (o.level == "Feature" || o.level == "Body") {
        check(map.contains(QStringLiteral("target")), "InvalidInput",
              o.level + " override requires target");
        o.target = wire::text(map, "target");
        domain::label(o.target);
    }
    check(map.value("appearance").isMap(), "InvalidInput", "Appearance map required");
    o.appearance = Appearance::parse(map.value("appearance").toMap());
    return o;
}

Appearance resolveAppearance(const std::optional<Appearance> &faceOverride,
                             const std::optional<Appearance> &featureOverride,
                             const std::optional<Appearance> &bodyOverride,
                             const std::optional<Appearance> &partOverride,
                             const std::optional<Appearance> &materialDefault) {
    if (faceOverride)
        return *faceOverride;
    if (featureOverride)
        return *featureOverride;
    if (bodyOverride)
        return *bodyOverride;
    if (partOverride)
        return *partOverride;
    if (materialDefault)
        return *materialDefault;
    return Appearance{};
}

// ============================================================================
// 6. Named Section Views Implementation
// ============================================================================

QCborMap SectionView::map() const {
    QCborMap m;
    m.insert(QStringLiteral("version"), version);
    m.insert(QStringLiteral("id"), id);
    m.insert(QStringLiteral("name"), name);
    m.insert(QStringLiteral("active"), active);
    m.insert(QStringLiteral("support"), support.map());
    m.insert(QStringLiteral("offset"), offset);
    m.insert(QStringLiteral("flip"), flip);
    if (cap_color_rgba) {
        QCborArray c;
        for (float v : *cap_color_rgba)
            c.append(v);
        m.insert(QStringLiteral("cap_color_rgba"), c);
    }
    m.insert(QStringLiteral("clip_mode"), clip_mode);
    return m;
}

SectionView SectionView::parse(const QCborMap &map) {
    SectionView v;
    checkKeys(map, {"version", "id", "name", "active", "support", "offset", "flip", "clip_mode"},
              {"cap_color_rgba"});
    v.version = wire::integer(map, "version");
    check(v.version == 1, "InvalidInput", "Section view version must be 1");
    v.id = wire::text(map, "id");
    domain::label(v.id);
    v.name = wire::text(map, "name");
    check(!v.name.isEmpty(), "InvalidInput", "Section view name cannot be empty");
    v.active = map.value("active").toBool();
    check(map.value("support").isMap(), "InvalidInput", "Support reference required");
    v.support = domain::GeometryReference::parse(map.value("support").toMap());
    check(v.support.role == "Plane" || v.support.role == "Face", "InvalidInput",
          "Section view support must be Plane or Face role");
    v.offset = optNumber(map, "offset", -1000.0, 1000.0, 0.0);
    v.flip = map.value("flip").toBool();
    if (map.contains(QStringLiteral("cap_color_rgba"))) {
        v.cap_color_rgba = parseVec4f(map.value("cap_color_rgba"), "cap_color_rgba");
    }
    v.clip_mode = wire::text(map, "clip_mode");
    check(v.clip_mode == "ViewOnly", "UnsupportedOperation",
          "Section view clip_mode must be ViewOnly");
    return v;
}

} // namespace cad::features

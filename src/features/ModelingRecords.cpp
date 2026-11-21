#include "ModelingRecords.hpp"
namespace cad::features {
using domain::check;
namespace {
QStringList referencePath(QString path) {
    check(QStringList{"start/reference", "direction1/reference", "direction2/reference", "axis",
                      "plane", "seed/source", "target/source", "neutral", "pull", "reference",
                      "placement/face", "termination/reference", "reference_face", "edge",
                      "path/reference", "centerline/reference"}
                  .contains(path) ||
              QRegularExpression("^(faces|edges|faces1|faces2|side_faces1|center_faces|side_faces2|"
                                 "seed/faces|remove_faces|guides)/([0-9]|[12][0-9]|3[01])$")
                  .match(path)
                  .hasMatch() ||
              QRegularExpression(
                  "^(radii/([0-9]|[12][0-9]|3[01])/edge|overrides/([0-9]|[12][0-9]|3[01])/face)$")
                  .match(path)
                  .hasMatch(),
          "InvalidInput", "Choose an explicit modeling reference path");
    return path.split('/');
}
QCborValue at(const QCborMap &intent, const QStringList &parts) {
    auto value = intent.value(parts[0]);
    if (parts.size() == 3 && parts[0] == "seed" && parts[1] == "faces") {
        check(value.isArray() && parts[2].toInt() < value.toArray().size(), "InvalidInput",
              "Face seed slot missing");
        value = value.toArray()[parts[2].toInt()];
        check(value.isMap(), "InvalidInput", "Typed face seed required");
        return value;
    }
    if (parts.size() == 3 && parts[0] == "overrides" && parts[2] == "face") {
        check(value.isArray() && parts[1].toInt() < value.toArray().size(), "InvalidInput",
              "Override slot missing");
        value = value.toArray()[parts[1].toInt()].toMap().value("face");
        check(value.isMap(), "InvalidInput", "Typed override face required");
        return value;
    }
    if (parts.size() == 2) {
        if (value.isArray()) {
            check(parts[1].toInt() < value.toArray().size(), "InvalidInput",
                  "Array reference slot missing");
            value = value.toArray()[parts[1].toInt()];
        } else {
            check(value.isMap(), "InvalidInput", "Modeling reference path missing");
            value = value.toMap().value(parts[1]);
        }
    } else if (parts.size() == 3) {
        check(value.isMap() || value.isArray(), "InvalidInput", "Modeling nested path missing");
        if (value.isArray()) {
            check(parts[1].toInt() < value.toArray().size(), "InvalidInput",
                  "Array reference slot missing");
            value = value.toArray()[parts[1].toInt()].toMap().value(parts[2]);
        } else {
            value = value.toMap().value(parts[1]).toArray()[parts[2].toInt()];
        }
    }
    check(value.isMap(), "InvalidInput", "Typed modeling reference missing");
    return value;
}
void token(QCborValue value) {
    check(value.isString() &&
              QRegularExpression("^(edge|face)/[0-9a-f]{64}$").match(value.toString()).hasMatch(),
          "InvalidProject", "Typed Boolean topology token required");
}
} // namespace
domain::GeometryReference intentReference(const QCborMap &intent, QString path) {
    ModelingIntent::parse(intent);
    return domain::GeometryReference::parse(at(intent, referencePath(path)).toMap());
}
QCborArray replaceIntentReference(const QCborArray &intents, QString owner, QString path,
                                  const domain::GeometryReference &replacement) {
    domain::id(owner);
    replacement.validate();
    auto parts = referencePath(path);
    auto result = intents;
    bool found = false;
    for (qsizetype i = 0; i < intents.size(); ++i) {
        auto m = ModelingIntent::parse(intents[i].toMap()).map();
        if (m.value("id").toString() != owner)
            continue;
        check(!found, "InvalidInput", "Repeated modeling owner");
        found = true;
        at(m, parts);
        if (parts.size() == 1)
            m.insert(parts[0], replacement.map());
        else if (parts.size() == 2) {
            if (m.value(parts[0]).isArray()) {
                auto arr = m.value(parts[0]).toArray();
                arr[parts[1].toInt()] = replacement.map();
                m.insert(parts[0], arr);
            } else {
                auto map = m.value(parts[0]).toMap();
                map.insert(parts[1], replacement.map());
                m.insert(parts[0], map);
            }
        } else if (parts.size() == 3) {
            if (parts[0] == "radii") {
                auto arr = m.value("radii").toArray();
                auto item = arr[parts[1].toInt()].toMap();
                item.insert(QStringLiteral("edge"), replacement.map());
                arr[parts[1].toInt()] = item;
                m.insert(QStringLiteral("radii"), arr);
            } else if (parts[0] == "seed" && parts[1] == "faces") {
                auto arr = m.value("seed").toArray();
                arr[parts[2].toInt()] = replacement.map();
                m.insert(QStringLiteral("seed"), arr);
            } else if (parts[0] == "overrides" && parts[2] == "face") {
                auto arr = m.value("overrides").toArray();
                auto item = arr[parts[1].toInt()].toMap();
                item.insert(QStringLiteral("face"), replacement.map());
                arr[parts[1].toInt()] = item;
                m.insert(QStringLiteral("overrides"), arr);
            }
        }
        result[i] = ModelingIntent::parse(m).map();
    }
    check(found, "ReferenceMissing", "Modeling reference owner missing");
    return result;
}
void validateModelingRecords(const QCborMap &m) {
    wire::keys(m, {"version", "intents", "provenance"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported modeling collection version");
    check(m.value("intents").isArray() && m.value("provenance").isArray(), "InvalidProject",
          "Typed modeling collections required");
    auto intents = m.value("intents").toArray(), provenance = m.value("provenance").toArray();
    check(intents.size() <= 100 && QCborValue(intents).toCbor().size() <= 131072 &&
              QCborValue(m).toCbor().size() <= 262144,
          "ResourceLimit", "Modeling metadata exceeds existing admission");
    check(intents.size() == provenance.size(), "InvalidProject",
          "Every modeling intent requires provenance");
    QSet<QString> seen;
    for (qsizetype i = 0; i < intents.size(); ++i) {
        check(intents[i].isMap() && provenance[i].isMap(), "InvalidProject",
              "Typed intent/provenance required");
        auto f = ModelingIntent::parse(intents[i].toMap()).map(), p = provenance[i].toMap();
        auto id = f.value("id").toString();
        check(!seen.contains(id), "InvalidProject", "Duplicate modeling identity");
        seen.insert(id);
        wire::keys(p, {"feature", "kind", "operation", "target", "outputs", "history"});
        bool directOp = f.value("kind") == "Draft" || f.value("kind") == "Fillet" ||
                        f.value("kind") == "Chamfer" || f.value("kind") == "CosmeticThread" ||
                        f.value("kind") == "Shell";
        check(p.value("feature") == f.value("id") && p.value("kind") == f.value("kind") &&
                  p.value("operation") ==
                      (directOp ? QCborValue(f.value("kind")) : f.value("operation")) &&
                  p.value("target") == f.value("target") && p.value("outputs").isArray() &&
                  p.value("history").isArray(),
              "InvalidProject", "Modeling provenance disagrees with original intent");
        auto outputs = p.value("outputs").toArray();
        check(!outputs.empty() && outputs.size() <= 10, "ResourceLimit",
              "Modeling output identities exceed body admission");
        QSet<QString> bodies;
        for (auto value : outputs) {
            auto body = domain::id(value.toString());
            check(!bodies.contains(body), "InvalidProject", "Repeated output body identity");
            bodies.insert(body);
        }
        for (auto value : p.value("history").toArray()) {
            check(value.isMap(), "InvalidProject", "Typed kernel provenance required");
            auto h = value.toMap();
            wire::keys(h, {"feature", "input", "generated", "modified", "deleted"});
            check(h.value("feature") == f.value("id") && h.value("deleted").isBool() &&
                      h.value("generated").isArray() && h.value("modified").isArray(),
                  "InvalidProject", "Invalid kernel provenance owner/type");
            token(h.value("input"));
            for (auto key : {"generated", "modified"})
                for (auto t : h.value(QString::fromLatin1(key)).toArray())
                    token(t);
        }
    }
}
} // namespace cad::features

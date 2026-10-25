#include "Plan.hpp"
#include "domain/Expressions.hpp"
namespace cad::evaluation {
using domain::check;
using sketch::fields;
namespace {
QString symbol(QString id, QString key) {
    return "F_" + QString(id).remove('-') + "_" + key;
}
expression::Dimension dimension(QString key) {
    return key == "angle"   ? expression::angle
           : key == "count" ? expression::scalar
                            : expression::length;
}
QString literal(double value, QString key) {
    return QString::number(value, 'g', 17) + (key == "angle" ? " rad" : key == "count" ? "" : " m");
}
} // namespace
Plan::Plan(QCborArray sketches, QCborArray definitions) {
    auto all = features::parse(definitions);
    QMap<QString, int> index, owners;
    QMap<QString, QByteArray> sketchHashes;
    QMap<QString, QString> names;
    check(sketches.size() <= 32 && QCborValue(sketches).toCbor().size() <= 524288, "ResourceLimit",
          "Impact profile union exceeds bounded admission");
    for (auto value : sketches) {
        auto s = sketch::Sketch::parse(value.toMap());
        check(!sketchHashes.contains(s.id), "InvalidInput", "Duplicate profile identity");
        sketchHashes.insert(s.id, QCborValue(s.map()).toCbor(QCborValue::SortKeysInMaps));
    }
    for (size_t i = 0; i < all.size(); ++i) {
        auto &f = all[i];
        index.insert(f.id, int(i));
        owners.insert(f.id, int(i));
        for (int k = 1; k < 10; ++k)
            owners.insert(QUuid::createUuidV5(QUuid(f.id), QByteArray::number(k))
                              .toString(QUuid::WithoutBraces),
                          int(i));
        for (auto it = f.parameters.cbegin(); it != f.parameters.cend(); ++it)
            if (it.value().isDouble() || it.value().isInteger())
                names.insert(symbol(f.id, it.key().toString()),
                             literal(it.value().toDouble(), it.key().toString()));
        for (auto it = f.expressions.cbegin(); it != f.expressions.cend(); ++it)
            names.insert(symbol(f.id, it.key()), it.value());
    }
    QMap<QString, QString> lastWriter;
    std::vector<QStringList> deps(all.size());
    for (size_t i = 0; i < all.size(); ++i) {
        auto &f = all[i];
        auto &d = deps[i];
        auto add = [&](QString id) {
            if (!id.isEmpty() && !d.contains(id))
                d.append(id);
        };
        auto p = f.parameters;
        auto source = p.value("source").toString(), target = p.value("target").toString();
        if (!source.isEmpty()) {
            check(index.contains(source), "ReferenceMissing",
                  "Source feature is missing: " + source);
            add(source);
        }
        if (!target.isEmpty()) {
            check(owners.contains(target), "ReferenceMissing", "Target body owner is missing");
            add(all[owners[target]].id);
            add(lastWriter.value(target));
            lastWriter[target] = f.id;
        }
        if (p.value("operation").toString() == "NewBody")
            for (auto it = owners.cbegin(); it != owners.cend(); ++it)
                if (it.value() == int(i))
                    lastWriter[it.key()] = f.id;
        QRegularExpression reference("F_([0-9a-f]{32})_[A-Za-z][A-Za-z0-9_]*");
        for (auto expression : f.expressions) {
            auto matches = reference.globalMatch(expression);
            while (matches.hasNext()) {
                auto hex = matches.next().captured(1);
                auto id = QUuid::fromRfc4122(QByteArray::fromHex(hex.toLatin1()))
                              .toString(QUuid::WithoutBraces);
                check(index.contains(id), "ReferenceMissing", "Expression feature is missing");
                add(id);
            }
        }
        auto sketch = p.value("sketch").toString();
        check(sketch.isEmpty() || sketchHashes.contains(sketch), "ReferenceMissing",
              "Profile sketch is missing");
    }
    QSet<QString> visiting, done;
    std::function<void(QString)> visit = [&](QString id) {
        check(!visiting.contains(id), "DependencyCycle", "Feature/expression dependency cycle");
        if (done.contains(id))
            return;
        visiting.insert(id);
        for (auto dep : deps[index[id]])
            visit(dep);
        visiting.remove(id);
        done.insert(id);
    };
    for (auto &f : all)
        visit(f.id);
    expression::Environment environment(names, 1024);
    environment.validateAll();
    QMap<QString, QByteArray> fingerprints;
    for (size_t i = 0; i < all.size(); ++i) {
        auto f = all[i];
        for (auto dep : deps[i])
            check(index[dep] < int(i), "DependencyOrder",
                  "Feature must follow its source/target/expression dependencies");
        for (auto it = f.expressions.cbegin(); it != f.expressions.cend(); ++it) {
            auto q = environment.name(symbol(f.id, it.key()));
            check(q.dimension == dimension(it.key()), "UnitMismatch",
                  "Feature expression has incorrect dimensions");
            if (it.key() == "count") {
                check(q.value == std::round(q.value), "InvalidInput",
                      "Pattern count must be an integer");
                f.parameters.insert(it.key(), qint64(q.value));
            } else
                f.parameters.insert(it.key(), q.value);
        }
        features::Feature::parse(f.map());
        auto semantic = f.map();
        semantic.remove(QStringLiteral("name"));
        QCborArray upstream;
        for (auto dep : deps[i])
            upstream.append(fingerprints.value(dep));
        auto bytes =
            QCborValue(
                fields({{"engine", "OCCT=7.9.2;part=2;SI;tol-mm=0.0001;fingerprint=1"},
                        {"definition", semantic},
                        {"sketch", sketchHashes.value(f.parameters.value("sketch").toString())},
                        {"upstream", upstream}}))
                .toCbor(QCborValue::SortKeysInMaps);
        auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
        fingerprints[f.id] = hash;
        nodes.push_back({f, deps[i], hash});
    }
    for (auto &n : nodes) {
        QSet<QString> affected;
        std::function<void(QString)> collect = [&](QString id) {
            for (auto &candidate : nodes)
                if (candidate.dependencies.contains(id) &&
                    !affected.contains(candidate.feature.id)) {
                    affected.insert(candidate.feature.id);
                    collect(candidate.feature.id);
                }
        };
        collect(n.feature.id);
        QStringList ordered;
        for (auto &candidate : nodes)
            if (affected.contains(candidate.feature.id))
                ordered.append(candidate.feature.id);
        descendants[n.feature.id] = ordered;
    }
}
QCborArray Plan::map() const {
    QCborArray out;
    for (auto &n : nodes)
        out.append(
            fields({{"id", n.feature.id},
                    {"dependencies", QCborArray::fromStringList(n.dependencies)},
                    {"fingerprint", n.fingerprint.toHex()},
                    {"descendants", QCborArray::fromStringList(descendants.value(n.feature.id))}}));
    return out;
}
QCborMap impact(QCborArray sketches, QCborArray before, QCborArray after) {
    Plan a(sketches, before), b(sketches, after);
    QMap<QString, QByteArray> hashes;
    for (auto &n : a.nodes)
        hashes[n.feature.id] = n.fingerprint;
    QCborArray changed, reused, deleted;
    for (auto &n : b.nodes) {
        (hashes.value(n.feature.id) == n.fingerprint ? reused : changed).append(n.feature.id);
        hashes.remove(n.feature.id);
    }
    for (auto id : hashes.keys())
        deleted.append(id);
    return fields(
        {{"affected", changed}, {"unaffected", reused}, {"deleted", deleted}, {"graph", b.map()}});
}
QCborArray change(QCborArray sketches, QCborArray before, QString action, QString id,
                  QCborMap options) {
    auto all = features::parse(before);
    Plan plan(sketches, before);
    int found = -1;
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i].id == id)
            found = int(i);
    check(found >= 0, "ReferenceMissing", "Feature to edit is missing");
    if (action == "Rename") {
        wire::keys(options, {"name"});
        all[found].name = domain::label(wire::text(options, "name"));
    } else if (action == "Suppress") {
        wire::keys(options, {"suppressed"});
        check(options.value("suppressed").isBool(), "InvalidInput",
              "Explicit suppression choice required");
        all[found].suppressed = options.value("suppressed").toBool();
    } else if (action == "Delete") {
        wire::keys(options, {"cascade"});
        check(options.value("cascade").isBool(), "InvalidInput",
              "Choose dependent deletion explicitly");
        auto descendants = plan.descendants.value(id);
        check(descendants.empty() || options.value("cascade").toBool(), "DependencyImpact",
              "Deletion affects dependent features; explicitly choose cascade");
        QSet<QString> remove{id};
        if (options.value("cascade").toBool())
            for (auto d : descendants)
                remove.insert(d);
        std::erase_if(all, [&](auto &f) { return remove.contains(f.id); });
    } else if (action == "Reorder") {
        wire::keys(options, {"index"});
        auto to = wire::integer(options, "index", all.size() - 1);
        auto f = all[found];
        all.erase(all.begin() + found);
        all.insert(all.begin() + to, f);
    } else
        throw domain::Error("UnsupportedOperation", "Unsupported history action");
    auto candidate = features::encode(all);
    Plan valid(sketches, candidate);
    return candidate;
}
} // namespace cad::evaluation

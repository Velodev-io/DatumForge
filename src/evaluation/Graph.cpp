#include "Graph.hpp"
#include <functional>
namespace cad::evaluation {
Graph::Graph(const std::vector<GraphNode> &nodes, QStringList history, QString document) {
    domain::check(nodes.size() <= 132 && history.size() <= 132, "ResourceLimit",
                  "Model dependency metadata exceeds the bounded profile/feature union");
    if (document != "Self")
        domain::id(document);
    QMap<QString, const GraphNode *> byId;
    size_t sketches = 0, features = 0;
    for (auto &node : nodes) {
        sketches += node.kind == "Sketch";
        features += node.kind == "Feature";
        domain::check(sketches <= 32 && features <= 100 && node.references.size() <= 512 &&
                          node.members.size() <= 256,
                      "ResourceLimit", "Model dependency node exceeds existing admission bounds");
        domain::id(node.id);
        domain::check(!byId.contains(node.id), "InvalidInput", "Duplicate model node identity");
        domain::check(
            QStringList{"Sketch", "DatumPlane", "DatumAxis", "DerivedCurve", "Feature"}.contains(
                node.kind),
            "InvalidInput", "Unsupported model node kind");
        domain::check(node.kind == "Sketch" || node.members.empty(), "InvalidInput",
                      "Only sketch nodes own member entities");
        for (auto it = node.members.cbegin(); it != node.members.cend(); ++it) {
            domain::id(it.key());
            domain::check(it.value() == "Point" || it.value() == "Curve", "InvalidInput",
                          "Typed sketch member role required");
        }
        byId.insert(node.id, &node);
    }
    for (auto &node : nodes) {
        edges.insert(node.id, {});
        for (auto &ref : node.references) {
            ref.validate();
            domain::check(ref.document == "Self" || ref.document == document,
                          "UnsupportedOperation",
                          "Cross-document geometry edges are not implemented");
            domain::check(byId.contains(ref.object), "ReferenceMissing", "Model source is missing");
            const auto &source = *byId.value(ref.object);
            bool compatible = ref.role == "Object";
            if (source.kind == "Sketch") {
                compatible = compatible || ref.role == "Plane";
                if (ref.role == "Point" || ref.role == "Curve")
                    compatible = source.members.contains(ref.entity) &&
                                 source.members.value(ref.entity) == ref.role;
            } else if (source.kind == "DatumPlane")
                compatible = compatible || ref.role == "Plane";
            else if (source.kind == "DatumAxis")
                compatible = compatible || ref.role == "Axis";
            else if (source.kind == "DerivedCurve")
                compatible = compatible || (ref.role == "Curve" && ref.entity.isEmpty());
            else if (source.kind == "Feature")
                compatible =
                    compatible || ref.role == "Face" || ref.role == "Edge" || ref.role == "Vertex";
            domain::check(compatible, "ReferenceMissing",
                          "Model source no longer has the referenced geometry role/member");
            if (!edges[node.id].contains(ref.object))
                edges[node.id].append(ref.object);
        }
    }
    QMap<QString, int> states;
    std::function<void(QString)> visit = [&](QString id) {
        domain::check(states.value(id) != 1, "DependencyCycle",
                      "Cyclic model geometry dependencies");
        if (states.value(id) == 2)
            return;
        states[id] = 1;
        for (auto dep : edges.value(id))
            visit(dep);
        states[id] = 2;
        ordered.append(id);
    };
    for (auto &node : nodes)
        visit(node.id);
    QMap<QString, qsizetype> positions;
    for (qsizetype i = 0; i < history.size(); ++i) {
        auto id = history[i];
        domain::check(edges.contains(id), "ReferenceMissing", "History node is missing");
        domain::check(!positions.contains(id), "InvalidInput", "Repeated history node");
        positions.insert(id, i);
    }
    for (auto root : ordered) {
        QSet<QString> affected;
        for (auto id : ordered)
            for (auto dep : edges.value(id))
                if (dep == root || affected.contains(dep)) {
                    affected.insert(id);
                    break;
                }
        for (auto id : ordered)
            if (affected.contains(id)) {
                downstream[root].append(id);
                if (positions.contains(root) && positions.contains(id))
                    domain::check(positions.value(root) < positions.value(id), "DependencyOrder",
                                  "Model history must follow every upstream geometry dependency");
            }
    }
}
QStringList Graph::dependencies(QString id) const {
    domain::check(edges.contains(id), "ReferenceMissing", "Model node is missing");
    return edges.value(id);
}
QStringList Graph::descendants(QString id) const {
    domain::check(edges.contains(id), "ReferenceMissing", "Model node is missing");
    return downstream.value(id);
}
} // namespace cad::evaluation

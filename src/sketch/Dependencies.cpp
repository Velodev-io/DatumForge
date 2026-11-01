#include "Dependencies.hpp"
namespace cad::sketch {
std::vector<evaluation::GraphNode> dependencyNodes(const std::vector<Sketch> &all) {
    std::vector<evaluation::GraphNode> nodes;
    for (auto &s : all) {
        evaluation::GraphNode node{s.id, "Sketch", {}, {}};
        for (auto &e : s.entities)
            node.members.insert(e.id, e.kind == "Point" ? "Point" : "Curve");
        for (auto &c : s.constraints)
            if (c.reference) {
                domain::GeometryReference ref;
                ref.document = c.reference->document;
                ref.object = c.reference->sketch;
                ref.entity = c.reference->entity;
                ref.role = c.reference->role == "Point" ? "Point" : "Curve";
                if (ref.role == "Curve")
                    ref.parameter = c.reference->parameter;
                node.references.push_back(ref);
            }
        nodes.push_back(std::move(node));
    }
    return nodes;
}
std::vector<size_t> dependencyOrder(const std::vector<Sketch> &all) {
    evaluation::Graph graph(dependencyNodes(all));
    QMap<QString, size_t> indices;
    for (size_t i = 0; i < all.size(); ++i)
        indices.insert(all[i].id, i);
    std::vector<size_t> order;
    for (auto id : graph.order())
        order.push_back(indices.value(id));
    return order;
}
} // namespace cad::sketch

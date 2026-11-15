#pragma once
#include "domain/GeometryReference.hpp"
#include <vector>
namespace cad::evaluation {
struct GraphNode {
    QString id, kind;
    std::vector<domain::GeometryReference> references;
    QMap<QString, QString> members; // Stable sketch-member UUID -> Point or Curve.
};
class Graph {
    QStringList ordered;
    QMap<QString, QStringList> edges, downstream;

  public:
    explicit Graph(const std::vector<GraphNode> &nodes = {}, QStringList history = {},
                   QString document = "Self");
    const QStringList &order() const {
        return ordered;
    }
    QStringList dependencies(QString id) const;
    QStringList descendants(QString id) const;
};
} // namespace cad::evaluation

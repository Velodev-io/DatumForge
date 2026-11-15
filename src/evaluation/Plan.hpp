#pragma once
#include "Graph.hpp"
#include "features/Model.hpp"
namespace cad::evaluation {
struct Node {
    features::Feature feature;
    QStringList dependencies;
    QByteArray fingerprint;
};
struct Plan {
    Graph geometryGraph;
    std::vector<Node> nodes;
    QMap<QString, QStringList> descendants;
    // Transient members permit structural planning before derived geometry is
    // materialized. Exact Part builds validate actual members (the default).
    explicit Plan(QCborArray sketches, QCborArray features,
                  const QMap<QString, QMap<QString, QString>> &transientMembers = {});
    QCborArray map() const;
};
QCborMap impact(QCborArray sketches, QCborArray before, QCborArray after);
QCborArray change(QCborArray sketches, QCborArray before, QString action, QString feature,
                  QCborMap options);
} // namespace cad::evaluation

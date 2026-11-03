#pragma once
#include "evaluation/Graph.hpp"
namespace cad::features {
// CM5 semantic contract only. Not an accepted Feature or a geometry adapter.
class ModelingIntent {
    QCborMap definition;
    evaluation::GraphNode node;
    QMap<QString, QString> sourceKinds;
    ModelingIntent() = default;

  public:
    static ModelingIntent parse(const QCborMap &map);
    QCborMap map() const {
        return definition;
    }
    evaluation::GraphNode graphNode() const {
        return node;
    }
    evaluation::Graph graph(const std::vector<evaluation::GraphNode> &upstream, QStringList history,
                            QString document = "Self") const;
};
} // namespace cad::features

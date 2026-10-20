#pragma once
#include "features/Model.hpp"
namespace cad::evaluation {
struct Node { features::Feature feature; QStringList dependencies; QByteArray fingerprint; };
struct Plan {
    std::vector<Node> nodes;
    QMap<QString,QStringList> descendants;
    explicit Plan(QCborArray sketches,QCborArray features);
    QCborArray map()const;
};
QCborMap impact(QCborArray sketches,QCborArray before,QCborArray after);
QCborArray change(QCborArray sketches,QCborArray before,QString action,QString feature,QCborMap options);
} // namespace cad::evaluation

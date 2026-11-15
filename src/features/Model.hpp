#pragma once
#include "sketch/Sketch.hpp"
namespace cad::features {
struct Feature {
    QString id = newId(), name = "Feature", kind = "Extrude";
    QCborMap parameters;
    bool suppressed = false;
    QMap<QString, QString> expressions;
    QCborMap map() const;
    static Feature parse(const QCborMap &map);
};
QCborArray encode(const std::vector<Feature> &features);
std::vector<Feature> parse(const QCborArray &features);
double number(const QCborMap &p, QString key, double low, double high);
sketch::Vec3 vector(const QCborMap &p, QString key, bool direction = false);
QCborArray array(sketch::Vec3 v);
} // namespace cad::features

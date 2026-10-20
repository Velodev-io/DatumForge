#pragma once
#include "domain/Documents.hpp"
#include <array>
#include <vector>
namespace cad::sketch {
using Vec3 = std::array<double, 3>;
struct Vec2 {
    double x = 0, y = 0;
};
struct Frame {
    Vec3 origin{0, 0, 0}, u{1, 0, 0}, v{0, 1, 0};
    void validate() const;
    Vec3 world(Vec2 p) const;
    Vec2 local(Vec3 p) const;
};
struct Entity {
    QString id = newId(), kind;
    bool construction = false;
    Vec2 point;
    QString a, b; // line endpoints, or circle/arc center in a
    double radius = .005, start = 0, sweep = 1.5707963267948966;
};
struct Constraint {
    QString id = newId(), kind, a, b, expression;
    double value = 0, second = 0;
};
struct Sketch {
    QString id = newId(), name = "Sketch";
    Frame frame;
    std::vector<Entity> entities;
    std::vector<Constraint> constraints;
    QMap<QString, QString> names;
    Entity &entity(QString id);
    const Entity &entity(QString id) const;
    QString point(Vec2 p, bool construction = false);
    QString line(QString a, QString b, bool construction = false);
    QString circle(QString center, double radius, bool construction = false);
    QString arc(QString center, double radius, double start, double sweep,
                bool construction = false);
    QStringList polyline(const std::vector<Vec2> &points, bool closed, bool construction = false);
    QStringList rectangle(Vec2 first, Vec2 second, bool construction = false);
    Vec2 snap(Vec2 p, double tolerance, double grid, QString *pointId = nullptr) const;
    void validate() const;
    QCborMap map() const;
    static Sketch parse(const QCborMap &map);
};
QCborMap fields(std::initializer_list<std::pair<QString, QCborValue>> list);
QCborArray encode(const std::vector<Sketch> &sketches);
std::vector<Sketch> parse(const QCborArray &sketches);
double length(Vec2 a, Vec2 b);
QString constraintReason(const Sketch &sketch, QString kind, QString first, QString second);
} // namespace cad::sketch

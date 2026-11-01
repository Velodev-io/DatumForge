#pragma once
#include "domain/Documents.hpp"
#include <array>
#include <optional>
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
    QStringList poles;
    std::vector<double> weights, knots;
    std::vector<int> multiplicities;
    int degree = 3;
};
struct CurveReference {
    QString document = "Self", sketch, entity, role;
    double lower = 0, upper = 1, parameter = 0;
    void validate() const;
    QCborMap map() const;
    static CurveReference parse(const QCborMap &map);
};
struct Constraint {
    QString id = newId(), kind, a, b, expression;
    double value = 0, second = 0;
    QString third;
    bool reversed = false;
    std::optional<CurveReference> reference;
    bool driving = true;
    std::optional<Vec2> placement;   // sketch-plane SI annotation anchor
    std::optional<double> increment; // SI step size; annotation/editor metadata
    bool drawing = false;
    QStringList members;
    std::vector<bool> directions;
    bool closed = false;
};
struct ConstructionGroup {
    QString id = newId(), method;
    QString kind = "Rectangle", lengthMode = "Center to Center";
    bool dimensions = false;
    QStringList vertices, edges, diagonals, relations;
    QStringList supports;
    QString center;
    QCborMap map() const;
    static ConstructionGroup parse(const QCborMap &map);
};
struct Sketch {
    QString id = newId(), name = "Sketch";
    Frame frame;
    std::vector<Entity> entities;
    std::vector<Constraint> constraints;
    std::vector<ConstructionGroup> groups;
    QCborArray edits; // schema-5 bounded historical edit provenance
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
    bool extendedRelations() const;
    bool primitiveSemantics() const;
    bool dimensionSemantics() const;
    bool associatedDimensionSemantics() const;
    bool interactionSemantics() const;
    QCborMap map() const;
    static Sketch parse(const QCborMap &map);
};
QCborMap fields(std::initializer_list<std::pair<QString, QCborValue>> list);
QCborArray encode(const std::vector<Sketch> &sketches);
std::vector<Sketch> parse(const QCborArray &sketches);
double length(Vec2 a, Vec2 b);
QString constraintReason(const Sketch &sketch, QString kind, QString first, QString second,
                         QString third = {});
} // namespace cad::sketch

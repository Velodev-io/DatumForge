#pragma once
#include "domain/Documents.hpp"
#include "domain/GeometryReference.hpp"
#include "evaluation/Graph.hpp"
#include "sketch/Sketch.hpp"
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace cad::sketch {

inline double dot(const Vec3 &a, const Vec3 &b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

inline double norm(const Vec3 &a) {
    return std::sqrt(dot(a, a));
}

inline Vec3 normalize(const Vec3 &a) {
    double n = norm(a);
    if (n < 1e-14)
        throw domain::Error("InvalidInput", "Cannot normalize near-zero vector");
    return {a[0] / n, a[1] / n, a[2] / n};
}

struct SpatialEntity {
    QString id = newId(), kind;
    bool construction = false;
    Vec3 point{0, 0, 0};
    QString a, b;
    QString center;
    Vec3 normal{0, 0, 1};
    double radius = 0.005;
    QStringList poles;
    int degree = 3;
    std::vector<double> weights, knots;

    void validate() const;
    QCborMap map() const;
    static SpatialEntity parse(const QCborMap &map);
};

struct SpatialConstraint {
    QString id = newId(), kind, a, b, third;
    std::optional<domain::GeometryReference> plane;
    double value = 0;
    bool driving = true;
    bool reversed = false;

    void validate() const;
    QCborMap map() const;
    static SpatialConstraint parse(const QCborMap &map);
};

struct SpatialSketch {
    QString id = newId(), name = "3D Sketch";
    int version = 1;
    QString activePlane = "XY"; // Transient editor state: "XY", "YZ", "ZX"
    std::vector<SpatialEntity> entities;
    std::vector<SpatialConstraint> constraints;

    void validate() const;
    QCborMap map() const;
    static SpatialSketch parse(const QCborMap &map);

    evaluation::GraphNode graphNode() const;
    std::vector<domain::GeometryReference> references() const;
    QMap<QString, QString> members() const;

    SpatialEntity &entity(const QString &id);
    const SpatialEntity &entity(const QString &id) const;
    QString point(Vec3 p, bool construction = false);
    QString line(const QString &a, const QString &b, bool construction = false);
    QString arc(const QString &center, const QString &a, const QString &b, const Vec3 &normal,
                double radius, bool construction = false);
};

QCborArray encodeSpatial(const std::vector<SpatialSketch> &sketches);
std::vector<SpatialSketch> parseSpatial(const QCborArray &sketches);

} // namespace cad::sketch

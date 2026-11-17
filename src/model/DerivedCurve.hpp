#pragma once
#include "domain/Documents.hpp"
#include "domain/GeometryReference.hpp"
#include "evaluation/Graph.hpp"
#include "sketch/SpatialSketch.hpp"
#include <optional>
#include <vector>

namespace cad::model {

struct BranchSelection {
    int index = 0;
    QString token = "branch/0";
    bool reversed = false;
    double startParam = 0.0, endParam = 1.0;

    void validate() const;
    QCborMap map() const;
    static BranchSelection parse(const QCborMap &map);
};

struct SketchOnSketchDefinition {
    domain::GeometryReference sketch1, sketch2;
    BranchSelection branch;

    void validate() const;
    QCborMap map() const;
    static SketchOnSketchDefinition parse(const QCborMap &map);
};

struct SketchOnFacesDefinition {
    domain::GeometryReference sketch;
    std::vector<domain::GeometryReference> faces;
    QString mode = "AlongVector"; // "AlongVector" or "NormalToFace"
    std::optional<domain::GeometryReference> direction;
    BranchSelection branch;

    void validate() const;
    QCborMap map() const;
    static SketchOnFacesDefinition parse(const QCborMap &map);
};

struct CompositeCurveMember {
    domain::GeometryReference source;
    bool reversed = false;

    void validate() const;
    QCborMap map() const;
    static CompositeCurveMember parse(const QCborMap &map);
};

struct CompositeCurveDefinition {
    std::vector<CompositeCurveMember> members;
    bool closed = false;

    void validate() const;
    QCborMap map() const;
    static CompositeCurveDefinition parse(const QCborMap &map);
};

struct HelixSpiralDefinition {
    QString mode = "PitchAndRevolution"; // "PitchAndRevolution", "HeightAndRevolution",
                                         // "HeightAndPitch", "Spiral"
    domain::GeometryReference axis;
    std::optional<domain::GeometryReference> center;
    double radius = 0.01;
    double pitch = 0.005;
    double height = 0.05;
    double revolutions = 5.0;
    double taperAngle = 0.0;
    double startAngle = 0.0;
    bool clockwise = true;

    void validate() const;
    QCborMap map() const;
    static HelixSpiralDefinition parse(const QCborMap &map);
};

struct CurveFrame {
    sketch::Vec3 point{0, 0, 0};
    sketch::Vec3 tangent{0, 0, 1};
    sketch::Vec3 normal{1, 0, 0};
    sketch::Vec3 binormal{0, 1, 0};

    static CurveFrame evaluate(sketch::Vec3 point, sketch::Vec3 tangent, bool reversed = false,
                               std::optional<sketch::Vec3> preferredUp = std::nullopt);
};

struct DerivedCurve {
    QString id = newId(), name = "DerivedCurve", kind;
    int version = 1;
    std::optional<SketchOnSketchDefinition> sketchOnSketch;
    std::optional<SketchOnFacesDefinition> sketchOnFaces;
    std::optional<CompositeCurveDefinition> compositeCurve;
    std::optional<HelixSpiralDefinition> helixSpiral;

    void validate() const;
    QCborMap map() const;
    static DerivedCurve parse(const QCborMap &map);

    evaluation::GraphNode graphNode() const;
    std::vector<domain::GeometryReference> references() const;
};

QCborArray encodeDerivedCurves(const std::vector<DerivedCurve> &curves);
std::vector<DerivedCurve> parseDerivedCurves(const QCborArray &curves);

} // namespace cad::model

#pragma once
#include "SpatialSketch.hpp"
#include <functional>

namespace cad::sketch {

using PlaneResolver = std::function<sketch::Frame(const domain::GeometryReference &)>;

struct SpatialSolveResult {
    SpatialSketch sketch;
    QString status = "FailedSolve", message;
    QStringList conflicting, redundant;
    int freedom = -1;
    int rank = 0;
    double maximumResidual = 0;
    bool valid = false;

    QCborMap diagnostic() const;
};

SpatialSolveResult solveSpatial(SpatialSketch candidate, const SpatialSketch *previous = nullptr,
                                const PlaneResolver &resolve = {});

QCborMap evaluateSpatial(const SpatialSketch &candidate, const SpatialSketch *previous = nullptr,
                         const PlaneResolver &resolve = {});

} // namespace cad::sketch

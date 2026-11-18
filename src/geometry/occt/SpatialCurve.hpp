#pragma once
#include "SupportReferences.hpp"
#include "model/DerivedCurve.hpp"
#include "sketch/SpatialSketch.hpp"

namespace cad::geometry {

SupportCurve spatialEntityCurve(const sketch::SpatialSketch &s, const sketch::SpatialEntity &e);
std::vector<SupportCurve> evaluateSpatialSketch(const sketch::SpatialSketch &s);

std::vector<SupportCurve> evaluateSketchOnSketch(const model::DerivedCurve &c,
                                                 const SupportReferences &scene);
std::vector<SupportCurve> evaluateSketchOnFaces(const model::DerivedCurve &c,
                                                const SupportReferences &scene);
std::vector<SupportCurve> evaluateCompositeCurve(const model::DerivedCurve &c,
                                                 const SupportReferences &scene);
std::vector<SupportCurve> evaluateHelixSpiral(const model::DerivedCurve &c,
                                              const SupportReferences &scene);

std::vector<SupportCurve> evaluateDerivedCurve(const model::DerivedCurve &c,
                                               const SupportReferences &scene);

QCborArray tessellateSupportCurve(const SupportCurve &c, int samples = 64);

} // namespace cad::geometry

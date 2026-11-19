#pragma once
#include "Supports.hpp"
namespace cad::model {
SupportDefinition editableSupports(const QCborMap &document);
SupportDefinition editSupportedFeatures(const QCborMap &document, QCborArray features);
SupportDefinition editSupportedSketches(const QCborMap &document, QCborArray sketches);
SupportDefinition editSupportedSpatialSketches(const QCborMap &document,
                                               QCborArray spatialSketches);
SupportDefinition editSupportedDerivedCurves(const QCborMap &document, QCborArray derivedCurves);
bool convertedSketch(const QCborMap &document, QString sketch);
} // namespace cad::model

#pragma once
#include "Supports.hpp"
namespace cad::model {
SupportDefinition editableSupports(const QCborMap &document);
SupportDefinition editSupportedFeatures(const QCborMap &document, QCborArray features);
SupportDefinition editSupportedSketches(const QCborMap &document, QCborArray sketches);
bool convertedSketch(const QCborMap &document, QString sketch);
} // namespace cad::model

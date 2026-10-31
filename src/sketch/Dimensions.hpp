#pragma once
#include "Sketch.hpp"
namespace cad::sketch {
bool dimensional(QString kind);
bool componentDimension(QString kind);
bool horizontalDimension(QString kind);
bool associatedDimension(QString kind);
double axisDistance(const Sketch &sketch, const Constraint &dimension, bool end = false);
double measureDimension(const Sketch &sketch, const Constraint &dimension);
void validateDimension(const Sketch &sketch, const Constraint &dimension);
} // namespace cad::sketch

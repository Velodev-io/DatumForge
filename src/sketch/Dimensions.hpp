#pragma once
#include "Sketch.hpp"
namespace cad::sketch {
bool dimensional(QString kind);
bool angularDimension(QString kind);
bool componentDimension(QString kind);
bool horizontalDimension(QString kind);
bool associatedDimension(QString kind);
double axisDistance(const Sketch &sketch, const Constraint &dimension, bool end = false);
double measureDimension(const Sketch &sketch, const Constraint &dimension);
void validateDimension(const Sketch &sketch, const Constraint &dimension, bool connectivity = true);
Vec2 pathEndpoint(const Sketch &sketch, const Constraint &dimension, int member, bool end);
double curveLength(const Sketch &sketch, const Entity &curve);
} // namespace cad::sketch

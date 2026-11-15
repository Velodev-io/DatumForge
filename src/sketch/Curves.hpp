#pragma once
#include "Sketch.hpp"
#include <utility>
namespace cad::sketch {
bool advancedCurve(QString kind);
bool ellipseCurve(QString kind);
bool radiusCurve(QString kind);
Vec2 curveValue(const Sketch &s, const Entity &e, double parameter);
Vec2 curveDerivative(const Sketch &s, const Entity &e, double parameter);
std::pair<double, double> curveRange(const Entity &e);
void validateCurve(const Sketch &s, const Entity &e);
QString createEllipse(Sketch &s, Vec2 center, Vec2 major, Vec2 minor, bool arc = false,
                      double start = 0, double sweep = 1.5707963267948966,
                      bool construction = false);
QString createParabola(Sketch &s, Vec2 focus, Vec2 vertex, Vec2 first, Vec2 last,
                       bool construction = false);
QString createSpline(Sketch &s, const std::vector<Vec2> &controls, int degree = 3,
                     bool closed = false, bool construction = false);
QStringList controlPoints(const Entity &e);
Vec2 location(const Sketch &s, const Entity &e);
int curveInputCount(QString kind);
QString createCurveInput(Sketch &s, QString kind, const std::vector<Vec2> &input, int degree = 3,
                         bool closed = false, bool construction = false);
} // namespace cad::sketch

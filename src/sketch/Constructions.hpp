#pragma once
#include "Sketch.hpp"
#include <QSet>
namespace cad::sketch {
struct RectangleParameters {
    Vec2 anchor;
    double width = 0, side = 0, rotation = 0, sideAngle = 0;
};
QStringList rectangleMethods();
int rectangleInputCount(QString method);
std::array<Vec2, 4> rectangleVertices(QString method, const std::vector<Vec2> &inputs);
const ConstructionGroup *constructionOf(const Sketch &sketch, QString member);
QStringList constructionMembers(const ConstructionGroup &group);
QString createRectangle(Sketch &sketch, QString method, const std::vector<Vec2> &inputs,
                        bool construction = false);
void validateConstructions(const Sketch &sketch, QSet<QString> &identities);
RectangleParameters rectangleParameters(const Sketch &sketch, const ConstructionGroup &group);
void resizeRectangle(Sketch &sketch, const ConstructionGroup &group, RectangleParameters values);
Sketch dragConstruction(const Sketch &sketch, const ConstructionGroup &group, QString member,
                        Vec2 delta);
Sketch eraseConstruction(const Sketch &sketch, const ConstructionGroup &group);
void checkConstructionEdit(const Sketch &before, const Sketch &after,
                           const ConstructionGroup &group);
} // namespace cad::sketch

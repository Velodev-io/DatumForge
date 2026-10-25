#pragma once
#include "ArcGeometry.hpp"
#include "Constructions.hpp"
namespace cad::sketch {
struct SlotParameters {
    Vec2 anchor;
    double length = 0, width = 0, rotation = 0, radius = 0, sweep = 0;
};
QStringList slotMethods();
bool curvedSlot(QString method);
int slotInputCount(QString method);
SlotParameters slotFromInput(QString method, const std::vector<Vec2> &input);
QString createSlot(Sketch &s, QString method, const std::vector<Vec2> &input,
                   QString lengthMode = "Center to Center", bool dimensions = false,
                   bool construction = false);
SlotParameters slotParameters(const Sketch &s, const ConstructionGroup &g);
void resizeSlot(Sketch &s, const ConstructionGroup &g, SlotParameters p);
Sketch dragSlot(const Sketch &s, const ConstructionGroup &g, Vec2 delta);
void validateSlot(const Sketch &s, const ConstructionGroup &g, QSet<QString> &owned,
                  QSet<QString> &ownedRelations);
void slotDimensions(Sketch &s, const ConstructionGroup &g);
} // namespace cad::sketch

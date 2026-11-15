#pragma once
#include "Sketch.hpp"
#include <optional>
namespace cad::sketch {
struct Rectangle {
    std::array<QString, 4> points, lines;
};
std::optional<Rectangle> rectangleOf(const Sketch &sketch, QString entity);
Sketch dragEntity(const Sketch &seed, QString entity, Vec2 from, Vec2 to);
Sketch eraseEntity(const Sketch &seed, QString entity);
} // namespace cad::sketch

#pragma once
#include "Sketch.hpp"
#include "evaluation/Graph.hpp"
namespace cad::sketch {
std::vector<evaluation::GraphNode> dependencyNodes(const std::vector<Sketch> &sketches);
std::vector<size_t> dependencyOrder(const std::vector<Sketch> &sketches);
} // namespace cad::sketch

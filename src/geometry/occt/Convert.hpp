#pragma once
#include "SupportReferences.hpp"
#include "model/Supports.hpp"
namespace cad::geometry {
// Reconstruct exact projected geometry from base inputs; never append a prior preview.
sketch::Sketch convert(const model::Conversion &definition, const sketch::Frame &target,
                       const SupportReferences &references);
} // namespace cad::geometry

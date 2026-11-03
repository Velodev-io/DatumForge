#pragma once
#include "Supports.hpp"
namespace cad::geometry {
SupportResult evaluateModeling(const model::SupportDefinition &definition,
                               const QCborArray &intents, const SupportBuild &build = {},
                               const exchange::Control &control = {});
} // namespace cad::geometry

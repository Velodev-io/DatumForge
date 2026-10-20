#pragma once
#include <cstddef>
namespace limits {
constexpr int version = 1;
constexpr std::size_t control = 1U << 20;
constexpr std::size_t segment = 32U << 20;
constexpr std::size_t transfer = 256U << 20;
constexpr std::size_t text = 16U << 10;
constexpr std::size_t collection = 100000;
constexpr int nesting = 32;
constexpr int outstanding = 32;
constexpr std::size_t object = 128U << 20;
constexpr std::size_t exchange = 100U << 20;
constexpr std::size_t project = 2ULL << 30;
// Prototype budgets; no measured acceptance claim until 0.5e/Module 11.
constexpr std::size_t cache = 256U << 20;
constexpr std::size_t history_memory = 64U << 20;
constexpr std::size_t history_disk = 512U << 20;
constexpr int heavy_jobs = 1;
} // namespace limits

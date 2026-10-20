#pragma once
#include "limits.hpp"
#include <array>
#include <stdexcept>
namespace admission {
enum class Pressure { Normal, Elevated, Critical };
enum class Response { Run, StopPreviewsAndEvictDerived, PostponeHeavyJob };
inline Response decide(std::size_t estimated, std::size_t available, int active,
                       Pressure pressure) {
    if (active < 0 || estimated > limits::project)
        throw std::runtime_error("Invalid admission estimate");
    constexpr std::size_t reserve = 512U << 20;
    if (active >= limits::heavy_jobs || pressure == Pressure::Critical || available < reserve ||
        estimated > available - reserve)
        return Response::PostponeHeavyJob;
    if (pressure == Pressure::Elevated)
        return Response::StopPreviewsAndEvictDerived;
    return Response::Run;
}
struct CacheEntry {
    bool authoritative;
    bool retainedRoot;
    std::size_t bytes;
};
template <std::size_t N> std::size_t trim(std::array<CacheEntry, N> &entries) {
    std::size_t removed = 0;
    for (auto &entry : entries)
        if (!entry.authoritative && !entry.retainedRoot) {
            removed += entry.bytes;
            entry.bytes = 0;
        }
    return removed;
}
} // namespace admission

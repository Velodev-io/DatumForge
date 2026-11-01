#pragma once
#include "Documents.hpp"
#include <optional>
namespace cad::domain {
// Semantic identity only. Exact topology/branch resolution belongs to the worker.
struct GeometryReference {
    QString document = "Self", object, role = "Object", entity, topology, endpoint;
    std::optional<double> parameter;
    void validate() const;
    QCborMap map() const;
    static GeometryReference parse(const QCborMap &map);
};
} // namespace cad::domain

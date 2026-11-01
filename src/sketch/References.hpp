#pragma once
#include "Dependencies.hpp"
namespace cad::sketch {
struct ReferenceFailure : domain::Error {
    QCborArray candidates;
    ReferenceFailure(QString code, QString message, QCborArray choices = {})
        : Error(code, message), candidates(std::move(choices)) {}
};
void resolvePierce(Sketch &target, Constraint &constraint, const Sketch &source);
} // namespace cad::sketch

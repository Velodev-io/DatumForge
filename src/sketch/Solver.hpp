#pragma once
#include "Sketch.hpp"
namespace cad::sketch {
struct Result {
    Sketch sketch;
    QString status = "FailedSolve", message;
    QStringList conflicting, redundant;
    int freedom = -1;
    double maximumResidual = 0, lengthScale = 1;
    bool valid = false;
    QCborMap diagnostic() const;
};
Result solve(Sketch candidate, const Sketch *previous = nullptr);
QCborArray validateAccepted(const QCborArray &sketches);
QCborMap evaluate(const QCborArray &candidates, const QCborArray &previous = {},
                  bool rejectInvalid = true);
} // namespace cad::sketch

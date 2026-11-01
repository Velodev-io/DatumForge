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
    QCborArray referenceCandidates;
    QCborMap diagnostic() const;
};
Result solve(Sketch candidate, const Sketch *previous = nullptr);
QCborArray validateAccepted(const QCborArray &sketches);
QCborMap interact(const QCborArray &candidates, const QCborMap &request,
                  const QCborArray &previous = {});
QCborMap evaluate(const QCborArray &candidates, const QCborArray &previous = {},
                  bool rejectInvalid = true);
} // namespace cad::sketch

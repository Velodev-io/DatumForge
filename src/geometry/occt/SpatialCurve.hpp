#pragma once
#include "SupportReferences.hpp"
#include "model/DerivedCurve.hpp"
#include "sketch/SpatialSketch.hpp"

namespace cad::geometry {

struct BranchCandidate {
    QString token;
    Handle(Geom_Curve) curve;
    double first = 0, last = 1;
    gp_Pnt startPt;
    gp_Pnt endPt;
};

SupportCurve spatialEntityCurve(const sketch::SpatialSketch &s, const sketch::SpatialEntity &e);
std::vector<SupportCurve> evaluateSpatialSketch(const sketch::SpatialSketch &s);

std::vector<BranchCandidate> enumerateSketchOnSketchBranches(const model::DerivedCurve &c,
                                                             const SupportReferences &scene);
std::vector<BranchCandidate> enumerateSketchOnFacesBranches(const model::DerivedCurve &c,
                                                            const SupportReferences &scene);
QCborMap inspectBranchCandidates(const model::DerivedCurve &c, const SupportReferences &scene);

std::vector<SupportCurve> evaluateSketchOnSketch(const model::DerivedCurve &c,
                                                 const SupportReferences &scene);
std::vector<SupportCurve> evaluateSketchOnFaces(const model::DerivedCurve &c,
                                                const SupportReferences &scene);
std::vector<SupportCurve> evaluateCompositeCurve(const model::DerivedCurve &c,
                                                 const SupportReferences &scene);
std::vector<SupportCurve> evaluateHelixSpiral(const model::DerivedCurve &c,
                                              const SupportReferences &scene);

std::vector<SupportCurve> evaluateDerivedCurve(const model::DerivedCurve &c,
                                               const SupportReferences &scene);

QCborArray tessellateSupportCurve(const SupportCurve &c, int samples = 64);

} // namespace cad::geometry

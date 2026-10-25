#pragma once
// Worker-only PlaneGCS adapter. Exact Value/derivative evaluation comes from
// the pinned curve classes; the project owns parameter/contact semantics.
#include <GCS.h>
#include <algorithm>
#include <memory>
namespace cad::sketch {
class CurveCoordinate final : public GCS::Constraint {
    std::unique_ptr<GCS::Curve> curve;
    double lower, upper;
    int coordinate;
    void reconstruct() {
        if (pvecChangedFlag) {
            int offset = 2;
            curve->ReconstructOnNewPvec(pvec, offset);
            pvecChangedFlag = false;
        }
    }

  public:
    CurveCoordinate(GCS::Curve &source, GCS::Point point, double *parameter, int axis, double first,
                    double last, int identity)
        : curve(source.Copy()), lower(first), upper(last), coordinate(axis) {
        pvec = {axis ? point.y : point.x, parameter};
        curve->PushOwnParams(pvec);
        origpvec = pvec;
        setTag(identity);
    }
    GCS::ConstraintType getTypeId() override {
        return GCS::None;
    }
    double error() override {
        reconstruct();
        auto p = curve->Value(*pvec[1], 0);
        return scale * (*pvec[0] - (coordinate ? p.y : p.x));
    }
    double grad(double *parameter) override {
        if (findParamInPvec(parameter) < 0)
            return 0;
        reconstruct();
        auto value = curve->Value(*pvec[1], parameter == pvec[1] ? 1 : 0, parameter);
        return scale * ((parameter == pvec[0] ? 1 : 0) - (coordinate ? value.dy : value.dx));
    }
    double maxStep(GCS::MAP_pD_D &direction, double limit = 1.) override {
        auto i = direction.find(pvec[1]);
        if (i == direction.end() || i->second == 0)
            return limit;
        double step = (i->second > 0 ? upper - *pvec[1] : lower - *pvec[1]) / i->second;
        return std::max(0., std::min(limit, step));
    }
};
} // namespace cad::sketch

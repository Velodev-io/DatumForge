#pragma once
// Worker-only exact product constraint, with derivatives independent of the
// model's residual/rank calculation. Angle pointers may be end/start angles
// for joined arcs, or sweep/zero for free arcs.
#include <GCS.h>
namespace cad::sketch {
class ArcLengthConstraint final : public GCS::Constraint {
  public:
    ArcLengthConstraint(double *radius, double *start, double *end, double *length, int tag) {
        pvec = {radius, start, end, length};
        origpvec = pvec;
        setTag(tag);
    }
    GCS::ConstraintType getTypeId() override {
        return GCS::None;
    }
    double error() override {
        return scale * (*pvec[0] * (*pvec[2] - *pvec[1]) - *pvec[3]);
    }
    double grad(double *parameter) override {
        double value = 0;
        if (parameter == pvec[0])
            value += *pvec[2] - *pvec[1];
        if (parameter == pvec[1])
            value -= *pvec[0];
        if (parameter == pvec[2])
            value += *pvec[0];
        if (parameter == pvec[3])
            value -= 1;
        return scale * value;
    }
};
} // namespace cad::sketch

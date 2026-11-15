#pragma once
// Worker-only oriented point/line distance. Unlike an unsigned distance plus a
// branch check, this equation follows an axis that moves across the old seed.
#include <GCS.h>
#include <cmath>
namespace cad::sketch {
class AxisDistanceConstraint final : public GCS::Constraint {
  public:
    AxisDistanceConstraint(GCS::Point point, GCS::Line axis, double *target, int tag) {
        pvec = {point.x, point.y, axis.p1.x, axis.p1.y, axis.p2.x, axis.p2.y, target};
        origpvec = pvec;
        setTag(tag);
    }
    GCS::ConstraintType getTypeId() override {
        return GCS::None;
    }
    double error() override {
        double dx = *pvec[4] - *pvec[2], dy = *pvec[5] - *pvec[3];
        double qx = *pvec[0] - *pvec[2], qy = *pvec[1] - *pvec[3];
        return scale * ((dx * qy - dy * qx) / std::hypot(dx, dy) - *pvec[6]);
    }
    double grad(double *parameter) override {
        double dx = *pvec[4] - *pvec[2], dy = *pvec[5] - *pvec[3];
        double qx = *pvec[0] - *pvec[2], qy = *pvec[1] - *pvec[3];
        double norm = std::hypot(dx, dy), area = dx * qy - dy * qx;
        double dn[] = {-dy, dx, *pvec[5] - *pvec[1], *pvec[0] - *pvec[4], qy, -qx};
        double dl[] = {0, 0, -dx / norm, -dy / norm, dx / norm, dy / norm};
        double value = 0;
        // Sum derivatives when PlaneGCS aliases parameters during reduction.
        for (int i = 0; i < 6; ++i)
            if (parameter == pvec[i])
                value += dn[i] / norm - area * dl[i] / (norm * norm);
        if (parameter == pvec[6])
            value -= 1;
        return scale * value;
    }
};
} // namespace cad::sketch

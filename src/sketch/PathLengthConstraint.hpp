#pragma once
#include "Curves.hpp"
#include "Dimensions.hpp"
#include <GCS.h>
#include <cmath>
namespace cad::sketch {
class PathLengthConstraint final : public GCS::Constraint {
  public:
    PathLengthConstraint(const std::vector<GCS::Line> &lines, double *target, int tag) {
        for (auto &line : lines)
            for (auto point : {line.p1, line.p2}) {
                pvec.push_back(point.x);
                pvec.push_back(point.y);
            }
        pvec.push_back(target);
        origpvec = pvec;
        setTag(tag);
    }
    GCS::ConstraintType getTypeId() override {
        return GCS::None;
    }
    double error() override {
        double total = 0;
        for (size_t i = 0; i + 4 < pvec.size(); i += 4)
            total += std::hypot(*pvec[i + 2] - *pvec[i], *pvec[i + 3] - *pvec[i + 1]);
        return scale * (total - *pvec.back());
    }
    double grad(double *parameter) override {
        double result = parameter == pvec.back() ? -1 : 0;
        for (size_t i = 0; i + 4 < pvec.size(); i += 4) {
            double dx = *pvec[i + 2] - *pvec[i], dy = *pvec[i + 3] - *pvec[i + 1];
            double norm = std::max(1e-15, std::hypot(dx, dy));
            if (parameter == pvec[i])
                result -= dx / norm;
            if (parameter == pvec[i + 1])
                result -= dy / norm;
            if (parameter == pvec[i + 2])
                result += dx / norm;
            if (parameter == pvec[i + 3])
                result += dy / norm;
        }
        return scale * result;
    }
};
// Joined curves use a bounded numerical derivative of the exact semantic
// equation. Reconstruct from PlaneGCS's current pvec so aliased/reduced
// parameters are differentiated together, without mutating solver storage.
class CurvePathConstraint final : public GCS::Constraint {
    Sketch source;
    cad::sketch::Constraint dimension;
    QSet<QString> joined;
    struct Binding {
        QString id;
        int field;
    };
    std::vector<Binding> bindings;
    int junction, coordinate;
    double evaluate(double *parameter = nullptr, double offset = 0) const {
        auto candidate = source;
        for (size_t i = 0; i < bindings.size(); ++i) {
            auto &e = candidate.entity(bindings[i].id);
            double value = *pvec[i] + (parameter == pvec[i] ? offset : 0);
            switch (bindings[i].field) {
            case 0:
                e.point.x = value;
                break;
            case 1:
                e.point.y = value;
                break;
            case 2:
                e.radius = value;
                break;
            case 3:
                e.start = value;
                break;
            case 4:
                e.sweep = value;
                break;
            }
        }
        for (auto &e : candidate.entities)
            if (joined.contains(e.id))
                e.sweep -= e.start;
        if (junction >= 0) {
            auto a = pathEndpoint(candidate, dimension, junction, true);
            auto b = pathEndpoint(candidate, dimension, (junction + 1) % dimension.members.size(),
                                  false);
            return coordinate ? a.y - b.y : a.x - b.x;
        }
        return measureDimension(candidate, dimension) - *pvec.back() -
               (parameter == pvec.back() ? offset : 0);
    }

  public:
    CurvePathConstraint(Sketch &candidate, const cad::sketch::Constraint &definition,
                        QSet<QString> joinedArcs, double *target, int tag, int join = -1,
                        int axis = 0)
        : source(candidate), dimension(definition), joined(std::move(joinedArcs)), junction(join),
          coordinate(axis) {
        auto add = [&](Entity &e, int field, double *value) {
            bindings.push_back({e.id, field});
            pvec.push_back(value);
        };
        for (auto &e : candidate.entities) {
            if (e.kind == "Point") {
                add(e, 0, &e.point.x);
                add(e, 1, &e.point.y);
            }
            if (radiusCurve(e.kind))
                add(e, 2, &e.radius);
            if (e.kind == "Arc" || e.kind == "EllipticalArc" || e.kind == "Parabola") {
                add(e, 3, &e.start);
                add(e, 4, &e.sweep);
            }
        }
        pvec.push_back(target);
        origpvec = pvec;
        setTag(tag);
    }
    GCS::ConstraintType getTypeId() override {
        return GCS::None;
    }
    double error() override {
        return scale * evaluate();
    }
    double grad(double *parameter) override {
        if (findParamInPvec(parameter) < 0)
            return 0;
        double step = 1e-6 * std::max(1., std::abs(*parameter));
        return scale * (evaluate(parameter, step) - evaluate(parameter, -step)) / (2 * step);
    }
};
} // namespace cad::sketch

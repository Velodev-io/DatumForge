#include "Model.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <numbers>
namespace cad::assembly {
using V = Eigen::Vector3d;
using R = Eigen::Matrix3d;
using domain::check;
using sketch::fields;
namespace {
V vector(Vec p) {
    return {p[0], p[1], p[2]};
}
Vec vec(V p) {
    return {p.x(), p.y(), p.z()};
}
R rotation(const Pose &p) {
    return Eigen::Quaterniond(p.rotation[0], p.rotation[1], p.rotation[2], p.rotation[3])
        .toRotationMatrix();
}
Pose pose(V t, R r) {
    auto q = Eigen::Quaterniond(r).normalized();
    Pose p;
    p.translation = vec(t);
    p.rotation = {q.w(), q.x(), q.y(), q.z()};
    return Pose::parse(p.map());
}
R exp(V v) {
    return v.norm() < 1e-14 ? R(R::Identity()) : R(Eigen::AngleAxisd(v.norm(), v.normalized()));
}
constexpr double scale = .1;
} // namespace
Vec transform(const Pose &p, Vec v, bool direction) {
    return vec(rotation(p) * vector(v) + (direction ? V::Zero().eval() : vector(p.translation)));
}
Pose compose(const Pose &p, const Pose &c) {
    return pose(vector(transform(p, c.translation)), rotation(p) * rotation(c));
}
Solution solve(Model model, const FrameResolver &resolve) {
    Solution result;
    result.model = model;
    QMap<QString, int> index;
    std::vector<int> moving;
    for (size_t i = 0; i < model.occurrences.size(); ++i) {
        index[model.occurrences[i].id] = int(i);
        if (!model.occurrences[i].grounded && !model.occurrences[i].suppressed)
            moving.push_back(int(i));
    }
    struct Constraint {
        Mate mate;
        Frame a, b;
        int first, second;
    };
    std::vector<Constraint> constraints;
    for (auto &mate : model.mates) {
        if (mate.suppressed)
            continue;
        try {
            check(index.contains(mate.first.occurrence) && index.contains(mate.second.occurrence),
                  "ReferenceMissing", "Mate occurrence is missing");
            int a = index[mate.first.occurrence], b = index[mate.second.occurrence];
            if (model.occurrences[a].suppressed || model.occurrences[b].suppressed)
                continue;
            auto x = resolve(mate.first), y = resolve(mate.second);
            bool bothDirected = x.kind != "Point" && y.kind != "Point";
            check((mate.kind == "Distance" && ((x.kind == "Point" && y.kind == "Point") ||
                                               (x.kind == "Plane" && y.kind == "Plane"))) ||
                      (mate.kind == "Coincident" && x.kind == y.kind && x.kind != "Axis") ||
                      (mate.kind == "Concentric" && x.kind == "Axis" && y.kind == "Axis") ||
                      ((mate.kind == "Parallel" || mate.kind == "Perpendicular" ||
                        mate.kind == "Angle") &&
                       bothDirected),
                  "UnsupportedOperation", "Unsupported planar/axial mate reference combination");
            constraints.push_back({mate, x, y, a, b});
        } catch (const domain::Error &e) {
            result.diagnostics.append(fields(
                {{"mate", mate.id}, {"code", e.code}, {"message", QString::fromUtf8(e.what())}}));
        }
    }
    if (!result.diagnostics.empty()) {
        result.freedom = int(moving.size()) * 6;
        return result;
    }
    auto residual = [&](const Model &m) {
        std::vector<double> values;
        for (auto &c : constraints) {
            auto &a = m.occurrences[c.first].pose, &b = m.occurrences[c.second].pose;
            V p = vector(transform(a, c.a.origin)), q = vector(transform(b, c.b.origin)),
              n = vector(transform(a, c.a.direction, true)),
              d = vector(transform(b, c.b.direction, true));
            V delta = (q - p) / scale;
            auto add = [&](V v) {
                for (double x : v)
                    values.push_back(x);
            };
            auto parallel = [&] { add(n - (c.mate.alignment == "Aligned" ? d : -d)); };
            if (c.mate.kind == "Coincident") {
                if (c.a.kind == "Point")
                    add(delta);
                else {
                    parallel();
                    values.push_back(n.dot(delta));
                }
            } else if (c.mate.kind == "Concentric") {
                parallel();
                add(delta - n * delta.dot(n));
            } else if (c.mate.kind == "Parallel")
                parallel();
            else if (c.mate.kind == "Perpendicular")
                values.push_back(n.dot(d));
            else if (c.mate.kind == "Angle") {
                if (c.mate.value < 1e-10)
                    add(n - d);
                else if (std::abs(c.mate.value - std::numbers::pi) < 1e-10)
                    add(n + d);
                else
                    values.push_back(std::atan2(n.cross(d).norm(), n.dot(d)) - c.mate.value);
            } else if (c.a.kind == "Point") {
                check(c.mate.value >= 0, "InvalidInput", "Point distance must be nonnegative");
                if (c.mate.value < 1e-10)
                    add(delta);
                else
                    values.push_back(delta.norm() - c.mate.value / scale);
            } else
                values.push_back(n.dot(delta) - c.mate.value / scale);
        }
        Eigen::VectorXd v(values.size());
        for (size_t i = 0; i < values.size(); ++i)
            v[i] = values[i];
        return v;
    };
    auto move = [&](Model m, const Eigen::VectorXd &delta) {
        for (size_t i = 0; i < moving.size(); ++i) {
            auto &p = m.occurrences[moving[i]].pose;
            p = pose(vector(p.translation) + scale * delta.segment<3>(i * 6),
                     exp(delta.segment<3>(i * 6 + 3)) * rotation(p));
        }
        return m;
    };
    auto jacobian = [&](const Model &m) {
        Eigen::MatrixXd j(residual(m).size(), moving.size() * 6);
        for (int i = 0; i < j.cols(); ++i) {
            auto delta = Eigen::VectorXd::Zero(j.cols()).eval();
            delta[i] = 1e-6;
            j.col(i) = (residual(move(m, delta)) - residual(move(m, -delta))) / (2e-6);
        }
        return j;
    };
    if (constraints.empty()) {
        result.valid = true;
        result.freedom = int(moving.size()) * 6;
        return result;
    }
    Model candidate = model;
    for (auto &c : constraints)
        if (c.mate.kind == "Distance" && c.a.kind == "Point" && c.mate.value > 1e-10 &&
            (vector(transform(candidate.occurrences[c.first].pose, c.a.origin)) -
             vector(transform(candidate.occurrences[c.second].pose, c.b.origin)))
                    .norm() < 1e-10) {
            if (!candidate.occurrences[c.second].grounded)
                candidate.occurrences[c.second].pose.translation[0] += .001;
            else if (!candidate.occurrences[c.first].grounded)
                candidate.occurrences[c.first].pose.translation[0] += .001;
        }

    // Deterministic temporary angular seed escapes parallel/antiparallel stationary branches.
    for (auto &c : constraints) {
        if (c.a.kind == "Point" || c.b.kind == "Point")
            continue;
        auto &b = candidate.occurrences[c.second];
        if (b.grounded)
            continue;
        auto n = vector(transform(candidate.occurrences[c.first].pose, c.a.direction, true)),
             d = vector(transform(b.pose, c.b.direction, true));
        if (n.cross(d).norm() < 1e-8 && residual(candidate).norm() > 1e-8)
            b.pose = pose(vector(b.pose.translation), exp(V(.013, .021, .034)) * rotation(b.pose));
    }
    for (int iteration = 0; iteration < 80; ++iteration) {
        auto values = residual(candidate);
        check(values.allFinite(), "SolveFailure", "Nonfinite mate residual");
        result.residual = values.norm();
        if (moving.empty()) {
            result.valid = result.residual < 1e-8;
            break;
        }
        auto j = jacobian(candidate);
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(j, Eigen::ComputeThinU | Eigen::ComputeThinV);
        svd.setThreshold(1e-7);
        result.rank = int(svd.rank());
        result.freedom = int(moving.size()) * 6 - result.rank;
        if (result.residual < 1e-8) {
            result.valid = true;
            break;
        }
        auto step = svd.solve(-values).eval();
        check(step.allFinite(), "SolveFailure", "Nonfinite mate step");
        if (step.norm() > 1)
            step /= step.norm();
        bool improved = false;
        for (double damping : {1., .5, .25, .125, .0625}) {
            auto next = move(candidate, step * damping);
            if (residual(next).norm() < result.residual - 1e-13) {
                candidate = next;
                improved = true;
                break;
            }
        }
        if (!improved)
            break;
    }
    if (result.valid)
        result.model = candidate;
    else
        for (auto &c : constraints)
            result.diagnostics.append(
                fields({{"mate", c.mate.id},
                        {"code", "ConstraintConflict"},
                        {"message", "Mate system did not converge; accepted transforms preserved "
                                    "(reported set is not a minimal conflict set)"}}));
    return result;
}
} // namespace cad::assembly

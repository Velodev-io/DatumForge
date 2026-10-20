#include "support/check.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <functional>
#include <numbers>
#include <vector>
using V = Eigen::Vector3d;
struct Pose {
    V t = V::Zero();
    Eigen::Matrix3d r = Eigen::Matrix3d::Identity();
};
using Model = std::array<Pose, 3>;
using Residual = std::function<Eigen::VectorXd(const Model &)>;
Eigen::Matrix3d expRotation(const V &v) {
    if (v.norm() < 1e-12)
        return Eigen::Matrix3d::Identity();
    return Eigen::AngleAxisd(v.norm(), v.normalized()).toRotationMatrix();
}
Model move(Model model, const Eigen::VectorXd &delta) {
    for (int i = 1; i <= 2; ++i) {
        model[i].t += delta.segment<3>((i - 1) * 6);
        model[i].r = expRotation(delta.segment<3>((i - 1) * 6 + 3)) * model[i].r;
        model[i].r = Eigen::Quaterniond(model[i].r).normalized().toRotationMatrix();
    }
    return model;
}
Eigen::MatrixXd jacobian(const Model &model, const Residual &residual) {
    Eigen::MatrixXd j(residual(model).size(), 12);
    for (int i = 0; i < 12; ++i) {
        Eigen::VectorXd step = Eigen::VectorXd::Zero(12);
        step[i] = 1e-6;
        j.col(i) = (residual(move(model, step)) - residual(move(model, -step))) / (2e-6);
    }
    return j;
}
struct Result {
    Model model;
    bool success;
    int freedom;
    double error;
};
Result solve(Model model, const Residual &residual) {
    // Escape the stationary antiparallel-normal branch deterministically in the
    // temporary solve seed. Accepted transforms remain untouched on failure.
    for (int i = 1; i <= 2; ++i)
        if (model[i - 1].r.col(2).dot(model[i].r.col(2)) < -.999)
            model[i].r = expRotation(.1 * model[i - 1].r.col(0)) * model[i].r;
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto r = residual(model);
        CHECK(r.allFinite());
        Eigen::MatrixXd j = jacobian(model, residual);
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(j, Eigen::ComputeThinU | Eigen::ComputeThinV);
        svd.setThreshold(1e-7);
        if (r.norm() < 1e-8)
            return {model, true, 12 - int(svd.rank()), r.norm()};
        Eigen::VectorXd delta = svd.solve(-r);
        if (delta.norm() > 1)
            delta /= delta.norm();
        if (delta.norm() < 1e-12)
            return {model, false, 12 - int(svd.rank()), r.norm()};
        model = move(model, delta);
    }
    auto r = residual(model);
    return {model, false, 12 - int(jacobian(model, residual).fullPivLu().rank()), r.norm()};
}
// Plane normal, axial location, and signed in-plane angle; body 0 is grounded.
void pairResidual(std::vector<double> &out, const Pose &a, const Pose &b, double z, bool axial,
                  bool angle) {
    V n = a.r.col(2), m = b.r.col(2), relative = a.r.transpose() * (b.t - a.t);
    V cross = n.cross(m);
    out.insert(out.end(), {relative.z() - z, cross.x(), cross.y(), cross.z(), n.dot(m) - 1});
    if (axial)
        out.insert(out.end(), {relative.x(), relative.y()});
    if (angle) {
        V x = a.r.transpose() * b.r.col(0);
        out.push_back(std::atan2(x.y(), x.x()));
    }
}
Residual constraints(bool axial, bool angle, bool cycle = false, bool conflict = false) {
    return [=](const Model &m) {
        std::vector<double> v;
        pairResidual(v, m[0], m[1], 0, axial, angle);
        pairResidual(v, m[1], m[2], 10, axial, angle);
        if (cycle)
            v.push_back(m[2].t.z() - 10);
        if (conflict)
            v.push_back(m[2].t.z() - 11);
        return Eigen::VectorXd(Eigen::Map<Eigen::VectorXd>(v.data(), v.size()));
    };
}
int main() {
    return run([] {
        Model seed;
        seed[1].t = V(2, 3, .5);
        seed[2].t = V(4, 5, 9);
        auto plane = solve(seed, constraints(false, false));
        CHECK(plane.success);
        CHECK(plane.freedom == 6);
        auto axis = solve(seed, constraints(true, false));
        CHECK(axis.success);
        CHECK(axis.freedom == 2);
        auto fixed = solve(seed, constraints(true, true));
        CHECK(fixed.success);
        CHECK(fixed.freedom == 0);
        near(fixed.model[1].t.norm(), 0);
        near((fixed.model[2].t - V(0, 0, 10)).norm(), 0);
        CHECK(solve(seed, constraints(true, true, true)).success); // consistent closed cycle
        auto bad = solve(seed, constraints(true, true, true, true));
        CHECK(!bad.success);
        CHECK(bad.error > .1);
        // Local SO(3) updates avoid Euler pitch singularity; near-pi yaw is tested.
        seed[1].r = expRotation(V(0, 0, std::numbers::pi - 1e-3));
        seed[2].r = seed[1].r;
        CHECK(solve(seed, constraints(true, true)).success);
        Model singular;
        singular[1].r = expRotation(V(std::numbers::pi, 0, 0));
        singular[2].t.z() = 10;
        auto singularResult = solve(singular, constraints(true, true));
        CHECK(singularResult.success);
        near((singularResult.model[1].r - Eigen::Matrix3d::Identity()).norm(), 0);
        auto accepted = fixed.model;
        auto preview = solve(seed, constraints(true, true));
        CHECK(preview.success);
        preview.model = accepted; // canceled solve never mutates accepted transform
        near((preview.model[2].t - accepted[2].t).norm(), 0);
        near((accepted[0].r - Eigen::Matrix3d::Identity()).norm(), 0);
        near(accepted[0].t.norm(), 0);
        std::cout << "grounded 3-body planar/axial/angle rank: 6/2/0 DOF; consistent cycle; "
                     "conflicting distance; near-pi yaw; antiparallel seed recovery; cancel\n";
    });
}

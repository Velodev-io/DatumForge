#include "SpatialSolver.hpp"
#include <Eigen/Dense>
#include <Eigen/SVD>
#include <cmath>
#include <numbers>

namespace cad::sketch {

using domain::check;

QCborMap SpatialSolveResult::diagnostic() const {
    return fields({{"sketch_id", sketch.id},
                   {"status", status},
                   {"message", message},
                   {"valid", valid},
                   {"freedom", freedom},
                   {"rank", rank},
                   {"maximum_normalized_residual", maximumResidual},
                   {"conflicting", QCborArray::fromStringList(conflicting)},
                   {"redundant", QCborArray::fromStringList(redundant)}});
}

namespace {

sketch::Frame defaultResolvePlane(const domain::GeometryReference &ref,
                                  const PlaneResolver &customResolve) {
    if (customResolve) {
        try {
            return customResolve(ref);
        } catch (...) {
        }
    }
    QString obj = ref.object;
    if (obj.isEmpty())
        obj = ref.topology;
    if (obj == "PlaneXY" || obj == "XY" || obj.contains("XY") || obj.contains("Front"))
        return {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    if (obj == "PlaneYZ" || obj == "YZ" || obj.contains("YZ") || obj.contains("Right"))
        return {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    if (obj == "PlaneZX" || obj == "PlaneXZ" || obj == "ZX" || obj == "XZ" || obj.contains("Top"))
        return {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}};
    return {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
}

inline Eigen::Vector3d toEigen(const Vec3 &v) {
    return {v[0], v[1], v[2]};
}

inline Vec3 fromEigen(const Eigen::Vector3d &v) {
    return {v.x(), v.y(), v.z()};
}

struct ConstraintBlock {
    QString constraintId;
    int startRow = 0;
    int rowCount = 0;
};

double measureSpatialDimension(const SpatialSketch &s, const SpatialConstraint &c,
                               const PlaneResolver &resolve) {
    auto getPt = [&](const QString &id) -> Eigen::Vector3d {
        const auto &e = s.entity(id);
        return {e.point[0], e.point[1], e.point[2]};
    };
    if (c.kind == "Distance") {
        if (c.plane.has_value()) {
            sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
            Eigen::Vector3d O = toEigen(fr.origin);
            Eigen::Vector3d N = toEigen(fr.u).cross(toEigen(fr.v)).normalized();
            if (s.entity(c.a).kind == "Point") {
                return std::abs((getPt(c.a) - O).dot(N));
            } else {
                const auto &l = s.entity(c.a);
                return std::abs((getPt(l.a) - O).dot(N));
            }
        }
        const auto &ea = s.entity(c.a);
        const auto &eb = s.entity(c.b);
        if (ea.kind == "Point" && eb.kind == "Point") {
            return (getPt(c.a) - getPt(c.b)).norm();
        }
        if (ea.kind == "Point" && eb.kind == "Line") {
            Eigen::Vector3d p = getPt(c.a);
            Eigen::Vector3d p1 = getPt(eb.a);
            Eigen::Vector3d p2 = getPt(eb.b);
            Eigen::Vector3d dir = p2 - p1;
            double dn = dir.norm();
            if (dn < 1e-12)
                return (p - p1).norm();
            dir /= dn;
            Eigen::Vector3d w = p - p1;
            return (w - w.dot(dir) * dir).norm();
        }
        if (ea.kind == "Line" && eb.kind == "Point") {
            Eigen::Vector3d p = getPt(c.b);
            Eigen::Vector3d p1 = getPt(ea.a);
            Eigen::Vector3d p2 = getPt(ea.b);
            Eigen::Vector3d dir = p2 - p1;
            double dn = dir.norm();
            if (dn < 1e-12)
                return (p - p1).norm();
            dir /= dn;
            Eigen::Vector3d w = p - p1;
            return (w - w.dot(dir) * dir).norm();
        }
        if (ea.kind == "Line" && eb.kind == "Line") {
            Eigen::Vector3d p1 = getPt(ea.a);
            Eigen::Vector3d q1 = getPt(eb.a);
            Eigen::Vector3d q2 = getPt(eb.b);
            Eigen::Vector3d dir = q2 - q1;
            double dn = dir.norm();
            if (dn < 1e-12)
                return (p1 - q1).norm();
            dir /= dn;
            Eigen::Vector3d w = p1 - q1;
            return (w - w.dot(dir) * dir).norm();
        }
    } else if (c.kind == "Angle") {
        if (c.plane.has_value()) {
            sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
            Eigen::Vector3d N = toEigen(fr.u).cross(toEigen(fr.v)).normalized();
            const auto &l = s.entity(c.a);
            Eigen::Vector3d dir = getPt(l.b) - getPt(l.a);
            double dn = dir.norm();
            if (dn < 1e-12)
                return 0.0;
            double dotVal = std::clamp((dir / dn).dot(N), -1.0, 1.0);
            return std::abs(std::asin(dotVal));
        } else {
            const auto &l1 = s.entity(c.a);
            const auto &l2 = s.entity(c.b);
            Eigen::Vector3d d1 = getPt(l1.b) - getPt(l1.a);
            Eigen::Vector3d d2 = getPt(l2.b) - getPt(l2.a);
            double n1 = d1.norm(), n2 = d2.norm();
            if (n1 < 1e-12 || n2 < 1e-12)
                return 0.0;
            double cosTheta = std::clamp((d1 / n1).dot(d2 / n2), -1.0, 1.0);
            return std::acos(cosTheta);
        }
    }
    return c.value;
}

} // namespace

SpatialSolveResult solveSpatial(SpatialSketch candidate, const SpatialSketch *previous,
                                const PlaneResolver &resolve, const CancelChecker &canceled) {
    SpatialSolveResult result;
    result.sketch = candidate;
    try {
        candidate.validate();
    } catch (const domain::Error &e) {
        result.status = "FailedSolve";
        result.message = QString::fromUtf8(e.what());
        result.valid = false;
        if (previous)
            result.sketch = *previous;
        return result;
    }

    if (canceled && canceled()) {
        result.valid = false;
        result.status = "Canceled";
        result.message = "Spatial solve canceled";
        if (previous)
            result.sketch = *previous;
        return result;
    }

    std::vector<QString> pointIds;
    QMap<QString, int> pointIndex;
    for (size_t i = 0; i < candidate.entities.size(); ++i) {
        if (candidate.entities[i].kind == "Point") {
            pointIndex[candidate.entities[i].id] = int(pointIds.size());
            pointIds.push_back(candidate.entities[i].id);
        }
    }

    int nPoints = int(pointIds.size());
    int nVars = nPoints * 3;

    if (nVars == 0) {
        result.valid = true;
        result.status = "Solved";
        result.freedom = 0;
        result.rank = 0;
        result.maximumResidual = 0.0;
        return result;
    }

    double lengthScale = 1.0;
    {
        double maxCoord = 0.01;
        for (const auto &pid : pointIds) {
            const auto &p = candidate.entity(pid).point;
            maxCoord = std::max({maxCoord, std::abs(p[0]), std::abs(p[1]), std::abs(p[2])});
        }
        lengthScale = std::clamp(maxCoord, 0.001, 1000.0);
    }

    Eigen::VectorXd X(nVars);
    for (int i = 0; i < nPoints; ++i) {
        const auto &pt = candidate.entity(pointIds[size_t(i)]).point;
        X[i * 3 + 0] = pt[0];
        X[i * 3 + 1] = pt[1];
        X[i * 3 + 2] = pt[2];
    }

    QMap<QString, Eigen::Vector3d> initialFixed;
    for (const auto &c : candidate.constraints) {
        if (c.kind == "Fixed") {
            if (pointIndex.contains(c.a)) {
                int idx = pointIndex[c.a];
                initialFixed[c.id] = {X[idx * 3 + 0], X[idx * 3 + 1], X[idx * 3 + 2]};
            }
        }
    }

    auto getPoint = [&](const Eigen::VectorXd &vars, const QString &id) -> Eigen::Vector3d {
        int idx = pointIndex.value(id, -1);
        if (idx < 0)
            return Eigen::Vector3d::Zero();
        return {vars[idx * 3 + 0], vars[idx * 3 + 1], vars[idx * 3 + 2]};
    };

    std::vector<ConstraintBlock> blocks;

    auto computeResiduals = [&](const Eigen::VectorXd &vars, bool recordBlocks) -> Eigen::VectorXd {
        if (recordBlocks)
            blocks.clear();
        std::vector<double> resList;

        auto addRes = [&](double val) { resList.push_back(val); };
        auto addVec3 = [&](const Eigen::Vector3d &v) {
            resList.push_back(v.x());
            resList.push_back(v.y());
            resList.push_back(v.z());
        };

        // Inherent Arc constraints
        for (const auto &e : candidate.entities) {
            if (e.kind == "Arc") {
                Eigen::Vector3d C = getPoint(vars, e.center);
                Eigen::Vector3d A = getPoint(vars, e.a);
                Eigen::Vector3d B = getPoint(vars, e.b);
                Eigen::Vector3d N = toEigen(e.normal).normalized();
                double R = e.radius;

                addRes(((A - C).norm() - R) / lengthScale);
                addRes(((B - C).norm() - R) / lengthScale);
                addRes(((A - C).dot(N)) / lengthScale);
                addRes(((B - C).dot(N)) / lengthScale);
            }
        }

        // Explicit Constraints
        for (const auto &c : candidate.constraints) {
            int startIdx = int(resList.size());
            if (c.kind == "Distance" || c.kind == "Angle") {
                if (!c.driving) {
                    if (recordBlocks) {
                        ConstraintBlock block;
                        block.constraintId = c.id;
                        block.startRow = startIdx;
                        block.rowCount = 0;
                        blocks.push_back(block);
                    }
                    continue;
                }
            }
            if (c.kind == "Fixed") {
                Eigen::Vector3d pt = getPoint(vars, c.a);
                Eigen::Vector3d target = initialFixed.value(c.id, pt);
                addVec3((pt - target) / lengthScale);
            } else if (c.kind == "Coincident") {
                if (pointIndex.contains(c.a) && pointIndex.contains(c.b)) {
                    Eigen::Vector3d pA = getPoint(vars, c.a);
                    Eigen::Vector3d pB = getPoint(vars, c.b);
                    addVec3((pA - pB) / lengthScale);
                } else {
                    QString ptId = pointIndex.contains(c.a) ? c.a : c.b;
                    QString otherId = pointIndex.contains(c.a) ? c.b : c.a;
                    if (pointIndex.contains(ptId)) {
                        const auto &otherEnt = candidate.entity(otherId);
                        Eigen::Vector3d pA = getPoint(vars, ptId);
                        if (otherEnt.kind == "Line") {
                            Eigen::Vector3d p1 = getPoint(vars, otherEnt.a);
                            Eigen::Vector3d p2 = getPoint(vars, otherEnt.b);
                            Eigen::Vector3d dir = p2 - p1;
                            double dirNorm = dir.norm();
                            if (dirNorm > 1e-12) {
                                Eigen::Vector3d dUnit = dir / dirNorm;
                                Eigen::Vector3d w = pA - p1;
                                Eigen::Vector3d perp = w - (w.dot(dUnit)) * dUnit;
                                addVec3(perp / lengthScale);
                            }
                        } else if (otherEnt.kind == "Arc") {
                            Eigen::Vector3d C = getPoint(vars, otherEnt.center);
                            Eigen::Vector3d N = toEigen(otherEnt.normal).normalized();
                            double R = otherEnt.radius;
                            addRes(((pA - C).norm() - R) / lengthScale);
                            addRes(((pA - C).dot(N)) / lengthScale);
                        }
                    }
                }
            } else if (c.kind == "Distance") {
                if (c.plane.has_value()) {
                    sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                    Eigen::Vector3d O = toEigen(fr.origin);
                    Eigen::Vector3d U = toEigen(fr.u).normalized();
                    Eigen::Vector3d V = toEigen(fr.v).normalized();
                    Eigen::Vector3d N = U.cross(V).normalized();

                    if (pointIndex.contains(c.a)) {
                        Eigen::Vector3d pt = getPoint(vars, c.a);
                        double dist = (pt - O).dot(N);
                        addRes((std::abs(dist) - c.value) / lengthScale);
                    } else {
                        const auto &lineEnt = candidate.entity(c.a);
                        Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                        Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                        Eigen::Vector3d dir = p2 - p1;
                        addRes((dir.dot(N)) / lengthScale);
                        addRes((std::abs((p1 - O).dot(N)) - c.value) / lengthScale);
                    }
                } else if (pointIndex.contains(c.a) && pointIndex.contains(c.b)) {
                    Eigen::Vector3d pA = getPoint(vars, c.a);
                    Eigen::Vector3d pB = getPoint(vars, c.b);
                    addRes(((pA - pB).norm() - c.value) / lengthScale);
                } else if (pointIndex.contains(c.a) || pointIndex.contains(c.b)) {
                    QString ptId = pointIndex.contains(c.a) ? c.a : c.b;
                    QString lineId = pointIndex.contains(c.a) ? c.b : c.a;
                    const auto &lineEnt = candidate.entity(lineId);
                    Eigen::Vector3d pA = getPoint(vars, ptId);
                    Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                    Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                    Eigen::Vector3d dir = p2 - p1;
                    double dirNorm = dir.norm();
                    if (dirNorm > 1e-12) {
                        Eigen::Vector3d dUnit = dir / dirNorm;
                        Eigen::Vector3d w = pA - p1;
                        double perpDist = (w - (w.dot(dUnit)) * dUnit).norm();
                        addRes((perpDist - c.value) / lengthScale);
                    }
                } else {
                    const auto &l1 = candidate.entity(c.a);
                    const auto &l2 = candidate.entity(c.b);
                    Eigen::Vector3d p1 = getPoint(vars, l1.a);
                    Eigen::Vector3d p2 = getPoint(vars, l1.b);
                    Eigen::Vector3d q1 = getPoint(vars, l2.a);
                    Eigen::Vector3d q2 = getPoint(vars, l2.b);
                    Eigen::Vector3d d1 = p2 - p1;
                    Eigen::Vector3d d2 = q2 - q1;
                    double n1 = d1.norm(), n2 = d2.norm();
                    if (n1 > 1e-12 && n2 > 1e-12) {
                        addVec3((d1 / n1).cross(d2 / n2));
                        Eigen::Vector3d mid = (p1 + p2) * 0.5;
                        Eigen::Vector3d dUnit2 = d2 / n2;
                        Eigen::Vector3d w = mid - q1;
                        double perpDist = (w - (w.dot(dUnit2)) * dUnit2).norm();
                        addRes((perpDist - c.value) / lengthScale);
                    }
                }
            } else if (c.kind == "OnPlane") {
                sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                Eigen::Vector3d O = toEigen(fr.origin);
                Eigen::Vector3d U = toEigen(fr.u).normalized();
                Eigen::Vector3d V = toEigen(fr.v).normalized();
                Eigen::Vector3d N = U.cross(V).normalized();

                if (pointIndex.contains(c.a)) {
                    Eigen::Vector3d pt = getPoint(vars, c.a);
                    addRes(((pt - O).dot(N)) / lengthScale);
                } else {
                    const auto &ent = candidate.entity(c.a);
                    if (ent.kind == "Line") {
                        Eigen::Vector3d p1 = getPoint(vars, ent.a);
                        Eigen::Vector3d p2 = getPoint(vars, ent.b);
                        addRes(((p1 - O).dot(N)) / lengthScale);
                        addRes(((p2 - O).dot(N)) / lengthScale);
                    } else if (ent.kind == "Arc") {
                        Eigen::Vector3d C = getPoint(vars, ent.center);
                        Eigen::Vector3d A = getPoint(vars, ent.a);
                        Eigen::Vector3d B = getPoint(vars, ent.b);
                        addRes(((C - O).dot(N)) / lengthScale);
                        addRes(((A - O).dot(N)) / lengthScale);
                        addRes(((B - O).dot(N)) / lengthScale);
                    }
                }
            } else if (c.kind == "ParallelYZ") {
                sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                Eigen::Vector3d U = toEigen(fr.u).normalized();
                const auto &lineEnt = candidate.entity(c.a);
                Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                Eigen::Vector3d dir = p2 - p1;
                double dNorm = dir.norm();
                addRes(dNorm > 1e-12 ? (dir.dot(U)) / dNorm : 0.0);
            } else if (c.kind == "ParallelZX") {
                sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                Eigen::Vector3d V = toEigen(fr.v).normalized();
                const auto &lineEnt = candidate.entity(c.a);
                Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                Eigen::Vector3d dir = p2 - p1;
                double dNorm = dir.norm();
                addRes(dNorm > 1e-12 ? (dir.dot(V)) / dNorm : 0.0);
            } else if (c.kind == "AlongZ") {
                sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                Eigen::Vector3d U = toEigen(fr.u).normalized();
                Eigen::Vector3d V = toEigen(fr.v).normalized();
                const auto &lineEnt = candidate.entity(c.a);
                Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                Eigen::Vector3d dir = p2 - p1;
                double dNorm = dir.norm();
                if (dNorm > 1e-12) {
                    addRes((dir.dot(U)) / dNorm);
                    addRes((dir.dot(V)) / dNorm);
                } else {
                    addRes(0.0);
                    addRes(0.0);
                }
            } else if (c.kind == "AlongX") {
                Eigen::Vector3d axisY = {0, 1, 0}, axisZ = {0, 0, 1};
                if (c.plane.has_value()) {
                    sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                    axisY = toEigen(fr.v).normalized();
                    axisZ = toEigen(fr.u).cross(axisY).normalized();
                }
                const auto &lineEnt = candidate.entity(c.a);
                Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                Eigen::Vector3d dir = p2 - p1;
                double dNorm = dir.norm();
                if (dNorm > 1e-12) {
                    addRes((dir.dot(axisY)) / dNorm);
                    addRes((dir.dot(axisZ)) / dNorm);
                } else {
                    addRes(0.0);
                    addRes(0.0);
                }
            } else if (c.kind == "AlongY") {
                Eigen::Vector3d axisX = {1, 0, 0}, axisZ = {0, 0, 1};
                if (c.plane.has_value()) {
                    sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                    axisX = toEigen(fr.u).normalized();
                    axisZ = axisX.cross(toEigen(fr.v)).normalized();
                }
                const auto &lineEnt = candidate.entity(c.a);
                Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                Eigen::Vector3d dir = p2 - p1;
                double dNorm = dir.norm();
                if (dNorm > 1e-12) {
                    addRes((dir.dot(axisX)) / dNorm);
                    addRes((dir.dot(axisZ)) / dNorm);
                } else {
                    addRes(0.0);
                    addRes(0.0);
                }
            } else if (c.kind == "EqualLength") {
                const auto &l1 = candidate.entity(c.a);
                const auto &l2 = candidate.entity(c.b);
                Eigen::Vector3d d1 = getPoint(vars, l1.b) - getPoint(vars, l1.a);
                Eigen::Vector3d d2 = getPoint(vars, l2.b) - getPoint(vars, l2.a);
                addRes((d1.norm() - d2.norm()) / lengthScale);
            } else if (c.kind == "Parallel") {
                const auto &l1 = candidate.entity(c.a);
                const auto &l2 = candidate.entity(c.b);
                Eigen::Vector3d d1 = getPoint(vars, l1.b) - getPoint(vars, l1.a);
                Eigen::Vector3d d2 = getPoint(vars, l2.b) - getPoint(vars, l2.a);
                double n1 = d1.norm(), n2 = d2.norm();
                if (n1 > 1e-12 && n2 > 1e-12) {
                    addVec3((d1 / n1).cross(d2 / n2));
                } else {
                    addVec3(Eigen::Vector3d::Zero());
                }
            } else if (c.kind == "Perpendicular") {
                const auto &l1 = candidate.entity(c.a);
                const auto &l2 = candidate.entity(c.b);
                Eigen::Vector3d d1 = getPoint(vars, l1.b) - getPoint(vars, l1.a);
                Eigen::Vector3d d2 = getPoint(vars, l2.b) - getPoint(vars, l2.a);
                double n1 = d1.norm(), n2 = d2.norm();
                addRes(n1 > 1e-12 && n2 > 1e-12 ? (d1 / n1).dot(d2 / n2) : 0.0);
            } else if (c.kind == "Angle") {
                const auto &l1 = candidate.entity(c.a);
                Eigen::Vector3d d1 = getPoint(vars, l1.b) - getPoint(vars, l1.a);
                double n1 = d1.norm();
                if (c.plane.has_value()) {
                    sketch::Frame fr = defaultResolvePlane(*c.plane, resolve);
                    Eigen::Vector3d N = toEigen(fr.u).cross(toEigen(fr.v)).normalized();
                    if (n1 > 1e-12) {
                        double dotVal = std::clamp((d1 / n1).dot(N), -1.0, 1.0);
                        double linePlaneAngle = std::abs(std::asin(dotVal));
                        addRes(linePlaneAngle - c.value);
                    } else {
                        addRes(0.0);
                    }
                } else {
                    const auto &l2 = candidate.entity(c.b);
                    Eigen::Vector3d d2 = getPoint(vars, l2.b) - getPoint(vars, l2.a);
                    double n2 = d2.norm();
                    if (n1 > 1e-12 && n2 > 1e-12) {
                        double cosTheta = std::clamp((d1 / n1).dot(d2 / n2), -1.0, 1.0);
                        double angle = std::acos(cosTheta);
                        addRes(angle - c.value);
                    } else {
                        addRes(0.0);
                    }
                }
            } else if (c.kind == "Tangent") {
                const auto &entA = candidate.entity(c.a);
                const auto &entB = candidate.entity(c.b);
                const auto &lineEnt = entA.kind == "Line" ? entA : entB;
                const auto &arcEnt = entA.kind == "Arc" ? entA : entB;

                Eigen::Vector3d p1 = getPoint(vars, lineEnt.a);
                Eigen::Vector3d p2 = getPoint(vars, lineEnt.b);
                Eigen::Vector3d dir = p2 - p1;
                Eigen::Vector3d C = getPoint(vars, arcEnt.center);
                Eigen::Vector3d A = getPoint(vars, arcEnt.a);
                Eigen::Vector3d B = getPoint(vars, arcEnt.b);

                double d1A = (p1 - A).norm(), d1B = (p1 - B).norm();
                double d2A = (p2 - A).norm(), d2B = (p2 - B).norm();
                double minD = std::min({d1A, d1B, d2A, d2B});
                Eigen::Vector3d touch = (minD == d1A || minD == d2A) ? A : B;
                Eigen::Vector3d rad = touch - C;
                double rNorm = rad.norm(), dNorm = dir.norm();
                Eigen::Vector3d arcN = toEigen(arcEnt.normal).normalized();
                if (rNorm > 1e-12 && dNorm > 1e-12) {
                    addRes((dir / dNorm).dot(rad / rNorm));
                    addRes((dir / dNorm).dot(arcN));
                } else {
                    addRes(0.0);
                    addRes(0.0);
                }
            }

            if (recordBlocks) {
                ConstraintBlock block;
                block.constraintId = c.id;
                block.startRow = startIdx;
                block.rowCount = int(resList.size()) - startIdx;
                blocks.push_back(block);
            }
        }

        Eigen::VectorXd res(resList.size());
        for (size_t i = 0; i < resList.size(); ++i)
            res[int(i)] = resList[i];
        return res;
    };

    auto computeJacobian = [&](const Eigen::VectorXd &vars) -> Eigen::MatrixXd {
        Eigen::VectorXd r0 = computeResiduals(vars, false);
        int m = int(r0.size());
        int n = int(vars.size());
        Eigen::MatrixXd J(m, n);
        double h = 1e-7;
        for (int j = 0; j < n; ++j) {
            Eigen::VectorXd xp = vars, xm = vars;
            xp[j] += h;
            xm[j] -= h;
            Eigen::VectorXd rp = computeResiduals(xp, false);
            Eigen::VectorXd rm = computeResiduals(xm, false);
            J.col(j) = (rp - rm) / (2.0 * h);
        }
        return J;
    };

    Eigen::VectorXd currentX = X;
    double lambda = 1e-3;
    bool converged = false;

    // Initial evaluation and block recording
    Eigen::VectorXd R = computeResiduals(currentX, true);
    double initialMaxRes = R.size() > 0 ? R.cwiseAbs().maxCoeff() : 0.0;
    if (R.size() == 0 || initialMaxRes < 1e-7) {
        converged = true;
    }

    int maxIterations = 80;
    for (int iter = 0; iter < maxIterations && !converged; ++iter) {
        if (canceled && canceled()) {
            result.valid = false;
            result.status = "Canceled";
            result.message = "Spatial solve canceled";
            if (previous)
                result.sketch = *previous;
            return result;
        }

        double currentNorm = R.norm();
        double maxRes = R.cwiseAbs().maxCoeff();
        if (maxRes < 1e-7) {
            converged = true;
            break;
        }

        Eigen::MatrixXd J = computeJacobian(currentX);

        Eigen::MatrixXd JtJ = J.transpose() * J;
        Eigen::VectorXd JtR = J.transpose() * R;

        bool stepAccepted = false;
        for (int attempt = 0; attempt < 5; ++attempt) {
            Eigen::MatrixXd A = JtJ;
            A.diagonal().array() += lambda;

            Eigen::VectorXd delta = A.ldlt().solve(-JtR);
            if (!delta.allFinite())
                break;

            if (delta.norm() > 2.0)
                delta = delta.normalized() * 2.0;

            Eigen::VectorXd nextX = currentX + delta;
            Eigen::VectorXd nextR = computeResiduals(nextX, false);

            if (nextR.norm() < currentNorm) {
                currentX = nextX;
                R = nextR;
                lambda = std::max(1e-7, lambda * 0.2);
                stepAccepted = true;
                break;
            } else {
                lambda = std::min(1e5, lambda * 5.0);
            }
        }

        if (!stepAccepted && lambda >= 1e4)
            break;
    }

    R = computeResiduals(currentX, true);
    result.maximumResidual = R.size() > 0 ? R.cwiseAbs().maxCoeff() : 0.0;
    converged = (R.size() == 0 || result.maximumResidual < 1e-6);

    Eigen::MatrixXd J = computeJacobian(currentX);
    if (J.rows() > 0 && J.cols() > 0) {
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(J, Eigen::ComputeThinU | Eigen::ComputeThinV);
        svd.setThreshold(1e-5);
        result.rank = int(svd.rank());
    } else {
        result.rank = 0;
    }
    result.freedom = std::max(0, nVars - result.rank);

    if (converged) {
        result.valid = true;
        result.status = result.freedom == 0 ? "Solved" : "Underconstrained";
        result.message = "Spatial sketch constraints successfully satisfied";

        // Update point coordinates in candidate
        for (int i = 0; i < nPoints; ++i) {
            const auto &pid = pointIds[size_t(i)];
            auto &ent = candidate.entity(pid);
            ent.point = {currentX[i * 3 + 0], currentX[i * 3 + 1], currentX[i * 3 + 2]};
        }
        for (auto &c : candidate.constraints) {
            if (!c.driving) {
                c.value = measureSpatialDimension(candidate, c, resolve);
            }
        }
        result.sketch = candidate;

        // Check for redundant constraints
        for (const auto &b : blocks) {
            if (b.rowCount <= 0)
                continue;
            int remRows = J.rows() - b.rowCount;
            if (remRows <= 0) {
                // If only this block exists, it cannot be redundant unless rank is 0
                continue;
            }
            Eigen::MatrixXd remaining(remRows, J.cols());
            int rIdx = 0;
            for (int r = 0; r < J.rows(); ++r) {
                if (r < b.startRow || r >= b.startRow + b.rowCount) {
                    remaining.row(rIdx++) = J.row(r);
                }
            }
            int remRank = 0;
            if (remaining.rows() > 0 && remaining.cols() > 0) {
                Eigen::JacobiSVD<Eigen::MatrixXd> remSvd(remaining,
                                                         Eigen::ComputeThinU | Eigen::ComputeThinV);
                remSvd.setThreshold(1e-5);
                remRank = int(remSvd.rank());
            }
            if (remRank == result.rank) {
                // Leaving out block b does not drop the rank
                result.redundant.append(b.constraintId);
            }
        }
    } else {
        result.valid = false;
        result.status = "FailedSolve";
        result.message = "Spatial sketch constraints did not converge; accepted state preserved";

        // Identify conflicting constraints based on block residuals and residual magnitude
        for (const auto &b : blocks) {
            if (b.rowCount <= 0)
                continue;
            double blockMax = 0.0;
            for (int r = b.startRow; r < b.startRow + b.rowCount; ++r) {
                blockMax = std::max(blockMax, std::abs(R[r]));
            }
            if (blockMax > 1e-4) {
                result.conflicting.append(b.constraintId);
            }
        }
        if (result.conflicting.isEmpty()) {
            for (const auto &c : candidate.constraints)
                result.conflicting.append(c.id);
        }

        if (previous)
            result.sketch = *previous;
    }

    return result;
}

QCborMap evaluateSpatial(const SpatialSketch &candidate, const SpatialSketch *previous,
                         const PlaneResolver &resolve, const CancelChecker &canceled) {
    auto res = solveSpatial(candidate, previous, resolve, canceled);
    QCborMap map;
    map.insert(QStringLiteral("sketch"), res.sketch.map());
    map.insert(QStringLiteral("diagnostics"), res.diagnostic());
    return map;
}

} // namespace cad::sketch

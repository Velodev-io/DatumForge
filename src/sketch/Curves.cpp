#include "Curves.hpp"
#include <algorithm>
#include <cmath>
namespace cad::sketch {
namespace {
constexpr double tau = 6.2831853071795864769;
Vec2 add(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
Vec2 sub(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
Vec2 mul(Vec2 a, double n) {
    return {a.x * n, a.y * n};
}
double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
Vec2 unit(Vec2 a) {
    auto n = std::hypot(a.x, a.y);
    domain::check(n > 1e-12, "InvalidGeometry", "Curve axis is degenerate");
    return mul(a, 1 / n);
}
std::pair<std::vector<double>, std::vector<double>> basis(const Entity &e, double t) {
    std::vector<double> k;
    for (size_t i = 0; i < e.knots.size(); ++i)
        for (int j = 0; j < e.multiplicities[i]; ++j)
            k.push_back(e.knots[i]);
    double u = t == e.knots.back() ? std::nextafter(t, e.knots.front()) : t;
    std::vector<double> n(k.size() - 1), lower;
    for (size_t i = 0; i + 1 < k.size(); ++i)
        n[i] = (u >= k[i] && u < k[i + 1]) ? 1 : 0;
    for (int d = 1; d <= e.degree; ++d) {
        lower = n;
        std::vector<double> next(n.size() - 1);
        for (size_t i = 0; i < next.size(); ++i) {
            double a = k[i + d] - k[i], b = k[i + d + 1] - k[i + 1];
            next[i] = (a > 0 ? (u - k[i]) / a * n[i] : 0) +
                      (b > 0 ? (k[i + d + 1] - u) / b * n[i + 1] : 0);
        }
        n = std::move(next);
    }
    std::vector<double> derivative(n.size());
    for (size_t i = 0; i < n.size(); ++i) {
        double a = k[i + e.degree] - k[i], b = k[i + e.degree + 1] - k[i + 1];
        derivative[i] = e.degree * ((a > 0 ? lower[i] / a : 0) - (b > 0 ? lower[i + 1] / b : 0));
    }
    return {n, derivative};
}
std::pair<Vec2, Vec2> spline(const Sketch &s, const Entity &e, double u) {
    auto [n, d] = basis(e, u);
    Vec2 p, v;
    double w = 0, dw = 0;
    for (int i = 0; i < e.poles.size(); ++i) {
        auto q = s.entity(e.poles[i]).point;
        double f = n[i] * e.weights[i], df = d[i] * e.weights[i];
        p = add(p, mul(q, f));
        v = add(v, mul(q, df));
        w += f;
        dw += df;
    }
    domain::check(w > 1e-14, "InvalidGeometry", "Spline homogeneous denominator is zero");
    return {mul(p, 1 / w), mul(sub(mul(v, w), mul(p, dw)), 1 / (w * w))};
}
void admit(const Sketch &s, size_t entities) {
    domain::check(s.entities.size() + entities <= 256, "ResourceLimit",
                  "Curve would exceed existing sketch entity limit");
}
} // namespace
bool ellipseCurve(QString k) {
    return k == "Ellipse" || k == "EllipticalArc";
}
bool advancedCurve(QString k) {
    return ellipseCurve(k) || k == "Parabola" || k == "Spline";
}
bool radiusCurve(QString k) {
    return k == "Circle" || k == "Arc" || ellipseCurve(k);
}
Vec2 location(const Sketch &s, const Entity &e) {
    if (e.kind == "Point")
        return e.point;
    auto a = s.entity(e.a).point;
    if (e.kind == "Line") {
        auto b = s.entity(e.b).point;
        return {(a.x + b.x) / 2, (a.y + b.y) / 2};
    }
    return a;
}
QStringList controlPoints(const Entity &e) {
    return e.kind == "Spline"                          ? e.poles
           : e.kind == "Point"                         ? QStringList{e.id}
           : e.kind == "Line" || advancedCurve(e.kind) ? QStringList{e.a, e.b}
                                                       : QStringList{e.a};
}
std::pair<double, double> curveRange(const Entity &e) {
    return e.kind == "Ellipse" || e.kind == "Circle" ? std::pair{0., tau}
           : e.kind == "Line"                        ? std::pair{0., 1.}
                                                     : std::pair{e.start, e.start + e.sweep};
}
Vec2 curveValue(const Sketch &s, const Entity &e, double u) {
    if (e.kind == "Spline")
        return spline(s, e, u).first;
    auto c = s.entity(e.a).point;
    if (e.kind == "Line")
        return add(c, mul(sub(s.entity(e.b).point, c), u));
    if (e.kind == "Circle" || e.kind == "Arc")
        return add(c, {e.radius * std::cos(u), e.radius * std::sin(u)});
    auto f = sub(s.entity(e.b).point, c), x = unit(f);
    Vec2 y{-x.y, x.x};
    if (ellipseCurve(e.kind)) {
        double a = std::hypot(length(f, {}), e.radius);
        return add(c, add(mul(x, a * std::cos(u)), mul(y, e.radius * std::sin(u))));
    }
    domain::check(e.kind == "Parabola", "UnsupportedOperation", "Unknown exact curve");
    return add(c, add(mul(x, u * u / (4 * length(f, {}))), mul(y, u)));
}
Vec2 curveDerivative(const Sketch &s, const Entity &e, double u) {
    if (e.kind == "Spline")
        return spline(s, e, u).second;
    if (e.kind == "Line")
        return sub(s.entity(e.b).point, s.entity(e.a).point);
    if (e.kind == "Circle" || e.kind == "Arc")
        return {-e.radius * std::sin(u), e.radius * std::cos(u)};
    auto f = sub(s.entity(e.b).point, s.entity(e.a).point), x = unit(f);
    Vec2 y{-x.y, x.x};
    if (ellipseCurve(e.kind))
        return add(mul(x, -std::hypot(length(f, {}), e.radius) * std::sin(u)),
                   mul(y, e.radius * std::cos(u)));
    return add(mul(x, u / (2 * length(f, {}))), y);
}
void validateCurve(const Sketch &s, const Entity &e) {
    using domain::check;
    check(advancedCurve(e.kind), "UnsupportedOperation", "Unknown exact curve");
    if (e.kind == "Spline") {
        check(e.degree >= 1 && e.degree <= 5 && e.poles.size() >= e.degree + 1 &&
                  e.poles.size() <= 64 && e.weights.size() == size_t(e.poles.size()) &&
                  e.knots.size() >= 2 && e.knots.size() <= 64 &&
                  e.knots.size() == e.multiplicities.size(),
              "InvalidInput", "Invalid spline degree/control/knot counts");
        size_t sum = 0;
        bool varied = false;
        for (int i = 0; i < e.poles.size(); ++i) {
            check(s.entity(e.poles[i]).kind == "Point", "InvalidInput",
                  "Spline poles require point identities");
            check(std::isfinite(e.weights[i]) && e.weights[i] >= 1e-6 && e.weights[i] <= 1e6,
                  "InvalidGeometry", "Spline weights must be positive and bounded");
            varied |= length(s.entity(e.poles[i]).point, s.entity(e.poles[0]).point) > 1e-9;
        }
        for (size_t i = 0; i < e.knots.size(); ++i) {
            check(std::isfinite(e.knots[i]) && std::abs(e.knots[i]) <= 1e6 &&
                      (!i || e.knots[i] > e.knots[i - 1]) && e.multiplicities[i] > 0 &&
                      e.multiplicities[i] <=
                          (i == 0 || i + 1 == e.knots.size() ? e.degree + 1 : e.degree),
                  "InvalidInput", "Invalid spline knot/multiplicity");
            sum += e.multiplicities[i];
        }
        check(varied && sum == size_t(e.poles.size() + e.degree + 1) &&
                  e.multiplicities.front() == e.degree + 1 &&
                  e.multiplicities.back() == e.degree + 1,
              "InvalidGeometry", "Spline requires nondegenerate controls and clamped knot counts");
        check(e.a == e.poles[0] && std::isfinite(e.start) && std::isfinite(e.sweep) &&
                  e.sweep > 1e-12 && e.start >= e.knots.front() &&
                  e.start + e.sweep <= e.knots.back(),
              "InvalidGeometry", "Invalid spline trimmed range");
    } else {
        check(s.entity(e.a).kind == "Point" && s.entity(e.b).kind == "Point" &&
                  length(s.entity(e.a).point, s.entity(e.b).point) > 1e-9,
              "InvalidGeometry", "Conic requires distinct center/vertex and focus control points");
        if (ellipseCurve(e.kind))
            check(std::isfinite(e.radius) && e.radius > 1e-9 && e.radius <= 1e3, "InvalidGeometry",
                  "Ellipse minor radius must exceed 1 nm");
        if (e.kind != "Ellipse")
            check(std::isfinite(e.start) && std::isfinite(e.sweep) && e.sweep > 1e-9 &&
                      (e.kind == "Parabola"
                           ? std::abs(e.start) <= 1e3 && std::abs(e.start + e.sweep) <= 1e3
                           : std::abs(e.start) <= tau && e.sweep < tau - 1e-8),
                  "InvalidGeometry", "Invalid trimmed conic range");
    }
    if (ellipseCurve(e.kind)) {
        auto center = s.entity(e.a).point, f = sub(s.entity(e.b).point, center), axis = unit(f);
        double major = std::hypot(length(f, {}), e.radius);
        double x = std::hypot(major * axis.x, e.radius * axis.y),
               y = std::hypot(major * axis.y, e.radius * axis.x);
        check(std::abs(center.x) + x <= 1e3 && std::abs(center.y) + y <= 1e3, "InvalidGeometry",
              "Ellipse exceeds 1 km coordinate bounds");
    }
    auto [a, b] = curveRange(e);
    if (e.kind == "Parabola") {
        auto vertex = s.entity(e.a).point, f = sub(s.entity(e.b).point, vertex), axis = unit(f);
        Vec2 normal{-axis.y, axis.x};
        for (auto pair : {std::pair{axis.x, normal.x}, std::pair{axis.y, normal.y}})
            if (std::abs(pair.first) > 1e-14) {
                double t = -2 * length(f, {}) * pair.second / pair.first;
                if (t > a && t < b) {
                    auto q = curveValue(s, e, t);
                    check(std::abs(q.x) <= 1e3 && std::abs(q.y) <= 1e3, "InvalidGeometry",
                          "Parabola exceeds 1 km coordinate bounds");
                }
            }
    }
    for (double t : {a, (a + b) / 2, b}) {
        auto p = curveValue(s, e, t);
        check(std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e3 &&
                  std::abs(p.y) <= 1e3,
              "InvalidGeometry", "Exact curve exceeds finite 1 km bounds");
    }
}
QString createEllipse(Sketch &s, Vec2 center, Vec2 major, Vec2 minor, bool arc, double start,
                      double sweep, bool construction) {
    admit(s, 3);
    auto c = s;
    auto v = sub(major, center), axis = unit(v);
    double a = length(v, {}), b = std::abs(dot(sub(minor, center), {-axis.y, axis.x}));
    domain::check(b > 1e-9 && a > b + 1e-9, "InvalidGeometry",
                  "Ellipse requires distinct positive major and minor radii");
    Entity e;
    e.kind = arc ? "EllipticalArc" : "Ellipse";
    e.construction = construction;
    e.a = c.point(center, true);
    e.b = c.point(add(center, mul(axis, std::sqrt(a * a - b * b))), true);
    e.radius = b;
    e.start = start;
    e.sweep = sweep;
    c.entities.push_back(e);
    c.validate();
    s = std::move(c);
    return e.id;
}
QString createParabola(Sketch &s, Vec2 focus, Vec2 vertex, Vec2 first, Vec2 last,
                       bool construction) {
    admit(s, 3);
    auto c = s;
    auto x = unit(sub(focus, vertex));
    Vec2 y{-x.y, x.x};
    double a = dot(sub(first, vertex), y), b = dot(sub(last, vertex), y);
    Entity e;
    e.kind = "Parabola";
    e.construction = construction;
    e.a = c.point(vertex, true);
    e.b = c.point(focus, true);
    e.start = std::min(a, b);
    e.sweep = std::abs(a - b);
    c.entities.push_back(e);
    c.validate();
    s = std::move(c);
    return e.id;
}
QString createSpline(Sketch &s, const std::vector<Vec2> &controls, int degree, bool closed,
                     bool construction) {
    domain::check(degree >= 1 && degree <= 5 && controls.size() >= size_t(degree + 1) &&
                      controls.size() <= 63,
                  "InvalidInput", "Spline needs degree+1 to 63 control points");
    admit(s, controls.size() + (closed ? 2 : 1));
    auto c = s;
    Entity e;
    e.kind = "Spline";
    e.construction = construction;
    e.degree = degree;
    for (auto p : controls)
        e.poles.append(c.point(p, true));
    if (closed)
        e.poles.append(e.poles[0]);
    e.a = e.poles[0];
    e.weights = std::vector<double>(e.poles.size(), 1.);
    int intervals = e.poles.size() - degree;
    for (int i = 0; i <= intervals; ++i) {
        e.knots.push_back(double(i) / intervals);
        e.multiplicities.push_back(i == 0 || i == intervals ? degree + 1 : 1);
    }
    e.start = 0;
    e.sweep = 1;
    c.entities.push_back(e);
    c.validate();
    s = std::move(c);
    return e.id;
}
int curveInputCount(QString kind) {
    domain::check(advancedCurve(kind), "UnsupportedOperation", "Unknown curve construction");
    return kind == "Ellipse" ? 3 : kind == "EllipticalArc" ? 5 : kind == "Parabola" ? 4 : 0;
}
QString createCurveInput(Sketch &s, QString kind, const std::vector<Vec2> &p, int degree,
                         bool closed, bool construction) {
    if (kind == "Spline")
        return createSpline(s, p, degree, closed, construction);
    domain::check(int(p.size()) == curveInputCount(kind), "InvalidInput",
                  "Complete the exact curve input sequence");
    if (kind == "Parabola")
        return createParabola(s, p[0], p[1], p[2], p[3], construction);
    auto candidate = s;
    auto id = createEllipse(candidate, p[0], p[1], p[2], kind == "EllipticalArc", 0, tau / 4,
                            construction);
    if (kind == "EllipticalArc") {
        auto &e = candidate.entity(id);
        auto axis = unit(sub(p[1], p[0]));
        Vec2 normal{-axis.y, axis.x};
        double major = length(p[1], p[0]);
        auto parameter = [&](Vec2 q) {
            q = sub(q, p[0]);
            return std::atan2(dot(q, normal) / e.radius, dot(q, axis) / major);
        };
        e.start = parameter(p[3]);
        e.sweep = std::fmod(parameter(p[4]) - e.start + tau, tau);
        candidate.validate();
    }
    s = std::move(candidate);
    return id;
}
} // namespace cad::sketch

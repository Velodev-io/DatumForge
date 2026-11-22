#include "SpatialCurve.hpp"
#include "SketchCurve.hpp"
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepOffsetAPI_NormalProjection.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepProj_Projection.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <GeomConvert_CompCurveToBSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <algorithm>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <numbers>

namespace cad::geometry {

using domain::check;

SupportCurve spatialEntityCurve(const sketch::SpatialSketch &s, const sketch::SpatialEntity &e) {
    if (e.kind == "Line") {
        auto a = kernelPoint(s.entity(e.a).point);
        auto b = kernelPoint(s.entity(e.b).point);
        double dist = a.Distance(b);
        check(dist > 1e-6, "InvalidGeometry", "Degenerate zero-length line");
        auto seg = GC_MakeSegment(a, b).Value();
        return {e.id, seg, 0.0, dist, dist, false, {}};
    }
    if (e.kind == "Arc") {
        auto center = kernelPoint(s.entity(e.center).point);
        auto a = kernelPoint(s.entity(e.a).point);
        auto b = kernelPoint(s.entity(e.b).point);
        gp_Dir normal(e.normal[0], e.normal[1], e.normal[2]);
        double r = e.radius * 1000.0;
        gp_Vec refDir(center, a);
        check(refDir.Magnitude() > 1e-6, "InvalidGeometry", "Degenerate arc radius");
        gp_Ax2 ax(center, normal, gp_Dir(refDir));
        gp_Circ circ(ax, r);
        auto arc = GC_MakeArcOfCircle(circ, a, b, true).Value();
        double f = arc->FirstParameter();
        double l = arc->LastParameter();
        return {e.id, arc, f, l, l - f, false, {}};
    }
    if (e.kind == "Spline") {
        int nPoles = int(e.poles.size());
        check(nPoles >= 2, "InvalidInput", "Spline requires at least 2 control poles");
        TColgp_Array1OfPnt poles(1, nPoles);
        for (int i = 0; i < nPoles; ++i) {
            poles(i + 1) = kernelPoint(s.entity(e.poles[i]).point);
        }

        std::vector<double> knots;
        std::vector<int> multiplicities;
        if (e.knots.empty()) {
            const int count = nPoles - e.degree + 1;
            for (int i = 0; i < count; ++i) {
                knots.push_back(double(i) / (count - 1));
                multiplicities.push_back(i == 0 || i == count - 1 ? e.degree + 1 : 1);
            }
        } else {
            for (double knot : e.knots) {
                if (knots.empty() || knot != knots.back()) {
                    knots.push_back(knot);
                    multiplicities.push_back(1);
                } else {
                    ++multiplicities.back();
                }
            }
        }
        TColStd_Array1OfReal occtKnots(1, int(knots.size()));
        TColStd_Array1OfInteger occtMults(1, int(knots.size()));
        for (size_t i = 0; i < knots.size(); ++i) {
            occtKnots(int(i) + 1) = knots[i];
            occtMults(int(i) + 1) = multiplicities[i];
        }
        Handle(Geom_BSplineCurve) spline;
        if (e.weights.empty()) {
            spline = new Geom_BSplineCurve(poles, occtKnots, occtMults, e.degree);
        } else {
            TColStd_Array1OfReal weights(1, nPoles);
            for (int i = 0; i < nPoles; ++i)
                weights(i + 1) = e.weights[size_t(i)];
            spline = new Geom_BSplineCurve(poles, weights, occtKnots, occtMults, e.degree);
        }
        return {e.id,
                spline,
                spline->FirstParameter(),
                spline->LastParameter(),
                spline->LastParameter() - spline->FirstParameter(),
                false,
                {}};
    }
    throw domain::Error("InvalidInput",
                        "Unsupported spatial entity kind for exact curve: " + e.kind);
}

std::vector<SupportCurve> evaluateSpatialSketch(const sketch::SpatialSketch &s) {
    std::vector<SupportCurve> result;
    for (const auto &e : s.entities) {
        if (e.kind != "Point" && !e.construction) {
            result.push_back(spatialEntityCurve(s, e));
        }
    }
    return result;
}

namespace {

std::vector<SupportCurve> collectSourceCurves(const domain::GeometryReference &ref,
                                              const SupportReferences &scene) {
    std::vector<SupportCurve> result;
    if (scene.sketches.contains(ref.object)) {
        const auto &s = scene.sketches.value(ref.object);
        if (!ref.entity.isEmpty()) {
            const auto &e = s.entity(ref.entity);
            if (e.kind != "Point") {
                auto sc = sketchCurve(s, e);
                result.push_back({e.id, sc.curve, sc.first, sc.last, sc.parameterScale, false, {}});
            }
        } else {
            for (const auto &e : s.entities) {
                if (e.kind != "Point" && !e.construction) {
                    auto sc = sketchCurve(s, e);
                    result.push_back(
                        {e.id, sc.curve, sc.first, sc.last, sc.parameterScale, false, {}});
                }
            }
        }
    } else if (scene.spatialSketches.contains(ref.object)) {
        const auto &s = scene.spatialSketches.value(ref.object);
        if (!ref.entity.isEmpty()) {
            const auto &e = s.entity(ref.entity);
            if (e.kind != "Point")
                result.push_back(spatialEntityCurve(s, e));
        } else {
            auto curves = evaluateSpatialSketch(s);
            result.insert(result.end(), curves.begin(), curves.end());
        }
    } else if (scene.derived.contains(ref.object)) {
        result = scene.derived.value(ref.object);
    } else {
        throw domain::Error("ReferenceMissing", "Source sketch is missing: " + ref.object);
    }
    check(!result.empty(), "ReferenceMissing", "No curve geometry found in source: " + ref.object);
    return result;
}

TopoDS_Shape makeExtrusion(const std::vector<SupportCurve> &curves, const gp_Vec &dir,
                           double depth) {
    BRep_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);

    for (const auto &c : curves) {
        if (!c.curve.IsNull()) {
            BRepBuilderAPI_MakeEdge makeEdge(c.curve, c.first, c.last);
            if (makeEdge.IsDone()) {
                TopoDS_Edge edge = makeEdge.Edge();
                BRepPrimAPI_MakePrism prismPos(edge, dir * depth);
                BRepPrimAPI_MakePrism prismNeg(edge, -dir * depth);
                if (prismPos.IsDone())
                    builder.Add(comp, prismPos.Shape());
                if (prismNeg.IsDone())
                    builder.Add(comp, prismNeg.Shape());
            }
        }
    }
    return comp;
}

} // namespace

std::vector<BranchCandidate> enumerateSketchOnSketchBranches(const model::DerivedCurve &c,
                                                             const SupportReferences &scene) {
    check(c.sketchOnSketch.has_value(), "InvalidInput", "SketchOnSketch definition missing");
    const auto &def = *c.sketchOnSketch;

    sketch::Frame f1;
    if (scene.sketches.contains(def.sketch1.object))
        f1 = scene.sketches.value(def.sketch1.object).frame;
    else if (scene.planes.contains(def.sketch1.object))
        f1 = scene.planes.value(def.sketch1.object);
    else
        throw domain::Error("ReferenceMissing",
                            "Source sketch plane is missing: " + def.sketch1.object);

    sketch::Frame f2;
    if (scene.sketches.contains(def.sketch2.object))
        f2 = scene.sketches.value(def.sketch2.object).frame;
    else if (scene.planes.contains(def.sketch2.object))
        f2 = scene.planes.value(def.sketch2.object);
    else
        throw domain::Error("ReferenceMissing",
                            "Source sketch plane is missing: " + def.sketch2.object);

    gp_Vec n1 = vector(f1.u).Crossed(vector(f1.v)).Normalized();
    gp_Vec n2 = vector(f2.u).Crossed(vector(f2.v)).Normalized();

    check(n1.Crossed(n2).Magnitude() > 1e-4, "InvalidGeometry",
          "Sketch-on-sketch projection planes must not be parallel");

    auto curves1 = collectSourceCurves(def.sketch1, scene);
    auto curves2 = collectSourceCurves(def.sketch2, scene);

    double extent = 2000.0;
    TopoDS_Shape shape1 = makeExtrusion(curves1, n1, extent);
    TopoDS_Shape shape2 = makeExtrusion(curves2, n2, extent);

    BRepAlgoAPI_Section sec(shape1, shape2);
    sec.Build();
    check(sec.IsDone(), "InvalidGeometry", "Sketch-on-sketch intersection failed");

    std::vector<BranchCandidate> candidates;
    for (TopExp_Explorer exp(sec.Shape(), TopAbs_EDGE); exp.More(); exp.Next()) {
        TopoDS_Edge edge = TopoDS::Edge(exp.Current());
        Standard_Real f, l;
        Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, f, l);
        if (!curve.IsNull() && std::abs(l - f) > 1e-6) {
            gp_Pnt pStart = curve->Value(f);
            gp_Pnt pEnd = curve->Value(l);
            if (pStart.Distance(pEnd) > 1e-6 || std::abs(l - f) > 1e-3) {
                BranchCandidate cand;
                cand.token = supportToken(edge);
                cand.curve = new Geom_TrimmedCurve(curve, f, l);
                cand.first = f;
                cand.last = l;
                cand.startPt = pStart;
                cand.endPt = pEnd;
                candidates.push_back(cand);
            }
        }
    }

    check(!candidates.empty(), "InvalidGeometry",
          "Sketch-on-sketch projection resulted in no intersection curves");

    // Sort deterministically by starting point coordinates
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        if (std::abs(a.startPt.X() - b.startPt.X()) > 1e-6)
            return a.startPt.X() < b.startPt.X();
        if (std::abs(a.startPt.Y() - b.startPt.Y()) > 1e-6)
            return a.startPt.Y() < b.startPt.Y();
        return a.startPt.Z() < b.startPt.Z();
    });

    return candidates;
}

namespace {
const BranchCandidate &selectedBranch(const std::vector<BranchCandidate> &candidates,
                                      const model::BranchSelection &selection) {
    if (selection.token == QString("branch/%1").arg(selection.index)) {
        check(selection.index >= 0 && size_t(selection.index) < candidates.size(),
              "ReferenceMissing", "Legacy projection branch is missing");
        return candidates[size_t(selection.index)];
    }
    const BranchCandidate *match = nullptr;
    for (const auto &candidate : candidates)
        if (candidate.token == selection.token) {
            check(!match, "ReferenceAmbiguous", "Multiple branches have the saved geometry");
            match = &candidate;
        }
    check(match, "ReferenceMissing", "Selected branch changed or is missing; repair explicitly");
    return *match;
}
} // namespace

std::vector<SupportCurve> evaluateSketchOnSketch(const model::DerivedCurve &c,
                                                 const SupportReferences &scene) {
    check(c.sketchOnSketch.has_value(), "InvalidInput", "SketchOnSketch definition missing");
    const auto &def = *c.sketchOnSketch;
    auto candidates = enumerateSketchOnSketchBranches(c, scene);

    const auto &sel = def.branch;
    const auto &chosen = selectedBranch(candidates, sel);
    Handle(Geom_Curve) finalCurve = chosen.curve;
    double f = chosen.first, l = chosen.last;

    if (sel.startParam > 0.0 || sel.endParam < 1.0) {
        double subF = f + sel.startParam * (l - f);
        double subL = f + sel.endParam * (l - f);
        finalCurve = new Geom_TrimmedCurve(finalCurve, subF, subL);
        f = subF;
        l = subL;
    }

    if (sel.reversed) {
        finalCurve = new Geom_TrimmedCurve(finalCurve, f, l);
        finalCurve->Reverse();
        f = finalCurve->FirstParameter();
        l = finalCurve->LastParameter();
    }

    SupportCurve sc;
    sc.key = c.id;
    sc.projectionToken = chosen.token;
    sc.curve = finalCurve;
    sc.first = f;
    sc.last = l;
    sc.parameterScale = l - f;
    sc.edge = false;
    return {sc};
}

std::vector<BranchCandidate> enumerateSketchOnFacesBranches(const model::DerivedCurve &c,
                                                            const SupportReferences &scene) {
    check(c.sketchOnFaces.has_value(), "InvalidInput", "SketchOnFaces definition missing");
    const auto &def = *c.sketchOnFaces;

    auto curves = collectSourceCurves(def.sketch, scene);

    BRep_Builder b;
    TopoDS_Compound targetFaces;
    b.MakeCompound(targetFaces);
    for (const auto &fRef : def.faces) {
        b.Add(targetFaces, scene.topology(fRef));
    }

    std::vector<BranchCandidate> candidates;

    if (def.mode == "AlongVector") {
        check(def.direction.has_value(), "InvalidInput",
              "AlongVector mode requires direction reference");
        SupportAxis sa = scene.axis(*def.direction);
        gp_Vec dir(sa.direction[0], sa.direction[1], sa.direction[2]);
        check(dir.Magnitude() > 1e-6, "InvalidGeometry", "Degenerate projection direction");
        dir.Normalize();

        double extent = 2000.0;
        TopoDS_Shape extruded = makeExtrusion(curves, dir, extent);

        BRepAlgoAPI_Section sec(extruded, targetFaces);
        sec.Build();
        if (sec.IsDone()) {
            for (TopExp_Explorer exp(sec.Shape(), TopAbs_EDGE); exp.More(); exp.Next()) {
                TopoDS_Edge edge = TopoDS::Edge(exp.Current());
                Standard_Real f, l;
                Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, f, l);
                if (!curve.IsNull() && std::abs(l - f) > 1e-6) {
                    BranchCandidate cand;
                    cand.token = supportToken(edge);
                    cand.curve = new Geom_TrimmedCurve(curve, f, l);
                    cand.first = f;
                    cand.last = l;
                    cand.startPt = curve->Value(f);
                    cand.endPt = curve->Value(l);
                    candidates.push_back(cand);
                }
            }
        }
    } else {
        // NormalToFace
        check(!def.direction.has_value(), "InvalidInput",
              "NormalToFace mode forbids explicit direction reference");
        for (const auto &fRef : def.faces) {
            TopoDS_Shape targetShape = scene.topology(fRef);
            BRepOffsetAPI_NormalProjection proj(targetShape);
            for (const auto &sc : curves) {
                if (!sc.curve.IsNull()) {
                    BRepBuilderAPI_MakeEdge makeEdge(sc.curve, sc.first, sc.last);
                    if (makeEdge.IsDone())
                        proj.Add(makeEdge.Edge());
                }
            }
            proj.SetLimit(Standard_True);
            proj.Compute3d(Standard_True);
            proj.Build();

            if (proj.IsDone()) {
                for (TopExp_Explorer exp(proj.Projection(), TopAbs_EDGE); exp.More(); exp.Next()) {
                    TopoDS_Edge edge = TopoDS::Edge(exp.Current());
                    Standard_Real f, l;
                    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, f, l);
                    if (!curve.IsNull() && std::abs(l - f) > 1e-6) {
                        BranchCandidate cand;
                        cand.token = supportToken(edge);
                        cand.curve = new Geom_TrimmedCurve(curve, f, l);
                        cand.first = f;
                        cand.last = l;
                        cand.startPt = curve->Value(f);
                        cand.endPt = curve->Value(l);
                        candidates.push_back(cand);
                    }
                }
            }
        }
    }

    check(!candidates.empty(), "InvalidGeometry",
          "Sketch-on-faces projection resulted in no intersection curves");

    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        if (std::abs(a.startPt.X() - b.startPt.X()) > 1e-6)
            return a.startPt.X() < b.startPt.X();
        if (std::abs(a.startPt.Y() - b.startPt.Y()) > 1e-6)
            return a.startPt.Y() < b.startPt.Y();
        return a.startPt.Z() < b.startPt.Z();
    });

    return candidates;
}

std::vector<SupportCurve> evaluateSketchOnFaces(const model::DerivedCurve &c,
                                                const SupportReferences &scene) {
    check(c.sketchOnFaces.has_value(), "InvalidInput", "SketchOnFaces definition missing");
    const auto &def = *c.sketchOnFaces;
    auto candidates = enumerateSketchOnFacesBranches(c, scene);

    const auto &sel = def.branch;
    const auto &chosen = selectedBranch(candidates, sel);
    Handle(Geom_Curve) finalCurve = chosen.curve;
    double f = chosen.first, l = chosen.last;

    if (sel.startParam > 0.0 || sel.endParam < 1.0) {
        double subF = f + sel.startParam * (l - f);
        double subL = f + sel.endParam * (l - f);
        finalCurve = new Geom_TrimmedCurve(finalCurve, subF, subL);
        f = subF;
        l = subL;
    }

    if (sel.reversed) {
        finalCurve = new Geom_TrimmedCurve(finalCurve, f, l);
        finalCurve->Reverse();
        f = finalCurve->FirstParameter();
        l = finalCurve->LastParameter();
    }

    SupportCurve sc;
    sc.key = c.id;
    sc.projectionToken = chosen.token;
    sc.curve = finalCurve;
    sc.first = f;
    sc.last = l;
    sc.parameterScale = l - f;
    sc.edge = false;
    return {sc};
}

std::vector<SupportCurve> evaluateCompositeCurve(const model::DerivedCurve &c,
                                                 const SupportReferences &scene) {
    check(c.compositeCurve.has_value(), "InvalidInput", "CompositeCurve definition missing");
    const auto &def = *c.compositeCurve;
    check(!def.members.empty(), "InvalidInput", "Composite curve requires at least one member");

    std::vector<SupportCurve> memberCurves;
    for (size_t i = 0; i < def.members.size(); ++i) {
        const auto &m = def.members[i];
        SupportCurve sc = scene.curve(m.source);
        Handle(Geom_TrimmedCurve) trimmed = new Geom_TrimmedCurve(sc.curve, sc.first, sc.last);
        if (m.reversed) {
            trimmed->Reverse();
        }
        sc.curve = trimmed;
        sc.first = trimmed->FirstParameter();
        sc.last = trimmed->LastParameter();
        memberCurves.push_back(sc);
    }

    // Check connectivity and corners
    for (size_t i = 0; i + 1 < memberCurves.size(); ++i) {
        gp_Pnt pEnd = memberCurves[i].curve->Value(memberCurves[i].last);
        gp_Pnt pStart = memberCurves[i + 1].curve->Value(memberCurves[i + 1].first);
        double gap = pEnd.Distance(pStart);
        check(gap < 0.1, "InvalidGeometry",
              QString("Composite curve has disconnected gap of %1 mm between members %2 and %3")
                  .arg(gap)
                  .arg(i)
                  .arg(i + 1));
    }

    if (def.closed) {
        gp_Pnt pEnd = memberCurves.back().curve->Value(memberCurves.back().last);
        gp_Pnt pStart = memberCurves.front().curve->Value(memberCurves.front().first);
        double gap = pEnd.Distance(pStart);
        check(gap < 0.1, "InvalidGeometry",
              QString("Closed composite curve endpoints do not coincide; gap is %1 mm").arg(gap));
    }

    if (memberCurves.size() == 1) {
        SupportCurve sc = memberCurves[0];
        sc.key = c.id;
        return {sc};
    }

    GeomConvert_CompCurveToBSplineCurve comp(
        Handle(Geom_BoundedCurve)::DownCast(memberCurves[0].curve));
    for (size_t i = 1; i < memberCurves.size(); ++i) {
        auto nextCurve = Handle(Geom_BoundedCurve)::DownCast(memberCurves[i].curve);
        bool added = comp.Add(nextCurve, 0.1, Standard_True, Standard_True, 1);
        check(added, "InvalidGeometry",
              "Failed to join composite curve member into continuous path");
    }

    Handle(Geom_BSplineCurve) bspline = comp.BSplineCurve();
    check(!bspline.IsNull(), "InvalidGeometry", "Failed to build composite B-spline");

    SupportCurve sc;
    sc.key = c.id;
    sc.curve = bspline;
    sc.first = bspline->FirstParameter();
    sc.last = bspline->LastParameter();
    sc.parameterScale = sc.last - sc.first;
    sc.edge = false;
    return {sc};
}

std::vector<SupportCurve> evaluateHelixSpiral(const model::DerivedCurve &c,
                                              const SupportReferences &scene) {
    check(c.helixSpiral.has_value(), "InvalidInput", "HelixSpiral definition missing");
    const auto &def = *c.helixSpiral;

    double R = def.radius * 1000.0;
    double theta0 = def.startAngle;
    double taper = def.taperAngle;
    bool cw = def.clockwise;
    double H = 0, P = 0, N = 0;

    if (def.mode == "PitchAndRevolution") {
        P = def.pitch * 1000.0;
        N = def.revolutions;
        H = P * N;
    } else if (def.mode == "HeightAndRevolution") {
        H = def.height * 1000.0;
        N = def.revolutions;
        P = H / N;
    } else if (def.mode == "HeightAndPitch") {
        H = def.height * 1000.0;
        P = def.pitch * 1000.0;
        N = H / P;
    } else if (def.mode == "Spiral") {
        P = def.pitch * 1000.0;
        N = def.revolutions;
        H = 0.0;
        taper = 0.0;
    } else {
        throw domain::Error("InvalidInput", "Unsupported helix/spiral mode: " + def.mode);
    }

    SupportAxis sa = scene.axis(def.axis);
    gp_Pnt origin = kernelPoint(sa.origin);
    if (def.center.has_value()) {
        origin = scene.point(*def.center);
    }

    gp_Dir axisDir(sa.direction[0], sa.direction[1], sa.direction[2]);
    gp_Ax2 ax(origin, axisDir);
    gp_Dir xDir = ax.XDirection();
    gp_Dir yDir = ax.YDirection();

    check(std::isfinite(N) && N > 0 && N <= 1000, "ResourceLimit",
          "Helix or spiral exceeds the 1000-turn limit");
    const double omega = (cw ? -1.0 : 1.0) * (2.0 * std::numbers::pi * N);
    const double radialChange = def.mode == "Spiral" ? P * N : H * std::tan(taper);
    const double maxRadius = std::max(R, R + radialChange);
    // Cubic Hermite coordinate error is bounded by the fourth derivative times h^4 / 384.
    const double fourthDerivative =
        maxRadius * std::pow(omega, 4) + 4 * std::abs(radialChange * std::pow(omega, 3));
    constexpr double errorMM = 1e-4;
    const double required =
        std::ceil(std::pow(std::numbers::sqrt2 * fourthDerivative / (384 * errorMM), 0.25));
    check(std::isfinite(required) && required <= 2048, "ResourceLimit",
          "Helix or spiral exceeds the curve accuracy budget");
    const int segments = std::max(32, int(required));
    TColgp_Array1OfPnt poles(1, 3 * segments + 1);
    TColStd_Array1OfReal knots(1, segments + 1);
    TColStd_Array1OfInteger multiplicities(1, segments + 1);
    for (int i = 0; i <= segments; ++i) {
        const double t = double(i) / segments;
        const double theta = theta0 + omega * t;
        const double r = R + radialChange * t;
        const gp_XYZ radial = xDir.XYZ() * std::cos(theta) + yDir.XYZ() * std::sin(theta);
        const gp_XYZ tangent = xDir.XYZ() * -std::sin(theta) + yDir.XYZ() * std::cos(theta);
        const gp_XYZ point = origin.XYZ() + radial * r + axisDir.XYZ() * (H * t);
        const gp_XYZ handle =
            (radial * radialChange + tangent * (r * omega) + axisDir.XYZ() * H) / (3.0 * segments);
        poles(3 * i + 1) = gp_Pnt(point);
        if (i > 0)
            poles(3 * i) = gp_Pnt(point - handle);
        if (i < segments)
            poles(3 * i + 2) = gp_Pnt(point + handle);
        knots(i + 1) = t;
        multiplicities(i + 1) = i == 0 || i == segments ? 4 : 3;
    }
    Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(poles, knots, multiplicities, 3);
    SupportCurve sc;
    sc.key = c.id;
    sc.curve = curve;
    sc.first = curve->FirstParameter();
    sc.last = curve->LastParameter();
    sc.parameterScale = sc.last - sc.first;
    sc.edge = false;
    return {sc};
}

std::vector<SupportCurve> evaluateDerivedCurve(const model::DerivedCurve &c,
                                               const SupportReferences &scene) {
    if (c.kind == "SketchOnSketch")
        return evaluateSketchOnSketch(c, scene);
    if (c.kind == "SketchOnFaces")
        return evaluateSketchOnFaces(c, scene);
    if (c.kind == "CompositeCurve")
        return evaluateCompositeCurve(c, scene);
    if (c.kind == "HelixSpiral")
        return evaluateHelixSpiral(c, scene);
    throw domain::Error("InvalidInput", "Unsupported derived curve kind: " + c.kind);
}

QCborMap inspectBranchCandidates(const model::DerivedCurve &c, const SupportReferences &scene) {
    std::vector<BranchCandidate> candidates;
    model::BranchSelection currentBranch;
    if (c.sketchOnSketch.has_value()) {
        candidates = enumerateSketchOnSketchBranches(c, scene);
        currentBranch = c.sketchOnSketch->branch;
    } else if (c.sketchOnFaces.has_value()) {
        candidates = enumerateSketchOnFacesBranches(c, scene);
        currentBranch = c.sketchOnFaces->branch;
    } else {
        throw domain::Error("UnsupportedOperation",
                            "Branch inspection requires a SketchOnSketch or SketchOnFaces curve");
    }

    QCborArray replacements;
    QCborArray descriptions;
    QCborArray candidateTokens;
    int matches = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto &cand = candidates[i];
        candidateTokens.append(cand.token);
        if (cand.token == currentBranch.token)
            ++matches;
        model::BranchSelection sel;
        sel.index = int(i);
        sel.token = cand.token;
        sel.reversed = currentBranch.reversed;
        sel.startParam = currentBranch.startParam;
        sel.endParam = currentBranch.endParam;
        replacements.append(sel.map());
        descriptions.append(sketch::fields(
            {{"index", int(i)},
             {"token", cand.token},
             {"start_pt", QCborArray{cand.startPt.X() / 1000.0, cand.startPt.Y() / 1000.0,
                                     cand.startPt.Z() / 1000.0}},
             {"end_pt", QCborArray{cand.endPt.X() / 1000.0, cand.endPt.Y() / 1000.0,
                                   cand.endPt.Z() / 1000.0}}}));
    }

    QString state;
    if (candidates.empty())
        state = "Missing";
    else if (matches > 1 || (!matches && candidates.size() > 1))
        state = "Ambiguous";
    else if (!matches)
        state = "Missing";
    else
        state = "Direct";

    return sketch::fields({{"state", state},
                           {"candidates", candidateTokens},
                           {"reference", currentBranch.map()},
                           {"replacements", replacements},
                           {"descriptions", descriptions}});
}

QCborArray tessellateSupportCurve(const SupportCurve &c, int samples) {
    QCborArray arr;
    if (c.curve.IsNull())
        return arr;
    int n = std::clamp(samples, 8, 512);
    for (int i = 0; i <= n; ++i) {
        double t = c.first + (double(i) / double(n)) * (c.last - c.first);
        gp_Pnt p = c.curve->Value(t);
        sketch::Vec3 pt = siPoint(p);
        QCborArray ptArr;
        ptArr.append(pt[0]);
        ptArr.append(pt[1]);
        ptArr.append(pt[2]);
        arr.append(ptArr);
    }
    return arr;
}

} // namespace cad::geometry

#include "SpatialCurve.hpp"
#include "SketchCurve.hpp"
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepProj_Projection.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <GeomConvert_CompCurveToBSplineCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_HArray1OfBoolean.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array1OfVec.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TColgp_HArray1OfVec.hxx>
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

        int degree = e.degree;
        if (!e.weights.empty()) {
            TColStd_Array1OfReal weights(1, nPoles);
            for (int i = 0; i < nPoles; ++i)
                weights(i + 1) = e.weights[size_t(i)];
            if (!e.knots.empty()) {
                std::vector<double> uKnots;
                std::vector<int> uMults;
                for (double k : e.knots) {
                    if (uKnots.empty() || std::abs(k - uKnots.back()) > 1e-8) {
                        uKnots.push_back(k);
                        uMults.push_back(1);
                    } else {
                        uMults.back()++;
                    }
                }
                int nK = int(uKnots.size());
                TColStd_Array1OfReal occtKnots(1, nK);
                TColStd_Array1OfInteger occtMults(1, nK);
                for (int i = 0; i < nK; ++i) {
                    occtKnots(i + 1) = uKnots[size_t(i)];
                    occtMults(i + 1) = uMults[size_t(i)];
                }
                auto spline = new Geom_BSplineCurve(poles, weights, occtKnots, occtMults, degree);
                return {e.id,
                        spline,
                        spline->FirstParameter(),
                        spline->LastParameter(),
                        spline->LastParameter() - spline->FirstParameter(),
                        false,
                        {}};
            } else {
                int nK = nPoles - degree + 1;
                TColStd_Array1OfReal occtKnots(1, nK);
                TColStd_Array1OfInteger occtMults(1, nK);
                for (int i = 1; i <= nK; ++i) {
                    occtKnots(i) = double(i - 1) / double(nK - 1);
                    occtMults(i) = (i == 1 || i == nK) ? (degree + 1) : 1;
                }
                auto spline = new Geom_BSplineCurve(poles, weights, occtKnots, occtMults, degree);
                return {e.id,
                        spline,
                        spline->FirstParameter(),
                        spline->LastParameter(),
                        spline->LastParameter() - spline->FirstParameter(),
                        false,
                        {}};
            }
        } else {
            int nK = nPoles - degree + 1;
            TColStd_Array1OfReal occtKnots(1, nK);
            TColStd_Array1OfInteger occtMults(1, nK);
            for (int i = 1; i <= nK; ++i) {
                occtKnots(i) = double(i - 1) / double(nK - 1);
                occtMults(i) = (i == 1 || i == nK) ? (degree + 1) : 1;
            }
            auto spline = new Geom_BSplineCurve(poles, occtKnots, occtMults, degree);
            return {e.id,
                    spline,
                    spline->FirstParameter(),
                    spline->LastParameter(),
                    spline->LastParameter() - spline->FirstParameter(),
                    false,
                    {}};
        }
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

struct BranchCandidate {
    QString token;
    Handle(Geom_Curve) curve;
    double first = 0, last = 1;
    gp_Pnt startPt;
};

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

std::vector<SupportCurve> evaluateSketchOnSketch(const model::DerivedCurve &c,
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
                cand.curve = new Geom_TrimmedCurve(curve, f, l);
                cand.first = f;
                cand.last = l;
                cand.startPt = pStart;
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

    for (size_t i = 0; i < candidates.size(); ++i) {
        candidates[i].token = QString("branch/%1").arg(i);
    }

    int branchCount = int(candidates.size());
    const auto &sel = def.branch;
    check(sel.index >= 0 && sel.index < branchCount, "ReferenceAmbiguous",
          "Select one projection branch");
    check(candidates[size_t(sel.index)].token == sel.token, "ReferenceMissing",
          "Selected branch changed or missing; repair explicitly");

    const auto &chosen = candidates[size_t(sel.index)];
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
    sc.curve = finalCurve;
    sc.first = f;
    sc.last = l;
    sc.parameterScale = l - f;
    sc.edge = false;
    return {sc};
}

std::vector<SupportCurve> evaluateSketchOnFaces(const model::DerivedCurve &c,
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
                    cand.curve = new Geom_TrimmedCurve(curve, f, l);
                    cand.first = f;
                    cand.last = l;
                    cand.startPt = curve->Value(f);
                    candidates.push_back(cand);
                }
            }
        }
    } else {
        // NormalToFace
        check(!def.direction.has_value(), "InvalidInput",
              "NormalToFace mode forbids explicit direction reference");
        for (const auto &fRef : def.faces) {
            TopoDS_Face face = TopoDS::Face(scene.topology(fRef));
            for (const auto &sc : curves) {
                if (sc.curve.IsNull())
                    continue;
                BRepBuilderAPI_MakeEdge makeEdge(sc.curve, sc.first, sc.last);
                if (!makeEdge.IsDone())
                    continue;
                BRepBuilderAPI_MakeWire makeWire(makeEdge.Edge());
                if (!makeWire.IsDone())
                    continue;

                // Estimate normal from center of face
                gp_Pnt p0 = sc.curve->Value((sc.first + sc.last) * 0.5);
                gp_Vec normal = scene.normalAt(fRef, p0);
                if (normal.Magnitude() < 1e-6)
                    normal = gp_Vec(0, 0, 1);
                normal.Normalize();

                double extent = 2000.0;
                BRepPrimAPI_MakePrism prismPos(makeEdge.Edge(), normal * extent);
                BRepPrimAPI_MakePrism prismNeg(makeEdge.Edge(), -normal * extent);

                TopoDS_Compound compPrism;
                b.MakeCompound(compPrism);
                if (prismPos.IsDone())
                    b.Add(compPrism, prismPos.Shape());
                if (prismNeg.IsDone())
                    b.Add(compPrism, prismNeg.Shape());

                BRepAlgoAPI_Section sec(compPrism, face);
                sec.Build();
                if (sec.IsDone()) {
                    for (TopExp_Explorer exp(sec.Shape(), TopAbs_EDGE); exp.More(); exp.Next()) {
                        TopoDS_Edge edge = TopoDS::Edge(exp.Current());
                        Standard_Real f, l;
                        Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, f, l);
                        if (!curve.IsNull() && std::abs(l - f) > 1e-6) {
                            BranchCandidate cand;
                            cand.curve = new Geom_TrimmedCurve(curve, f, l);
                            cand.first = f;
                            cand.last = l;
                            cand.startPt = curve->Value(f);
                            candidates.push_back(cand);
                        }
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

    for (size_t i = 0; i < candidates.size(); ++i) {
        candidates[i].token = QString("branch/%1").arg(i);
    }

    int branchCount = int(candidates.size());
    const auto &sel = def.branch;
    check(sel.index >= 0 && sel.index < branchCount, "ReferenceAmbiguous",
          "Select one projection branch");
    check(candidates[size_t(sel.index)].token == sel.token, "ReferenceMissing",
          "Selected branch changed or missing; repair explicitly");

    const auto &chosen = candidates[size_t(sel.index)];
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

    int nSegments = std::clamp(int(32 * N), 32, 2048);
    Handle(TColgp_HArray1OfPnt) pts = new TColgp_HArray1OfPnt(1, nSegments + 1);
    TColgp_Array1OfVec tgts(1, nSegments + 1);
    Handle(TColStd_HArray1OfBoolean) flags = new TColStd_HArray1OfBoolean(1, nSegments + 1);
    flags->Init(Standard_True);

    for (int i = 0; i <= nSegments; ++i) {
        double t = double(i) / double(nSegments);
        double theta = theta0 + (cw ? -1.0 : 1.0) * (2.0 * std::numbers::pi * N * t);
        double z = H * t;
        double r = (def.mode == "Spiral") ? (R + P * N * t) : (R + z * std::tan(taper));
        check(r > 0.0, "InvalidGeometry", "Conical helix taper caused radius to collapse to zero");

        gp_Pnt pt = origin.XYZ() + xDir.XYZ() * (r * std::cos(theta)) +
                    yDir.XYZ() * (r * std::sin(theta)) + axisDir.XYZ() * z;
        pts->SetValue(i + 1, pt);

        double dr = (def.mode == "Spiral") ? (P * N) : (H * std::tan(taper));
        double dtheta = (cw ? -1.0 : 1.0) * (2.0 * std::numbers::pi * N);
        double dx = dr * std::cos(theta) - r * std::sin(theta) * dtheta;
        double dy = dr * std::sin(theta) + r * std::cos(theta) * dtheta;
        double dz = H;

        gp_Vec tgt = xDir.XYZ() * dx + yDir.XYZ() * dy + axisDir.XYZ() * dz;
        tgts(i + 1) = tgt;
    }

    GeomAPI_Interpolate interp(pts, Standard_False, 1e-7);
    interp.Load(tgts, flags);
    interp.Perform();
    check(interp.IsDone(), "InvalidGeometry", "Helix/spiral interpolation failed");

    Handle(Geom_BSplineCurve) curve = interp.Curve();
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

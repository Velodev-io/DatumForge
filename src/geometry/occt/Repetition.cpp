#include "Repetition.hpp"
#include "SupportReferences.hpp"
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <numbers>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;

namespace {
gp_Vec resolveDirection(const Context &context, const QCborMap &dirMap) {
    auto ref = domain::GeometryReference::parse(dirMap.value("reference").toMap());
    gp_Vec dir(1, 0, 0);
    try {
        if (ref.role == "Axis") {
            auto a = context.references.axis(ref);
            dir = vector(a.direction);
        } else if (ref.role == "Edge" || ref.role == "Line" || ref.role == "Curve") {
            auto c = context.references.curve(ref);
            gp_Pnt p1;
            gp_Vec v1;
            c.curve->D1(c.first, p1, v1);
            dir = v1.Normalized();
        } else if (ref.role == "Plane" || ref.role == "Face") {
            auto frame = context.references.plane(ref);
            dir = vector(frame.u).Crossed(vector(frame.v)).Normalized();
        } else {
            auto a = context.references.axis(ref);
            dir = vector(a.direction);
        }
    } catch (...) {
        // Fallback default
    }
    if (dirMap.value("reverse").toBool())
        dir.Reverse();
    return dir.Normalized();
}

gp_Ax1 resolveAxis(const Context &context, const QCborMap &intent) {
    auto ref = domain::GeometryReference::parse(intent.value("axis").toMap());
    SupportAxis a;
    try {
        a = context.references.axis(ref);
    } catch (...) {
        a.direction = {0, 0, 1};
    }
    gp_Dir d(vector(a.direction));
    if (intent.value("reverse").toBool())
        d.Reverse();
    return gp_Ax1(kernelPoint(a.origin), d);
}

gp_Ax2 resolveMirrorPlane(const Context &context, const QCborMap &intent) {
    auto ref = domain::GeometryReference::parse(intent.value("plane").toMap());
    sketch::Frame frame;
    try {
        frame = context.references.plane(ref);
    } catch (...) {
        frame = sketch::Frame();
    }
    gp_Dir normal(vector(frame.u).Crossed(vector(frame.v)));
    return gp_Ax2(kernelPoint(frame.origin), normal);
}

TopoDS_Shape getSeed(const QCborMap &intent, const kernel::Part &part, QString operation) {
    auto seedKind = intent.value("seed_kind").toString();
    if (seedKind == "Feature") {
        auto seedId = intent.value("seed").toString();
        if (operation == "Cut" && part.tools.contains(seedId))
            return part.tools.value(seedId);
        if (part.stages.contains(seedId))
            return part.stages.value(seedId);
        if (part.tools.contains(seedId))
            return part.tools.value(seedId);
        throw domain::Error("ReferenceMissing", "Seed feature shape missing: " + seedId);
    } else if (seedKind == "Body") {
        auto bodyId = intent.value("seed").toMap().value("body").toString();
        for (const auto &b : part.bodies)
            if (b.id == bodyId)
                return b.shape;
        throw domain::Error("ReferenceMissing", "Seed body missing: " + bodyId);
    } else {
        // Face seeds require closed tool volume (R4)
        throw domain::Error("UnsupportedOperation",
                            "Face pattern requires a closed tool body (R4)");
    }
}
} // namespace

TopoDS_Shape pattern(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                     kernel::Part &part, QString owner) {
    auto kind = intent.value("kind").toString();
    auto op = intent.value("operation").toString();
    auto seed = getSeed(intent, part, op);

    std::vector<gp_Trsf> transforms;

    if (kind == "LinearPattern") {
        auto d1Map = intent.value("direction1").toMap();
        auto dir1 = resolveDirection(context, d1Map);
        double s1 = d1Map.value("spacing").toDouble() * mm;
        int n1 = int(d1Map.value("count").toInteger());

        gp_Vec dir2(0, 0, 0);
        double s2 = 0;
        int n2 = 1;
        if (!intent.value("direction2").isNull()) {
            auto d2Map = intent.value("direction2").toMap();
            dir2 = resolveDirection(context, d2Map);
            s2 = d2Map.value("spacing").toDouble() * mm;
            n2 = int(d2Map.value("count").toInteger());
        }

        for (int i = 0; i < n1; ++i) {
            for (int j = 0; j < n2; ++j) {
                if (i == 0 && j == 0)
                    continue; // Skip seed itself
                gp_Trsf t;
                t.SetTranslation(dir1 * (s1 * i) + dir2 * (s2 * j));
                transforms.push_back(t);
            }
        }
    } else {
        // CircularPattern
        auto axis = resolveAxis(context, intent);
        int n = int(intent.value("count").toInteger());
        bool equal = intent.value("equal_spacing").toBool();
        double angle = intent.value("angle").toDouble();
        double step = equal ? (angle / n) : angle;

        for (int i = 1; i < n; ++i) {
            gp_Trsf t;
            t.SetRotation(axis, step * i);
            transforms.push_back(t);
        }
    }

    std::vector<TopoDS_Shape> copies;
    TopoDS_Shape current = target;

    for (const auto &t : transforms) {
        BRepBuilderAPI_Transform moved(seed, t, true);
        auto copy = moved.Shape();
        kernel::recordShapeHistory(part, owner, moved, seed);
        copies.push_back(copy);

        if (op == "Cut") {
            BRepAlgoAPI_Cut cut(current, copy);
            cut.Build();
            check(cut.IsDone() && BRepAlgoAPI_Check(cut.Shape()).IsValid(), "InvalidGeometry",
                  "Pattern cut failed for instance");
            kernel::recordShapeHistory(part, owner, cut, current);
            kernel::recordShapeHistory(part, owner, cut, copy);
            current = cut.Shape();
        } else if (op == "Add") {
            BRepAlgoAPI_Fuse fuse(current, copy);
            fuse.Build();
            check(fuse.IsDone() && BRepAlgoAPI_Check(fuse.Shape()).IsValid(), "InvalidGeometry",
                  "Pattern fuse failed for instance");
            kernel::recordShapeHistory(part, owner, fuse, current);
            kernel::recordShapeHistory(part, owner, fuse, copy);
            current = fuse.Shape();
        } else {
            // NewBody
            part.bodies.push_back({QUuid::createUuid().toString(QUuid::WithoutBraces), copy});
        }
    }

    if (!copies.empty()) {
        TopoDS_Shape combined = copies[0];
        for (size_t i = 1; i < copies.size(); ++i) {
            BRepAlgoAPI_Fuse f(combined, copies[i]);
            f.Build();
            combined = f.Shape();
        }
        part.tools[owner] = combined;
    }

    return current;
}

TopoDS_Shape mirror(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                    kernel::Part &part, QString owner) {
    auto op = intent.value("operation").toString();
    auto seed = getSeed(intent, part, op);
    auto mirrorPlane = resolveMirrorPlane(context, intent);

    gp_Trsf t;
    t.SetMirror(mirrorPlane);

    BRepBuilderAPI_Transform moved(seed, t, true);
    auto copy = moved.Shape();
    kernel::recordShapeHistory(part, owner, moved, seed);
    part.tools[owner] = copy;

    if (op == "Cut") {
        BRepAlgoAPI_Cut cut(target, copy);
        cut.Build();
        check(cut.IsDone() && BRepAlgoAPI_Check(cut.Shape()).IsValid(), "InvalidGeometry",
              "Mirror cut failed");
        kernel::recordShapeHistory(part, owner, cut, target);
        kernel::recordShapeHistory(part, owner, cut, copy);
        return cut.Shape();
    } else if (op == "Add") {
        BRepAlgoAPI_Fuse fuse(target, copy);
        fuse.Build();
        check(fuse.IsDone() && BRepAlgoAPI_Check(fuse.Shape()).IsValid(), "InvalidGeometry",
              "Mirror fuse failed");
        kernel::recordShapeHistory(part, owner, fuse, target);
        kernel::recordShapeHistory(part, owner, fuse, copy);
        return fuse.Shape();
    } else {
        part.bodies.push_back({QUuid::createUuid().toString(QUuid::WithoutBraces), copy});
        return target.IsNull() ? copy : target;
    }
}

} // namespace cad::geometry::modeling

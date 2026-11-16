#include "Repetition.hpp"
#include "SupportReferences.hpp"
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
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
    if (dirMap.value("reverse").toBool())
        dir.Reverse();
    return dir.Normalized();
}

gp_Ax1 resolveAxis(const Context &context, const QCborMap &intent) {
    auto ref = domain::GeometryReference::parse(intent.value("axis").toMap());
    auto a = context.references.axis(ref);
    gp_Dir d(vector(a.direction));
    if (intent.value("reverse").toBool())
        d.Reverse();
    return gp_Ax1(kernelPoint(a.origin), d);
}

gp_Ax2 resolveMirrorPlane(const Context &context, const QCborMap &intent) {
    auto ref = domain::GeometryReference::parse(intent.value("plane").toMap());
    auto frame = context.references.plane(ref);
    gp_Dir normal(vector(frame.u).Crossed(vector(frame.v)));
    return gp_Ax2(kernelPoint(frame.origin), normal);
}

TopoDS_Shape getSeed(const QCborMap &intent, const kernel::Part &part, const Context &context) {
    auto seedKind = intent.value("seed_kind").toString();
    if (seedKind == "Feature") {
        auto seedId = intent.value("seed").toString();
        if (part.tools.contains(seedId))
            return part.tools.value(seedId);

        throw domain::Error("ReferenceMissing", "Seed feature shape missing: " + seedId);
    } else if (seedKind == "Body") {
        return body(context, intent.value("seed").toMap()).shape;
    } else if (seedKind == "Face") {
        check(intent.value("seed").isArray(), "InvalidInput", "Face seeds must be an array");
        auto array = intent.value("seed").toArray();
        check(!array.isEmpty(), "InvalidInput", "At least one face seed required");
        BRepBuilderAPI_Sewing sewing(1e-7);
        for (auto value : array)
            sewing.Add(
                context.references.topology(domain::GeometryReference::parse(value.toMap())));
        sewing.Perform();
        auto shell = sewing.SewedShape();
        check(!shell.IsNull() && shell.ShapeType() == TopAbs_SHELL && BRep_Tool::IsClosed(shell),
              "InvalidGeometry", "Face seeds must enclose one closed tool body");
        BRepBuilderAPI_MakeSolid makeSolid(TopoDS::Shell(shell));
        check(makeSolid.IsDone(), "InvalidGeometry", "Face seed solid construction failed");
        auto solid = makeSolid.Solid();
        check(BRepLib::OrientClosedSolid(solid) && BRepCheck_Analyzer(solid).IsValid(),
              "InvalidGeometry", "Face seed is not a valid closed tool body");
        kernel::validateShape(solid);
        return solid;
    } else {
        throw domain::Error("InvalidInput", "Unsupported seed kind: " + seedKind);
    }
}
} // namespace

TopoDS_Shape pattern(const QCborMap &intent, const Context &context, kernel::Part &part,
                     QString owner, QStringList &identities, const exchange::Control &control) {
    auto kind = intent.value("kind").toString();
    auto seed = getSeed(intent, part, context);

    std::vector<gp_Trsf> transforms;
    QStringList indices;

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
                indices.append(QString("%1/%2").arg(i).arg(j));
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
            indices.append(QString("%1/0").arg(i));
        }
    }

    int solids = 0;
    for (TopExp_Explorer ex(seed, TopAbs_SOLID); ex.More(); ex.Next())
        ++solids;
    bool newBody = intent.value("operation").toString() == "NewBody";
    if (newBody)
        check(part.bodies.size() + transforms.size() * solids <= 10, "ResourceLimit",
              "Part admits ten bodies; reduce the pattern count");
    check(!transforms.empty(), "InvalidInput", "Pattern needs at least one copied instance");
    TopoDS_Shape combined;
    TopoDS_Compound copies;
    BRep_Builder builder;
    builder.MakeCompound(copies);
    size_t instance = 0;
    for (const auto &t : transforms) {
        control.checkpoint("Transforming pattern instance", 50);
        BRepBuilderAPI_Transform moved(seed, t, true);
        check(moved.IsDone(), "InvalidGeometry", "Pattern transform failed");
        kernel::recordShapeHistory(part, owner, moved, seed);
        if (newBody) {
            int solid = 0;
            for (TopExp_Explorer ex(moved.Shape(), TopAbs_SOLID); ex.More(); ex.Next()) {
                builder.Add(copies, ex.Current());
                identities.append(
                    QUuid::createUuidV5(QUuid(owner), ("instance/" + indices[int(instance)] +
                                                       "/solid/" + QString::number(solid++))
                                                          .toUtf8())
                        .toString(QUuid::WithoutBraces));
            }
        } else
            combined = join(combined, moved.Shape(), part, owner);
        ++instance;
    }
    return newBody ? TopoDS_Shape(copies) : combined;
}

TopoDS_Shape mirror(const QCborMap &intent, const Context &context, kernel::Part &part,
                    QString owner, QStringList &identities) {
    auto seed = getSeed(intent, part, context);
    auto mirrorPlane = resolveMirrorPlane(context, intent);

    gp_Trsf t;
    t.SetMirror(mirrorPlane);

    BRepBuilderAPI_Transform moved(seed, t, true);
    auto copy = moved.Shape();
    kernel::recordShapeHistory(part, owner, moved, seed);
    if (intent.value("operation").toString() == "NewBody") {
        int solid = 0;
        for (TopExp_Explorer ex(copy, TopAbs_SOLID); ex.More(); ex.Next())
            identities.append(
                QUuid::createUuidV5(QUuid(owner),
                                    ("instance/1/0/solid/" + QString::number(solid++)).toUtf8())
                    .toString(QUuid::WithoutBraces));
        check(part.bodies.size() + identities.size() <= 10, "ResourceLimit",
              "Part admits ten bodies");
    }
    return copy;
}

} // namespace cad::geometry::modeling

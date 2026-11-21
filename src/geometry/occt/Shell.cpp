#include "Shell.hpp"
#include "SupportReferences.hpp"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

namespace cad::geometry::modeling {
using domain::check;
constexpr double mm = 1000.0;

namespace {
TopoDS_Face resolveFace(const TopoDS_Shape &target, const domain::GeometryReference &ref,
                        const Context &context) {
    auto shape = context.references.topology(ref);
    check(!shape.IsNull() && shape.ShapeType() == TopAbs_FACE, "ReferenceMissing",
          "Selected face is missing");
    for (TopExp_Explorer face(target, TopAbs_FACE); face.More(); face.Next())
        if (face.Current().IsSame(shape) || supportToken(face.Current()) == ref.topology)
            return TopoDS::Face(face.Current());
    throw domain::Error("ReferenceMissing", "Reference face is not in the target body");
}

void recordShellHistory(kernel::Part &part, QString id, BRepOffset_MakeOffset &op,
                        const TopoDS_Shape &input) {
    for (auto kind : {TopAbs_FACE, TopAbs_EDGE}) {
        TopTools_IndexedMapOfShape shapes;
        TopExp::MapShapes(input, kind, shapes);
        for (int i = 1; i <= shapes.Extent(); ++i) {
            auto shape = shapes(i);
            auto key = supportToken(shape);
            QCborArray gen, mod;
            for (auto &g : op.Generated(shape))
                if (g.ShapeType() == TopAbs_FACE || g.ShapeType() == TopAbs_EDGE)
                    gen.append(supportToken(g));
            for (auto &m : op.Modified(shape))
                if (m.ShapeType() == TopAbs_FACE || m.ShapeType() == TopAbs_EDGE)
                    mod.append(supportToken(m));
            bool deleted = op.IsDeleted(shape);
            if (gen.empty() && !deleted && mod.size() == 1 && mod[0].toString() == key)
                continue;
            if (!gen.empty() || !mod.empty() || deleted)
                part.history.append(sketch::fields({{"feature", id},
                                                    {"input", key},
                                                    {"generated", gen},
                                                    {"modified", mod},
                                                    {"deleted", deleted}}));
        }
    }
}
} // namespace

TopoDS_Shape shell(const TopoDS_Shape &target, const QCborMap &intent, const Context &context,
                   kernel::Part &part, QString owner) {
    try {
        double thickness = intent.value("thickness").toDouble();
        bool inward = intent.value("inward").toBool();
        double offsetVal = inward ? -thickness * mm : thickness * mm;

        BRepOffset_MakeOffset shellMaker;
        shellMaker.Initialize(target, offsetVal, 1e-7, BRepOffset_Skin, false, false,
                              GeomAbs_Intersection);

        auto removeFaces = intent.value("remove_faces").toArray();
        for (auto faceVal : removeFaces) {
            auto r = domain::GeometryReference::parse(faceVal.toMap());
            auto face = resolveFace(target, r, context);
            shellMaker.AddFace(face);
        }

        for (auto ovVal : intent.value("overrides").toArray()) {
            auto ovMap = ovVal.toMap();
            auto r = domain::GeometryReference::parse(ovMap.value("face").toMap());
            auto face = resolveFace(target, r, context);
            double ovThick = ovMap.value("thickness").toDouble();
            double faceOffset = inward ? -ovThick * mm : ovThick * mm;
            shellMaker.SetOffsetOnFace(face, faceOffset);
        }

        TopoDS_Shape resultShape;
        if (removeFaces.isEmpty()) {
            shellMaker.MakeOffsetShape();
            check(shellMaker.IsDone(), "InvalidGeometry", "Closed shell offset generation failed");
            auto offsetShape = shellMaker.Shape();
            check(!offsetShape.IsNull() && BRepCheck_Analyzer(offsetShape, true).IsValid(),
                  "InvalidGeometry", "Offset cavity shape is invalid");
            if (inward) {
                BRepAlgoAPI_Cut cut(target, offsetShape);
                cut.Build();
                check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry",
                      "Hollow cavity subtraction failed");
                resultShape = cut.Shape();
            } else {
                BRepAlgoAPI_Cut cut(offsetShape, target);
                cut.Build();
                check(cut.IsDone() && !cut.HasErrors(), "InvalidGeometry",
                      "Hollow cavity subtraction failed");
                resultShape = cut.Shape();
            }
        } else {
            shellMaker.MakeThickSolid();
            check(shellMaker.IsDone(), "InvalidGeometry",
                  "Shell operation failed (excessive thickness or self-intersection)");
            resultShape = shellMaker.Shape();
        }

        check(!resultShape.IsNull() && BRepCheck_Analyzer(resultShape, true).IsValid(),
              "InvalidGeometry", "Resulting shell shape is invalid");
        int solids = 0;
        for (TopExp_Explorer solid(resultShape, TopAbs_SOLID); solid.More(); solid.Next())
            ++solids;
        check(solids == 1, "InvalidGeometry",
              "Shell operation must produce exactly one solid body");
        recordShellHistory(part, owner, shellMaker, target);
        return resultShape;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Shell exact evaluation failed: ") + e.GetMessageString());
    }
}

} // namespace cad::geometry::modeling

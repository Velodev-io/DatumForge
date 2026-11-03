#include "ModelingFeatures.hpp"
#include "ModelingInternal.hpp"
#include "ModelingProfiles.hpp"
#include "features/ModelingIntent.hpp"
#include "sketch/Dependencies.hpp"
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
namespace cad::geometry {
using domain::check;
kernel::Body modeling::body(const Context &context, const QCborMap &selection) {
    auto r = domain::GeometryReference::parse(selection.value("source").toMap());
    check(r.role == "Object" && (r.document == "Self" || r.document == context.references.document),
          "UnsupportedOperation", "Body selection must be local to this document");
    auto id = selection.value("body").toString();
    domain::id(id);
    check(context.bodyStages.contains(r.object), "ReferenceMissing",
          "Producing body stage is missing");
    for (auto &body : context.bodyStages.value(r.object))
        if (body.id == id)
            return body;
    throw domain::Error("ReferenceMissing", "Selected body does not belong to the producing stage");
}
namespace {
QString bodyId(QString owner, int ordinal) {
    return ordinal == 0 ? owner
                        : QUuid::createUuidV5(QUuid(owner), QByteArray::number(ordinal))
                              .toString(QUuid::WithoutBraces);
}
void publish(kernel::Part &part, modeling::Context &context, const QCborMap &m,
             const TopoDS_Shape &tool) {
    auto id = m.value("id").toString();
    kernel::validateShape(tool);
    std::vector<kernel::Body> changed;
    if (m.value("operation").toString() == "NewBody") {
        int ordinal = 0;
        for (TopExp_Explorer solid(tool, TopAbs_SOLID); solid.More(); solid.Next()) {
            auto name = bodyId(id, ordinal++);
            check(std::none_of(part.bodies.begin(), part.bodies.end(),
                               [&](auto &b) { return b.id == name; }),
                  "InvalidInput", "Generated body identity collision");
            kernel::Body b{name, solid.Current()};
            part.bodies.push_back(b);
            changed.push_back(b);
        }
        part.stages[id] = tool;
    } else {
        auto selected = modeling::body(context, m.value("target").toMap());
        auto current = std::find_if(part.bodies.begin(), part.bodies.end(),
                                    [&](auto &b) { return b.id == selected.id; });
        check(current != part.bodies.end() && selected.shape.IsSame(current->shape),
              "ReferenceMissing", "Writable body selection is not the current target stage");
        double before = kernel::shapeVolume(current->shape);
        auto boolean = [&](auto &op) {
            TopTools_ListOfShape arguments, tools;
            arguments.Append(current->shape);
            tools.Append(tool);
            op.SetArguments(arguments);
            op.SetTools(tools);
            op.SetNonDestructive(true);
            op.Build();
            check(op.IsDone() && !op.HasErrors(), "InvalidGeometry", "Exact Boolean failed");
            kernel::validateShape(op.Shape());
            int solids = 0;
            for (TopExp_Explorer solid(op.Shape(), TopAbs_SOLID); solid.More(); solid.Next())
                ++solids;
            double after = kernel::shapeVolume(op.Shape());
            check(solids == 1 && (m.value("operation").toString() == "Cut" ? after < before - 1e-9
                                                                           : after > before + 1e-9),
                  "InvalidGeometry", "Boolean misses, removes, splits or disconnects the target");
            kernel::recordShapeHistory(part, id, op, current->shape);
            kernel::recordShapeHistory(part, id, op, tool);
            current->shape = op.Shape();
        };
        if (m.value("operation").toString() == "Cut") {
            BRepAlgoAPI_Cut op;
            boolean(op);
        } else {
            BRepAlgoAPI_Fuse op;
            boolean(op);
        }
        changed.push_back(*current);
        part.stages[id] = current->shape;
    }
    check(part.bodies.size() <= 10, "ResourceLimit", "Part admits ten bodies");
    context.bodyStages[id] = std::move(changed);
    part.tools[id] = tool;
}
std::vector<evaluation::GraphNode> baseNodes(const model::SupportDefinition &d,
                                             const SupportResult &result) {
    auto graph = d.graph();
    auto sketches = sketch::dependencyNodes(result.sketches);
    QMap<QString, QString> kinds;
    for (auto &s : sketches)
        kinds[s.id] = "Sketch";
    for (auto f : features::parse(d.features))
        kinds[f.id] = "Feature";
    for (auto &datum : d.datums)
        for (auto id : datum.outputs())
            kinds[id] = datum.kind;
    for (auto &c : d.conversions)
        kinds[c.id] = "DerivedCurve";
    std::vector<evaluation::GraphNode> nodes;
    for (auto id : d.history) {
        evaluation::GraphNode node{id, kinds.value(id), {}, {}};
        for (auto &s : sketches)
            if (s.id == id)
                node.members = s.members;
        for (auto parent : graph.dependencies(id)) {
            domain::GeometryReference r;
            r.object = parent;
            node.references.push_back(r);
        }
        nodes.push_back(std::move(node));
    }
    return nodes;
}
TopoDS_Shape revolve(const ModelingProfile &profile, const QCborMap &m,
                     const modeling::Context &context, kernel::Part &part) {
    check(!profile.open, "UnsupportedOperation", "Open revolve requires a thin section");
    auto a = context.references.axis(domain::GeometryReference::parse(m.value("axis").toMap()));
    auto origin = kernelPoint(a.origin);
    auto direction = vector(a.direction);
    check(std::abs(gp_Vec(profile.plane.Axis().Direction()).Dot(direction)) < 1e-10 &&
              profile.plane.Distance(origin) <= 1e-7,
          "InvalidInput", "Revolve axis must be straight and coplanar with the profile");
    gp_Ax1 axis(origin, gp_Dir(direction));
    auto turn = [&](QCborMap end, double sign) {
        double angle =
            end.value("value").toDouble() * sign * (m.value("reverse").toBool() ? -1 : 1);
        auto face = profile.shape;
        if (end.value("mode").toString() == "Midplane") {
            gp_Trsf rotation;
            rotation.SetRotation(axis, -angle / 2);
            face = BRepBuilderAPI_Transform(face, rotation, true).Shape();
        }
        BRepPrimAPI_MakeRevol make(face, axis, angle, true);
        check(make.IsDone() && BRepAlgoAPI_Check(make.Shape()).IsValid(), "InvalidGeometry",
              "Revolve failed or self-intersects");
        kernel::recordShapeHistory(part, m.value("id").toString(), make, face);
        return make.Shape();
    };
    auto result = turn(m.value("direction1").toMap(), 1);
    if (!m.value("direction2").isNull())
        result = modeling::join(result, turn(m.value("direction2").toMap(), -1), part,
                                m.value("id").toString());
    return result;
}
} // namespace
SupportResult evaluateModeling(const model::SupportDefinition &d, const QCborArray &definitions,
                               const SupportBuild &build, const exchange::Control &control) {
    auto combined = d.features;
    for (auto value : definitions)
        combined.append(value);
    check(combined.size() <= 100 && QCborValue(combined).toCbor().size() <= 131072, "ResourceLimit",
          "Combined feature definitions exceed 100 / 128 KiB admission");
    check(definitions.size() + d.features.size() <= 100 &&
              QCborValue(sketch::fields({{"model", d.map()}, {"intents", definitions}}))
                      .toCbor()
                      .size() <= 262144,
          "ResourceLimit", "Modeling preview exceeds existing feature/control admission");
    std::vector<features::ModelingIntent> intents;
    for (auto value : definitions) {
        check(value.isMap(), "InvalidInput", "Versioned modeling intent required");
        intents.push_back(features::ModelingIntent::parse(value.toMap()));
    }
    control.checkpoint("Preparing exact modeling preview", 0);
    auto result = evaluateSupports(d, build, control);
    auto nodes = baseNodes(d, result);
    QSet<QString> identities;
    QMap<QString, sketch::Sketch> sketches;
    for (auto &s : result.sketches) {
        sketches.insert(s.id, s);
        for (auto &entity : s.entities)
            identities.insert(entity.id);
        for (auto &c : s.constraints)
            identities.insert(c.id);
        for (auto &g : s.groups)
            identities.insert(g.id);
        for (auto edit : s.edits)
            identities.insert(edit.toMap().value("id").toString());
    }
    for (auto &intent : intents) {
        auto node = intent.graphNode();
        check(!identities.contains(node.id), "InvalidInput",
              "Modeling identity collides with an existing member");
        identities.insert(node.id);
        nodes.push_back(node);
        result.order.append(node.id);
    }
    for (size_t i = 0; i < intents.size(); ++i) {
        auto upstream = nodes;
        upstream.erase(upstream.begin() + (nodes.size() - intents.size() + i));
        intents[i].graph(upstream, result.order, d.document);
    }
    modeling::Context context;
    context.references.document = d.document;
    context.references.sketches = sketches;
    context.references.planes = result.planes;
    context.references.axes = result.axes;
    for (auto value : result.conversions) {
        auto conversion = value.toMap();
        auto sketchId = conversion.value("sketch").toString();
        check(sketches.contains(sketchId), "ReferenceMissing",
              "Converted axis source sketch is missing");
        const auto &s = sketches.constFind(sketchId).value();
        std::vector<SupportCurve> curves;
        for (auto identity : conversion.value("entities").toArray()) {
            auto &entity = s.entity(identity.toString());
            if (entity.kind == "Point")
                continue;
            auto exact = sketchCurve(s, entity);
            curves.push_back(
                {entity.id, exact.curve, exact.first, exact.last, exact.parameterScale, false, {}});
        }
        context.references.derived[conversion.value("id").toString()] = std::move(curves);
    }
    context.references.stages = result.part.stages;
    context.references.history = result.part.history;
    for (auto f : features::parse(d.features)) {
        if (!result.part.stages.contains(f.id))
            continue;
        auto stage = result.part.stages.value(f.id);
        if (f.parameters.value("operation").toString() == "NewBody") {
            int ordinal = 0;
            for (TopExp_Explorer solid(stage, TopAbs_SOLID); solid.More(); solid.Next())
                context.bodyStages[f.id].push_back({bodyId(f.id, ordinal++), solid.Current()});
        } else
            context.bodyStages[f.id].push_back({f.parameters.value("target").toString(), stage});
    }
    for (qsizetype i = 0; i < result.order.size(); ++i)
        context.references.stageOrder[result.order[i]] = i;
    try {
        for (size_t i = 0; i < intents.size(); ++i) {
            control.checkpoint("Evaluating exact solid intent", int(i * 100 / intents.size()));
            auto m = intents[i].map();
            auto id = m.value("id").toString();
            check(m.value("kind").toString() == "Extrude" ||
                      m.value("kind").toString() == "Revolve" ||
                      m.value("kind").toString() == "Draft",
                  "UnsupportedOperation", "Extrude/revolve adapter required");
            TopoDS_Shape target;
            if (m.value("operation").toString() != "NewBody") {
                auto selected = modeling::body(context, m.value("target").toMap());
                auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                            [&](auto &b) { return b.id == selected.id; });
                check(current != result.part.bodies.end() && selected.shape.IsSame(current->shape),
                      "ReferenceMissing",
                      "Writable body selection is not the current target stage");
                target = current->shape;
            }
            if (m.value("kind").toString() == "Draft") {
                std::vector<TopoDS_Face> faces;
                for (auto value : m.value("faces").toArray())
                    faces.push_back(TopoDS::Face(context.references.topology(
                        domain::GeometryReference::parse(value.toMap()))));
                auto frame = context.references.plane(
                    domain::GeometryReference::parse(m.value("neutral").toMap()));
                auto axis = context.references.axis(
                    domain::GeometryReference::parse(m.value("pull").toMap()));
                auto shape = modeling::draft(
                    target, faces, gp_Dir(vector(axis.direction)),
                    gp_Pln(kernelPoint(frame.origin),
                           gp_Dir(vector(frame.u).Crossed(vector(frame.v)))),
                    m.value("angle").toDouble(), m.value("outward").toBool(), result.part, id);
                auto selected = m.value("target").toMap().value("body").toString();
                auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                            [&](auto &b) { return b.id == selected; });
                check(current != result.part.bodies.end(), "ReferenceMissing",
                      "Draft target disappeared");
                current->shape = shape;
                result.part.stages[id] = shape;
                context.bodyStages[id] = {*current};
            } else {
                auto selection = m.value("profile").toMap();
                check(sketches.contains(selection.value("sketch").toString()), "ReferenceMissing",
                      "Profile sketch missing");
                auto profile = modelingProfile(sketches.value(selection.value("sketch").toString()),
                                               selection, m.value("thin").toMap());
                auto tool =
                    m.value("kind").toString() == "Revolve"
                        ? revolve(profile, m, context, result.part)
                        : modeling::extrude(profile, m, context, target, result.part, control);
                publish(result.part, context, m, tool);
            }
            context.references.stages = result.part.stages;
            context.references.history = result.part.history;
            result.part.diagnostics.append(sketch::fields(
                {{"id", id}, {"status", "Current"}, {"valid", true}, {"reused", false}}));
        }
        control.checkpoint("Exact modeling preview ready", 100);
        result.map();
        return result;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Exact solid intent failed: ") + e.GetMessageString());
    }
}
} // namespace cad::geometry

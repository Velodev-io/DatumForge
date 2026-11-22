#include "ModelingFeatures.hpp"
#include "Holes.hpp"
#include "Loft.hpp"
#include "ModelingInternal.hpp"
#include "ModelingProfiles.hpp"
#include "Repetition.hpp"
#include "Ribs.hpp"
#include "Shell.hpp"
#include "Sweep.hpp"
#include "Treatments.hpp"
#include "features/Catalogs.hpp"
#include "features/ModelingIntent.hpp"
#include "features/ModelingRecords.hpp"
#include "model/DerivedCurve.hpp"
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
             const TopoDS_Shape &tool, const QStringList &identities = {}) {
    auto id = m.value("id").toString();
    kernel::validateShape(tool);
    std::vector<kernel::Body> changed;
    if (m.value("operation").toString() == "NewBody") {
        int ordinal = 0;
        for (TopExp_Explorer solid(tool, TopAbs_SOLID); solid.More(); solid.Next()) {
            auto name = identities.empty() ? bodyId(id, ordinal) : identities.value(ordinal);
            ++ordinal;
            domain::id(name);
            check(std::none_of(part.bodies.begin(), part.bodies.end(),
                               [&](auto &b) { return b.id == name; }),
                  "InvalidInput", "Generated body identity collision");
            kernel::Body b{name, solid.Current()};
            part.bodies.push_back(b);
            changed.push_back(b);
        }
        if (ordinal == 0) {
            for (TopExp_Explorer shell(tool, TopAbs_SHELL); shell.More(); shell.Next()) {
                auto name = identities.empty() ? bodyId(id, ordinal) : identities.value(ordinal);
                ++ordinal;
                domain::id(name);
                check(std::none_of(part.bodies.begin(), part.bodies.end(),
                                   [&](auto &b) { return b.id == name; }),
                      "InvalidInput", "Generated body identity collision");
                kernel::Body b{name, shell.Current()};
                part.bodies.push_back(b);
                changed.push_back(b);
            }
            if (ordinal == 0) {
                for (TopExp_Explorer face(tool, TopAbs_FACE); face.More(); face.Next()) {
                    auto name =
                        identities.empty() ? bodyId(id, ordinal) : identities.value(ordinal);
                    ++ordinal;
                    domain::id(name);
                    check(std::none_of(part.bodies.begin(), part.bodies.end(),
                                       [&](auto &b) { return b.id == name; }),
                          "InvalidInput", "Generated body identity collision");
                    kernel::Body b{name, face.Current()};
                    part.bodies.push_back(b);
                    changed.push_back(b);
                }
            }
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
                               const SupportBuild &build, const exchange::Control &control,
                               QString inspectOwner, QString inspectPath, int inspectSlot) {
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
    auto nodes = d.nodes();
    QMap<QString, qsizetype> priorities;
    for (qsizetype i = 0; i < d.history.size(); ++i)
        priorities.insert(d.history[i], i);
    std::stable_sort(nodes.begin(), nodes.end(), [&](const auto &a, const auto &b) {
        return priorities.value(a.id) < priorities.value(b.id);
    });

    QString previous;
    auto keepOrder = [&](evaluation::GraphNode &node) {
        if (!previous.isEmpty()) {
            domain::GeometryReference predecessor;
            predecessor.object = previous;
            node.references.push_back(predecessor);
        }
        previous = node.id;
    };
    for (auto &node : nodes)
        if (node.kind == "Feature")
            keepOrder(node);
    std::vector<features::ModelingIntent> intents;
    QMap<QString, QCborMap> byId;
    for (auto value : definitions) {
        check(value.isMap(), "InvalidInput", "Versioned modeling intent required");
        auto intent = features::ModelingIntent::parse(value.toMap());
        auto node = intent.graphNode();
        byId.insert(node.id, intent.map());
        keepOrder(node);
        nodes.push_back(std::move(node));
        intents.push_back(std::move(intent));
    }
    evaluation::Graph graph(nodes, d.history, d.document);
    const auto order = graph.order();
    for (qsizetype i = 0; i < qsizetype(intents.size()); ++i) {
        auto upstream = nodes;
        upstream.erase(upstream.begin() + (nodes.size() - intents.size() + i));
        intents[i].graph(upstream, order, d.document);
    }

    modeling::Context context;
    bool initialized = false;
    auto evaluateIntent = [&](const QString &id, SupportResult &result, SupportReferences &scene) {
        context.references = scene;
        auto &sketches = context.references.sketches;
        if (!initialized) {
            for (const auto &feature : features::parse(d.features)) {
                if (!result.part.stages.contains(feature.id))
                    continue;
                auto stage = result.part.stages.value(feature.id);
                if (feature.parameters.value("operation") == "NewBody") {
                    int ordinal = 0;
                    for (TopExp_Explorer solid(stage, TopAbs_SOLID); solid.More(); solid.Next())
                        context.bodyStages[feature.id].push_back(
                            {bodyId(feature.id, ordinal++), solid.Current()});
                } else {
                    context.bodyStages[feature.id].push_back(
                        {feature.parameters.value("target").toString(), stage});
                }
            }
            initialized = true;
        }
        const auto m = byId.value(id);
        if (id == inspectOwner) {
            result.inspection =
                context.references.inspect(features::intentReference(m, inspectPath));
            return true;
        }
        QCborArray provenance;
        auto kind = m.value("kind").toString();
        check(QStringList{"Extrude", "Revolve", "Draft", "Hole", "CosmeticThread", "Rib",
                          "LinearPattern", "CircularPattern", "Mirror", "Fillet", "Chamfer",
                          "Shell", "Sweep", "Loft"}
                  .contains(kind),
              "UnsupportedOperation", "Modeling adapter required");
        TopoDS_Shape target;
        if (m.value("operation").toString() != "NewBody") {
            auto selected = modeling::body(context, m.value("target").toMap());
            auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                        [&](auto &b) { return b.id == selected.id; });
            check(current != result.part.bodies.end() && selected.shape.IsSame(current->shape),
                  "ReferenceMissing", "Writable body selection is not the current target stage");
            target = current->shape;
        }
        auto historyStart = result.part.history.size();
        if (kind == "Draft") {
            std::vector<TopoDS_Face> faces;
            for (auto value : m.value("faces").toArray())
                faces.push_back(TopoDS::Face(
                    context.references.topology(domain::GeometryReference::parse(value.toMap()))));
            auto frame = context.references.plane(
                domain::GeometryReference::parse(m.value("neutral").toMap()));
            auto axis =
                context.references.axis(domain::GeometryReference::parse(m.value("pull").toMap()));
            auto shape = modeling::draft(
                target, faces, gp_Dir(vector(axis.direction)),
                gp_Pln(kernelPoint(frame.origin), gp_Dir(vector(frame.u).Crossed(vector(frame.v)))),
                m.value("angle").toDouble(), m.value("outward").toBool(), result.part, id);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
            auto selected = m.value("target").toMap().value("body").toString();
            auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                        [&](auto &b) { return b.id == selected; });
            check(current != result.part.bodies.end(), "ReferenceMissing",
                  "Draft target disappeared");
            current->shape = shape;
            result.part.stages[id] = shape;
            context.bodyStages[id] = {*current};
        } else if (kind == "Fillet") {
            auto shape = modeling::fillet(target, m, context, result.part, id);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
            auto selected = m.value("target").toMap().value("body").toString();
            auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                        [&](auto &b) { return b.id == selected; });
            current->shape = shape;
            result.part.stages[id] = shape;
            context.bodyStages[id] = {*current};
        } else if (kind == "Chamfer") {
            auto shape = modeling::chamfer(target, m, context, result.part, id);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
            auto selected = m.value("target").toMap().value("body").toString();
            auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                        [&](auto &b) { return b.id == selected; });
            current->shape = shape;
            result.part.stages[id] = shape;
            context.bodyStages[id] = {*current};
        } else if (kind == "Shell") {
            auto shape = modeling::shell(target, m, context, result.part, id);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
            auto selected = m.value("target").toMap().value("body").toString();
            auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                        [&](auto &b) { return b.id == selected; });
            check(current != result.part.bodies.end(), "ReferenceMissing",
                  "Shell target disappeared");
            current->shape = shape;
            result.part.stages[id] = shape;
            context.bodyStages[id] = {*current};
        } else if (kind == "Sweep") {
            auto tool = modeling::sweep(target, m, context, result.part, id);
            publish(result.part, context, m, tool);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
        } else if (kind == "Loft") {
            auto tool = modeling::loft(target, m, context, result.part, id);
            publish(result.part, context, m, tool);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
        } else if (kind == "Hole") {
            auto tool = modeling::hole(target, m, context, result.part, id, control);
            publish(result.part, context, m, tool);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
        } else if (kind == "CosmeticThread") {
            result.part.cosmeticThreads.append(modeling::cosmeticThread(m, context));
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
            result.part.stages[id] = target;
            auto selected = m.value("target").toMap().value("body").toString();
            auto current = std::find_if(result.part.bodies.begin(), result.part.bodies.end(),
                                        [&](auto &b) { return b.id == selected; });
            context.bodyStages[id] = {*current};
        } else if (kind == "Rib") {
            auto tool = modeling::rib(target, m, context, result.part, id, control);
            publish(result.part, context, m, tool);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
        } else if (kind == "LinearPattern" || kind == "CircularPattern" || kind == "Mirror") {
            QStringList identities;
            auto tool = kind == "Mirror"
                            ? modeling::mirror(m, context, result.part, id, identities)
                            : modeling::pattern(m, context, result.part, id, identities, control);
            publish(result.part, context, m, tool, identities);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
        } else {
            auto selection = m.value("profile").toMap();
            check(sketches.contains(selection.value("sketch").toString()), "ReferenceMissing",
                  "Profile sketch missing");
            auto profile = modelingProfile(sketches.value(selection.value("sketch").toString()),
                                           selection, m.value("thin").toMap());
            auto tool = m.value("kind").toString() == "Revolve"
                            ? revolve(profile, m, context, result.part)
                            : modeling::extrude(profile, m, context, target, result.part, control);
            publish(result.part, context, m, tool);
            for (qsizetype n = historyStart; n < result.part.history.size(); ++n)
                provenance.append(result.part.history[n]);
        }
        QCborArray outputs;
        for (auto &b : context.bodyStages.value(id))
            outputs.append(b.id);
        bool directOp = kind == "Draft" || kind == "Fillet" || kind == "Chamfer" ||
                        kind == "CosmeticThread" || kind == "Shell";
        result.modelingProvenance.append(sketch::fields(
            {{"feature", id},
             {"kind", m.value("kind")},
             {"operation", directOp ? QCborValue(m.value("kind")) : m.value("operation")},
             {"target", m.value("target")},
             {"outputs", outputs},
             {"history", provenance}}));
        scene.stages = result.part.stages;
        scene.history = result.part.history;
        scene.evolution = context.references.evolution;
        result.part.diagnostics.append(sketch::fields(
            {{"id", id}, {"status", "Current"}, {"valid", true}, {"reused", false}}));
        return false;
    };
    auto result = evaluateSupports(d, build, control, inspectOwner, inspectSlot, inspectPath, order,
                                   evaluateIntent);
    if (inspectOwner.isEmpty())
        features::validateModelingRecords(sketch::fields(
            {{"version", 1}, {"intents", definitions}, {"provenance", result.modelingProvenance}}));
    return result;
}
} // namespace cad::geometry

#include "Supports.hpp"
#include "Convert.hpp"
#include "SpatialCurve.hpp"
#include "features/Catalogs.hpp"
#include "model/DerivedCurve.hpp"
#include "sketch/Solver.hpp"
#include "sketch/SpatialSolver.hpp"
#include <Standard_Failure.hxx>
namespace cad::geometry {
using domain::check;
using sketch::fields;
sketch::Frame attachmentFrame(const model::Attachment &a, const SupportReferences &references) {
    a.map();
    auto f = references.plane(a.reference);
    auto u = vector(f.u), v = vector(f.v), normal = u.Crossed(v);
    f.origin = siPoint(kernelPoint(f.origin).Translated(normal * (a.offset * 1000)));
    f.u = coordinates(u * std::cos(a.angle) + v * std::sin(a.angle));
    f.v = coordinates(v * std::cos(a.angle) - u * std::sin(a.angle));
    if (a.reversed)
        f.v = coordinates(-vector(f.v));
    f.validate();
    return f;
}
SupportResult evaluateSupports(const model::SupportDefinition &d, const SupportBuild &build,
                               const exchange::Control &control, QString inspectOwner,
                               int inspectSlot, QString inspectPath) {
    d.map(); // Validate the complete graph/history and transport admission first.
    control.checkpoint("Preparing model supports", 0);
    auto base = sketch::parse(d.sketches);
    auto features = features::parse(d.features);
    SupportReferences scene;
    scene.document = d.document;
    QMap<QString, size_t> sketchIndex;
    QMap<QString, const features::Feature *> byFeature;
    QMap<QString, const model::Datum *> byDatum;
    QMap<QString, const model::Conversion *> byConversion;
    QMap<QString, const model::Attachment *> attachments;
    QSet<QString> identities;
    auto admit = [&](QString id) {
        check(!identities.contains(id), "InvalidInput", "Model semantic identity collision");
        identities.insert(id);
    };
    for (size_t i = 0; i < base.size(); ++i) {
        sketchIndex.insert(base[i].id, i);
        admit(base[i].id);
        for (auto &e : base[i].entities)
            admit(e.id);
        for (auto &c : base[i].constraints)
            admit(c.id);
        for (auto &g : base[i].groups)
            admit(g.id);
        for (auto edit : base[i].edits)
            admit(edit.toMap().value("id").toString());
    }
    for (auto &f : features) {
        byFeature.insert(f.id, &f);
        scene.stageOrder.insert(f.id, d.history.indexOf(f.id));
        admit(f.id);
    }
    for (auto &datum : d.datums) {
        if (!datum.outputs().contains(datum.id))
            admit(datum.id);
        for (auto id : datum.outputs()) {
            byDatum.insert(id, &datum);
            admit(id);
        }
    }
    for (auto &c : d.conversions) {
        byConversion.insert(c.id, &c);
        admit(c.id);
    }
    auto parsedSpatial = sketch::parseSpatial(d.spatial_sketches);
    QMap<QString, size_t> spatialIndex;
    for (size_t i = 0; i < parsedSpatial.size(); ++i) {
        spatialIndex.insert(parsedSpatial[i].id, i);
        admit(parsedSpatial[i].id);
        for (const auto &e : parsedSpatial[i].entities)
            admit(e.id);
        for (const auto &c : parsedSpatial[i].constraints)
            admit(c.id);
    }
    auto parsedDerived = model::parseDerivedCurves(d.derived_curves);
    QMap<QString, size_t> derivedIndex;
    for (size_t i = 0; i < parsedDerived.size(); ++i) {
        derivedIndex.insert(parsedDerived[i].id, i);
        admit(parsedDerived[i].id);
    }
    for (auto &a : d.attachments)
        attachments.insert(a.sketch, &a);
    kernel::ResultCache cache;
    auto evaluate = [&](const QCborArray &sketches, const QCborArray &prefix) {
        return build ? build(sketches, prefix) : kernel::Part::build(sketches, prefix, &cache);
    };
    std::vector<sketch::Sketch> processed;
    QMap<QString, sketch::Sketch> converted;
    QSet<QString> evaluatedDatums;
    QCborArray prefix;
    SupportResult result;
    result.order = d.history;
    try {
        for (qsizetype position = 0; position < d.history.size(); ++position) {
            control.checkpoint("Evaluating model supports", int(position * 100 / d.history.size()));
            auto id = d.history[position];
            bool inspectAttachment =
                attachments.contains(inspectOwner) &&
                (id == inspectOwner ||
                 (byConversion.contains(id) && byConversion.value(id)->sketch == inspectOwner));
            if (inspectAttachment || id == inspectOwner ||
                (byDatum.contains(id) && byDatum.value(id)->id == inspectOwner)) {
                if (derivedIndex.contains(inspectOwner) && id == inspectOwner) {
                    const auto &candidate = parsedDerived[derivedIndex.value(id)];
                    if (inspectPath == "branch" || (inspectPath.isEmpty() &&
                                                    (candidate.sketchOnSketch.has_value() ||
                                                     candidate.sketchOnFaces.has_value()) &&
                                                    inspectSlot == -1)) {
                        result.inspection = inspectBranchCandidates(candidate, scene);
                        return result;
                    }
                    if (candidate.sketchOnSketch.has_value()) {
                        int s = inspectPath == "sketch1"   ? 0
                                : inspectPath == "sketch2" ? 1
                                                           : inspectSlot;
                        check(s >= 0 && s < 2, "InvalidInput",
                              "Choose a valid sketch-on-sketch slot");
                        auto ref = s == 0 ? candidate.sketchOnSketch->sketch1
                                          : candidate.sketchOnSketch->sketch2;
                        result.inspection = scene.inspect(ref);
                        return result;
                    }
                    if (candidate.sketchOnFaces.has_value()) {
                        const auto &def = *candidate.sketchOnFaces;
                        if (inspectPath == "direction" && def.direction.has_value()) {
                            result.inspection = scene.inspect(*def.direction);
                            return result;
                        }
                        if (inspectPath == "sketch") {
                            result.inspection = scene.inspect(def.sketch);
                            return result;
                        }
                        if (inspectPath.startsWith("faces/")) {
                            auto fidx = size_t(inspectPath.section('/', 1, 1).toULongLong());
                            check(fidx < def.faces.size(), "InvalidInput",
                                  "Choose a valid face slot");
                            result.inspection = scene.inspect(def.faces[fidx]);
                            return result;
                        }
                        check(inspectSlot <= int(def.faces.size()) + (def.direction ? 1 : 0),
                              "InvalidInput", "Choose a valid slot");
                        auto ref = inspectSlot == 0 ? def.sketch
                                   : inspectSlot <= int(def.faces.size())
                                       ? def.faces[size_t(inspectSlot - 1)]
                                       : *def.direction;
                        result.inspection = scene.inspect(ref);
                        return result;
                    }
                    if (candidate.compositeCurve.has_value()) {
                        const auto &def = *candidate.compositeCurve;
                        size_t memIdx = inspectPath.startsWith("members/")
                                            ? size_t(inspectPath.section('/', 1, 1).toULongLong())
                                            : size_t(inspectSlot);
                        check(memIdx < def.members.size(), "InvalidInput",
                              "Choose a valid member slot");
                        result.inspection = scene.inspect(def.members[memIdx].source);
                        return result;
                    }
                    if (candidate.helixSpiral.has_value()) {
                        const auto &def = *candidate.helixSpiral;
                        int s = inspectPath == "axis"     ? 0
                                : inspectPath == "center" ? 1
                                                          : inspectSlot;
                        check(s == 0 || (s == 1 && def.center.has_value()), "InvalidInput",
                              "Choose a valid slot");
                        auto ref = s == 0 ? def.axis : *def.center;
                        result.inspection = scene.inspect(ref);
                        return result;
                    }
                }
                if (spatialIndex.contains(inspectOwner) && id == inspectOwner) {
                    const auto &candidate = parsedSpatial[spatialIndex.value(id)];
                    if (inspectPath.startsWith("constraints/")) {
                        auto cid = inspectPath.section('/', 1, 1);
                        for (const auto &cn : candidate.constraints) {
                            if (cn.id == cid && cn.plane.has_value()) {
                                result.inspection = scene.inspect(*cn.plane);
                                return result;
                            }
                        }
                        throw domain::Error("ReferenceMissing",
                                            "Constraint plane reference not found: " + cid);
                    }
                    if (inspectSlot >= 0 && size_t(inspectSlot) < candidate.constraints.size()) {
                        check(candidate.constraints[size_t(inspectSlot)].plane.has_value(),
                              "InvalidInput", "Constraint does not have a plane reference");
                        result.inspection =
                            scene.inspect(*candidate.constraints[size_t(inspectSlot)].plane);
                        return result;
                    }
                }
                const std::vector<domain::GeometryReference> *operands = nullptr;
                std::vector<domain::GeometryReference> attached;
                if (inspectAttachment) {
                    attached.push_back(attachments.value(inspectOwner)->reference);
                    operands = &attached;
                } else if (byDatum.contains(id))
                    operands = &byDatum.value(id)->references;
                else if (byConversion.contains(id))
                    operands = &byConversion.value(id)->sources;
                else if (attachments.contains(id)) {
                    attached.push_back(attachments.value(id)->reference);
                    operands = &attached;
                }
                check(operands && inspectSlot >= 0 && size_t(inspectSlot) < operands->size(),
                      "InvalidInput", "Choose a current support operand slot");
                result.inspection = scene.inspect((*operands)[size_t(inspectSlot)]);
                return result;
            }
            if (byDatum.contains(id)) {
                const auto &definition = *byDatum.value(id);
                if (!evaluatedDatums.contains(definition.id)) {
                    auto values = datum(definition, scene);
                    scene.planes.insert(values.planes);
                    scene.axes.insert(values.axes);
                    evaluatedDatums.insert(definition.id);
                }
            } else if (byConversion.contains(id)) {
                const auto &conversion = *byConversion.value(id);
                auto target = base[sketchIndex.value(conversion.sketch)];
                if (attachments.contains(target.id))
                    target.frame = attachmentFrame(*attachments.value(target.id), scene);
                auto output = convert(conversion, target.frame, scene);
                QCborArray entities, constraints;
                QCborMap members;
                std::vector<SupportCurve> curves;
                for (auto &e : output.entities) {
                    admit(e.id);
                    entities.append(e.id);
                    members.insert(e.id, e.kind == "Point" ? "Point" : "Curve");
                    if (e.kind != "Point") {
                        auto exact = sketchCurve(output, e);
                        curves.push_back({e.id,
                                          exact.curve,
                                          exact.first,
                                          exact.last,
                                          exact.parameterScale,
                                          false,
                                          {}});
                    }
                }
                for (auto &c : output.constraints) {
                    admit(c.id);
                    constraints.append(c.id);
                }
                for (auto it = conversion.members.cbegin(); it != conversion.members.cend(); ++it)
                    check(
                        members.value(it.key()).toString() == it.value(), "ReferenceMissing",
                        "Generated member changed or is missing; repair its reference explicitly");
                converted.insert(id, output);
                scene.derived.insert(id, std::move(curves));
                result.conversions.append(fields({{"id", id},
                                                  {"sketch", output.id},
                                                  {"entities", entities},
                                                  {"constraints", constraints},
                                                  {"members", members}}));
            } else if (sketchIndex.contains(id)) {
                auto candidate = base[sketchIndex.value(id)];
                if (attachments.contains(id))
                    candidate.frame = attachmentFrame(*attachments.value(id), scene);
                for (auto &c : d.conversions)
                    if (c.sketch == id) {
                        check(converted.contains(c.id), "ReferenceMissing",
                              "Derived sketch input has not been evaluated");
                        auto output = converted.value(c.id);
                        candidate.entities.insert(candidate.entities.end(), output.entities.begin(),
                                                  output.entities.end());
                        candidate.constraints.insert(candidate.constraints.end(),
                                                     output.constraints.begin(),
                                                     output.constraints.end());
                    }
                candidate.validate();
                processed.push_back(candidate);
                auto encoded = sketch::encode(processed);
                auto solved = sketch::evaluate(encoded, encoded, true);
                processed = sketch::parse(solved.value("sketches").toArray());
                for (auto &s : processed) {
                    scene.sketches.insert(s.id, s);
                    base[sketchIndex.value(s.id)] = s;
                }
            } else if (spatialIndex.contains(id)) {
                auto candidate = parsedSpatial[spatialIndex.value(id)];
                auto solved = sketch::solveSpatial(
                    candidate, nullptr,
                    [&](const domain::GeometryReference &pref) { return scene.plane(pref); });
                check(solved.valid, "SolveFailure",
                      "Spatial sketch failed to solve: " + solved.message);
                auto exactCurves = evaluateSpatialSketch(solved.sketch);
                scene.spatialSketches.insert(id, solved.sketch);
                scene.derived.insert(id, exactCurves);
            } else if (derivedIndex.contains(id)) {
                const auto &candidate = parsedDerived[derivedIndex.value(id)];
                auto exactCurves = evaluateDerivedCurve(candidate, scene);
                scene.derived.insert(id, exactCurves);
            } else {
                check(byFeature.contains(id), "ReferenceMissing",
                      "Model history node is unavailable");
                prefix.append(byFeature.value(id)->map());
                result.part = evaluate(sketch::encode(processed), prefix);
                check(!result.part.stale, "StaleResult",
                      "Support preview requires current exact geometry");
                scene.stages = result.part.stages;
                scene.history = result.part.history;
            }
        }
        for (const auto &v : d.section_views) {
            auto sv = features::SectionView::parse(v.toMap());
            if (sv.id == inspectOwner) {
                result.inspection = scene.inspect(sv.support);
                return result;
            }
            if (sv.active &&
                (scene.planes.contains(sv.support.object) || scene.stages.contains(sv.support.object))) {
                scene.plane(sv.support);
            }
        }
        for (const auto &v : d.appearance_overrides) {
            auto o = features::AppearanceOverride::parse(v.toMap());
            if (o.id == inspectOwner) {
                check(o.level == "Face" && !o.faceReference.topology.isEmpty(), "InvalidInput",
                      "Only face appearance overrides have geometric references");
                result.inspection = scene.inspect(o.faceReference);
                return result;
            }
            if (o.level == "Face" && !o.faceReference.topology.isEmpty() &&
                scene.stages.contains(o.faceReference.object)) {
                scene.topology(o.faceReference);
            }
        }
        check(inspectOwner.isEmpty(), "ReferenceMissing", "Support operand owner is missing");
        result.evolution = scene.evolution;
        result.sketches = base;
        result.spatialSketches = scene.spatialSketches;
        result.planes = scene.planes;
        result.axes = scene.axes;
        result.derived = scene.derived;
        control.checkpoint("Model supports ready", 100);
        result.map(); // Refuse oversized output before publication.
        return result;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Exact support evaluation failed: ") + e.GetMessageString());
    }
}
QCborArray SupportResult::datumGeometry() const {
    QCborArray datums;
    for (auto it = planes.cbegin(); it != planes.cend(); ++it)
        datums.append(fields({{"id", it.key()},
                              {"kind", "DatumPlane"},
                              {"origin", features::array(it->origin)},
                              {"u", features::array(it->u)},
                              {"v", features::array(it->v)}}));
    for (auto it = axes.cbegin(); it != axes.cend(); ++it)
        datums.append(fields({{"id", it.key()},
                              {"kind", "DatumAxis"},
                              {"origin", features::array(it->origin)},
                              {"direction", features::array(it->direction)}}));
    return datums;
}
QCborMap SupportResult::map(qint64 revision) const {
    QCborArray history;
    for (auto id : order)
        history.append(id);
    auto result = fields(
        {{"version", 1},
         {"sketches", sketch::encode(sketches)},
         {"datums", datumGeometry()},
         {"conversions", conversions},
         {"evaluation_order", history},
         {"part_empty", part.bodies.empty()},
         {"measurement",
          part.bodies.empty()
              ? fields({{"revision", revision}, {"valid", false}, {"reason", "No solid features"}})
              : part.measure(revision)}});
    check(QCborValue(result).toCbor().size() <= qsizetype(limits::control), "ResourceLimit",
          "Support preview output exceeds control admission");
    return result;
}
} // namespace cad::geometry

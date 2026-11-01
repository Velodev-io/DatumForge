#include "Supports.hpp"
#include "Convert.hpp"
#include "sketch/Solver.hpp"
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
                               const exchange::Control &control) {
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
        admit(f.id);
    }
    for (auto &datum : d.datums)
        for (auto id : datum.outputs()) {
            byDatum.insert(id, &datum);
            admit(id);
        }
    for (auto &c : d.conversions) {
        byConversion.insert(c.id, &c);
        admit(c.id);
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
            } else {
                check(byFeature.contains(id), "ReferenceMissing",
                      "Model history node is unavailable");
                prefix.append(byFeature.value(id)->map());
                result.part = evaluate(sketch::encode(processed), prefix);
                check(!result.part.stale, "StaleResult",
                      "Support preview requires current exact geometry");
                scene.stages = result.part.stages;
            }
        }
        result.sketches = base;
        result.planes = scene.planes;
        result.axes = scene.axes;
        control.checkpoint("Model supports ready", 100);
        result.map(); // Refuse oversized output before publication.
        return result;
    } catch (const Standard_Failure &e) {
        throw domain::Error("InvalidGeometry",
                            QString("Exact support evaluation failed: ") + e.GetMessageString());
    }
}
QCborMap SupportResult::map(qint64 revision) const {
    QCborArray datums, history;
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
    for (auto id : order)
        history.append(id);
    auto result = fields(
        {{"version", 1},
         {"sketches", sketch::encode(sketches)},
         {"datums", datums},
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

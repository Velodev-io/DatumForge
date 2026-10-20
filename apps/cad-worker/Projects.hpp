#pragma once
#include "geometry/occt/Part.hpp"
#include "evaluation/Plan.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/Solver.hpp"
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
namespace cad::worker {
class Projects {
    QString configuredRoot, root;
    struct Grant {
        QString path, access;
    };
    QMap<QString, Grant> grants;
    std::unique_ptr<storage::ProjectStore> store;
    void initialize() {
        if (!store) {
            root = configuredRoot;
            if (root.isEmpty())
                root = storage::recoveryRoot(
                    QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                        "/recovery-v1",
                    true);
            store = std::make_unique<storage::ProjectStore>(root);
        }
    }
    QString take(QString token, QString access) {
        domain::id(token);
        domain::check(grants.contains(token) && grants.value(token).access == access,
                      "PermissionDenied",
                      "File capability is missing, expired or has the wrong access");
        return grants.take(token).path;
    }

  public:
    explicit Projects(QString recovery) : configuredRoot(std::move(recovery)) {}
    bool open() const {
        return store && store->isOpen();
    }
    qint64 revision() const {
        return open() ? store->revision() : 0;
    }
    bool handles(QString command) const {
        return QStringList{"NewProject",
                           "CreateDocument",
                           "RenameDocument",
                           "SetDocumentUnits",
                           "SetParameter",
                           "RemoveParameter",
                           "Undo",
                           "Redo",
                           "SwitchDocument",
                           "CloseDocument",
                           "CloseProject",
                           "GrantFile",
                           "InspectProject",
                           "OpenProject",
                           "SaveProject",
                           "SaveAs",
                           "ListRecoveries",
                           "RecoverProject",
                           "AttachObject",
                           "DetachObject",
                           "CollectObjects",
                           "SetSketches",
                           "PreviewSketches",
                           "SetFeatures", "PreviewHistory", "CommitHistory", "ReferenceCandidates", "ResolveReference", "TrimResults",
                           "PreviewFeatures",
                           "DisplayPart",
                           "MeasurePart"}
                   .contains(command) ||
               (command == "QueryOperation" && open());
    }
    QCborMap run(const wire::Envelope &e) {
        using domain::check;
        initialize();
        auto p = e.payload;
        if (e.command == "GrantFile") {
            wire::keys(p, {"token", "path", "access"});
            auto id = domain::id(wire::text(p, "token")), access = wire::text(p, "access"),
                 path = storage::canonicalDestination(wire::text(p, "path"));
            check(access == "read" || access == "save", "InvalidInput",
                  "Unknown file capability access");
            check(grants.size() < 32 && !grants.contains(id), "ResourceLimit",
                  "File capability limit reached");
            check(access != "read" || QFileInfo(path).isFile(), "InvalidInput",
                  "Chosen file does not exist");
            grants.insert(id, {path, access});
            QCborMap result;
            result.insert(QStringLiteral("capability"), id);
            return result;
        }
        if (e.command == "ListRecoveries") {
            wire::keys(p, {});
            QCborMap result;
            result.insert(QStringLiteral("recoveries"), store->recoveryOffers());
            return result;
        }
        if (e.command == "InspectProject") {
            wire::keys(p, {"capability"});
            QCborMap result;
            result.insert(QStringLiteral("inspection"),
                          store->inspect(take(wire::text(p, "capability"), "read")));
            return result;
        }
        if (e.command == "NewProject") {
            check(p.value("replace").isBool(), "InvalidInput",
                  "Project replacement choice required");
            auto semantic = e;
            semantic.payload.remove(QStringLiteral("replace"));
            wire::keys(semantic.payload, {"id", "type", "name", "units", "properties_entity"});
            domain::id(wire::text(p, "id"));
            domain::id(wire::text(p, "properties_entity"));
            domain::kind(wire::text(p, "type"));
            domain::label(wire::text(p, "name"));
            domain::units(wire::text(p, "units"));
            if (open() && store->project() == e.context.project) {
                auto result = store->execute(semantic);
                QCborMap reply;
                reply.insert(QStringLiteral("operation"), result);
                reply.insert(QStringLiteral("project_state"), store->state());
                return reply;
            }
            check(!open() || p.value("replace").toBool(), "InvalidInput",
                  "Resolve the current project before replacing it");
            auto candidate = std::make_unique<storage::ProjectStore>(root);
            candidate->create(e.context.project);
            candidate->execute(semantic);
            store = std::move(candidate);
        } else if (e.command == "OpenProject") {
            wire::keys(p, {"capability", "choice", "replace"});
            check(p.value("replace").isBool() && (!open() || p.value("replace").toBool()),
                  "InvalidInput", "Resolve the current project before replacing it");
            auto path = take(wire::text(p, "capability"), "read"), choice = wire::text(p, "choice");
            auto inspected = store->inspect(path);
            if (open() && wire::text(inspected, "project_id") == store->project()) {
                auto old = store->project();
                store->close();
                try {
                    store->open(path, choice);
                } catch (...) {
                    store->recover(old);
                    throw;
                }
            } else {
                auto candidate = std::make_unique<storage::ProjectStore>(root);
                candidate->open(path, choice);
                store = std::move(candidate);
            }
        } else if (e.command == "RecoverProject") {
            wire::keys(p, {"project_id"});
            check(!open(), "InvalidInput", "Close the active project before recovery");
            store->recover(domain::id(wire::text(p, "project_id")));
        } else if (e.command == "CloseProject") {
            wire::keys(p, {"choice"});
            check(wire::text(p, "choice") == "KeepRecoverable", "InvalidInput",
                  "Choose KeepRecoverable explicitly or cancel closing");
            store->close();
            QCborMap reply;
            reply.insert(QStringLiteral("project_closed"), true);
            return reply;
        } else {
            check(open() && (e.context.project == store->project() ||
                             (e.command == "SaveAs" &&
                              store->outcome(e.operation).value("durable").toBool())),
                  "StaleSession", "The project session changed");
            if(e.command=="TrimResults"){wire::keys(p,{});store->trimResults();return sketch::fields({{"cache_statistics",store->cacheStatistics()}});}
            if(e.command=="ReferenceCandidates"){wire::keys(p,{"owner","token"});check(e.revision==store->revision(),"StaleRevision","Project changed before repair inspection");return sketch::fields({{"reference_resolution",store->referenceCandidates(e.context.document,domain::id(wire::text(p,"owner")),wire::text(p,"token"))}});}
            if (e.command == "PreviewFeatures" || e.command == "DisplayPart" ||
                e.command == "MeasurePart" || e.command=="PreviewHistory") {
                check(e.revision == store->revision(), "StaleRevision",
                      "Part result revision changed");
                auto definitions = store->features(e.context.document);
                if(e.command=="PreviewHistory"){wire::keys(p,{"features"});check(p.value("features").isArray(),"InvalidInput","Feature array required");definitions=p.value("features").toArray();}
                else if (e.command == "PreviewFeatures") {
                    wire::keys(p, {"features", "deflection_mm"});
                    check(p.value("features").isArray(), "InvalidInput", "Feature array required");
                    definitions = p.value("features").toArray();
                } else if (e.command == "DisplayPart")
                    wire::keys(p, {"deflection_mm"});
                else
                    wire::keys(p, {"body", "token", "second_token", "density_kg_m3"});
                auto part = store->evaluatePart(e.context.document, e.command=="PreviewFeatures"||e.command=="PreviewHistory"?std::optional<QCborArray>(definitions):std::nullopt,e.command=="PreviewHistory");
                auto errors=part.errors;auto diagnostics=part.diagnostics;
                if(part.stale&&e.command=="PreviewHistory"){auto prefix=part.stages;part=store->evaluatePart(e.context.document);part.stale=true;part.errors=errors;part.diagnostics=diagnostics;part.stages=prefix;}
                double assignedDensity = 0;
                for (auto value : store->state().value("documents").toArray()) {
                    auto d = value.toMap();
                    if (d.value("id").toString() != e.context.document)
                        continue;
                    for (auto parameter : d.value("parameters").toArray()) {
                        auto q = parameter.toMap();
                        if (q.value("name").toString() == "MaterialDensityKgM3")
                            assignedDensity = q.value("value_si").toDouble();
                    }
                }

                QCborMap reply;
                if (e.command == "MeasurePart") {
                    reply.insert(QStringLiteral("part_measurement"),
                                 part.measure(e.revision, wire::text(p, "body"),
                                              wire::text(p, "token"),
                                              (features::number(p, "density_kg_m3", 0, 1e6) > 0
                                                   ? features::number(p, "density_kg_m3", 0, 1e6)
                                                   : assignedDensity),
                                              wire::text(p, "second_token")));
                } else {
                    bool stale=part.stale;
                    if(part.bodies.empty())return sketch::fields({{"part_empty",true},{"part_stale",stale},{"errors",part.errors},{"history_impact",evaluation::impact(store->profileSketches(e.context.document,definitions),store->features(e.context.document),definitions)}});
                    auto packed = pack(part.mesh(e.command=="PreviewHistory"?.2:features::number(p, "deflection_mm", .01, 5)));
                    reply.insert(QStringLiteral("display_manifest"), packed.manifest);
                    // The host streams and removes this private value before sending control data.
                    reply.insert(QStringLiteral("_display_bytes"), packed.bytes);
                    reply.insert(QStringLiteral("part_stale"),stale);reply.insert(QStringLiteral("errors"),part.errors);
                    reply.insert(QStringLiteral("cache_statistics"),store->cacheStatistics());
                    if(e.command=="PreviewHistory")reply.insert(QStringLiteral("history_impact"),evaluation::impact(store->profileSketches(e.context.document,definitions),store->features(e.context.document),definitions));
                    reply.insert(QStringLiteral("part_measurement"),stale?sketch::fields({{"revision",e.revision},{"valid",false},{"reason","STALE: last valid display; measurement refused"}}):part.measure(e.revision, {}, {}, assignedDensity));
                    reply.insert(QStringLiteral("feature_diagnostics"), part.diagnostics);
                    reply.insert(QStringLiteral("topology_history"), part.history);
                    reply.insert(QStringLiteral("topology"), part.topology());
                    QCborArray bodies;
                    for (auto &b : part.bodies)
                        bodies.append(b.id);
                    reply.insert(QStringLiteral("bodies"), bodies);
                }
                return reply;
            }
            if (e.command == "PreviewSketches") {
                wire::keys(p, {"sketches"});
                check(e.revision == store->revision(), "StaleRevision",
                      "Project changed before preview");
                check(p.value("sketches").isArray(), "InvalidInput", "Sketch array required");
                auto candidate = sketch::evaluate(p.value("sketches").toArray(),
                                                  store->sketches(e.context.document), false);
                QCborMap reply;
                reply.insert(QStringLiteral("sketch_preview"), candidate.value("sketches"));
                reply.insert(QStringLiteral("sketch_diagnostics"), candidate.value("diagnostics"));
                return reply;
            }
            QCborMap result;
            if (e.command == "SaveProject" || e.command == "SaveAs") {
                wire::keys(p, e.command == "SaveAs" ? QSet<QString>{"capability", "new_project_id"}
                                                    : QSet<QString>{"capability"});
                result = store->save(e, take(wire::text(p, "capability"), "save"),
                                     e.command == "SaveAs");
            } else if (e.command == "AttachObject") {
                wire::keys(p, {"capability", "size", "expected_sha256", "classification"});
                auto path = take(wire::text(p, "capability"), "read");
                QFile input(path);
                check(input.open(QIODevice::ReadOnly), "PermissionDenied",
                      "Cannot open attachment source");
                auto semantic = e;
                semantic.payload.remove(QStringLiteral("capability"));
                store->attach(semantic, input, wire::integer(p, "size"),
                              wire::text(p, "classification"));
                result = store->outcome(e.operation);
            } else if (e.command == "QueryOperation") {
                wire::keys(p, {"operation"});
                return store->outcome(wire::text(p, "operation"));
            } else if (e.command == "CollectObjects") {
                wire::keys(p, {});
                store->collect();
            } else
                result = store->execute(e);
            QCborMap reply;
            reply.insert(QStringLiteral("project_state"), store->state());
            reply.insert(QStringLiteral("operation"), result);
            return reply;
        }
        QCborMap result;
        result.insert(QStringLiteral("project_state"), store->state());
        return result;
    }
};
} // namespace cad::worker

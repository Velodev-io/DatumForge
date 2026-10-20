#pragma once
#include "persistence/ProjectStore.hpp"
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
        return QStringList{
                   "NewProject",     "CreateDocument",  "RenameDocument", "SetDocumentUnits",
                   "SetParameter",   "RemoveParameter", "Undo",           "Redo",
                   "SwitchDocument", "CloseDocument",   "CloseProject",   "GrantFile",
                   "InspectProject", "OpenProject",     "SaveProject",    "SaveAs",
                   "ListRecoveries", "RecoverProject",  "AttachObject",   "DetachObject",
                   "CollectObjects"}
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

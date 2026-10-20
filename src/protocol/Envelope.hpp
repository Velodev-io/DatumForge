#pragma once
#include "Framing.hpp"
#include "domain/WorkspaceState.hpp"
#include <algorithm>

namespace cad::wire {
inline bool uuid(const QString &value) {
    return value.size() == 36 && !QUuid(value).isNull() &&
           value == QUuid(value).toString(QUuid::WithoutBraces);
}
inline void keys(const QCborMap &map, const QSet<QString> &fields) {
    require(map.size() == fields.size(), "missing/unknown fields");
    for (auto it = map.begin(); it != map.end(); ++it)
        require(it.key().isString() && fields.contains(it.key().toString()), "unknown field");
}
inline QString text(const QCborMap &map, const QString &name) {
    auto value = map.value(name);
    require(value.isString(), "string required");
    return value.toString();
}
inline qint64 integer(const QCborMap &map, const QString &name, qint64 max = INT64_MAX) {
    auto value = map.value(name);
    require(value.isInteger() && value.toInteger() >= 0 && value.toInteger() <= max,
            "integer range");
    return value.toInteger();
}
struct Envelope {
    QString request = newId(), operation = newId(), epoch;
    Context context;
    qint64 revision = 0, generation = 0;
    QString command;
    QCborMap payload;
    QCborMap map() const {
        QCborMap m;
        m.insert(QStringLiteral("protocol_version"), 1);
        m.insert(QStringLiteral("request_id"), request);
        m.insert(QStringLiteral("operation_id"), operation);
        m.insert(QStringLiteral("worker_epoch"), epoch);
        m.insert(QStringLiteral("project_id"), context.project);
        m.insert(QStringLiteral("document_id"), context.document);
        m.insert(QStringLiteral("command_session_id"), context.session);
        m.insert(QStringLiteral("base_revision"), revision);
        m.insert(QStringLiteral("preview_generation"), generation);
        m.insert(QStringLiteral("command_type"), command);
        m.insert(QStringLiteral("payload"), payload);
        return m;
    }
    static Envelope parse(const QCborMap &map) {
        keys(map, {"protocol_version", "request_id", "operation_id", "worker_epoch", "project_id",
                   "document_id", "command_session_id", "base_revision", "preview_generation",
                   "command_type", "payload"});
        require(integer(map, "protocol_version") == 1, "protocol version");
        Envelope e;
        e.request = text(map, "request_id");
        e.operation = text(map, "operation_id");
        e.epoch = text(map, "worker_epoch");
        e.context = {text(map, "project_id"), text(map, "document_id"),
                     text(map, "command_session_id")};
        for (const auto &id : {e.request, e.operation, e.epoch, e.context.project,
                               e.context.document, e.context.session})
            require(uuid(id), "invalid identity");
        e.revision = integer(map, "base_revision");
        e.generation = integer(map, "preview_generation");
        e.command = text(map, "command_type");
        require(map.value("payload").isMap(), "payload map");
        e.payload = map.value("payload").toMap();
        return e;
    }
    QByteArray intentHash() const {
        auto m = map();
        m.remove(QStringLiteral("request_id"));
        return QCryptographicHash::hash(QCborValue(m).toCbor(QCborValue::SortKeysInMaps),
                                        QCryptographicHash::Sha256);
    }
};
inline QCborMap reply(const Envelope &e, QString status, QCborMap data = {}, QString code = {},
                      QString message = {}) {
    auto m = e.map();
    m.insert(QStringLiteral("status"), status);
    m.insert(QStringLiteral("result_revision"), e.revision);
    QCborMap diagnostic;
    diagnostic.insert(QStringLiteral("code"), code);
    diagnostic.insert(QStringLiteral("message"), message);
    diagnostic.insert(QStringLiteral("operation_id"), e.operation);
    diagnostic.insert(QStringLiteral("retryable"), false);
    m.insert(QStringLiteral("diagnostics"), diagnostic);
    m.insert(QStringLiteral("data"), data);
    return m;
}
inline Envelope replyEnvelope(const QCborMap &m) {
    auto base = m;
    for (auto key : {"status", "result_revision", "diagnostics", "data"})
        base.remove(QString::fromLatin1(key));
    auto e = Envelope::parse(base);
    require(m.value("status").isString() && m.value("data").isMap() &&
                m.value("diagnostics").isMap(),
            "reply types");
    integer(m, "result_revision");
    auto diagnostic = m.value("diagnostics").toMap();
    keys(diagnostic, {"code", "message", "operation_id", "retryable"});
    text(diagnostic, "code");
    text(diagnostic, "message");
    require(text(diagnostic, "operation_id") == e.operation &&
                diagnostic.value("retryable").isBool(),
            "diagnostic correlation");
    return e;
}
inline QCborMap bootstrap(QString epoch, QByteArray secret) {
    QCborMap map;
    map.insert(QStringLiteral("version"), 1);
    map.insert(QStringLiteral("epoch"), epoch);
    map.insert(QStringLiteral("capability"), secret);
    return map;
}
} // namespace cad::wire

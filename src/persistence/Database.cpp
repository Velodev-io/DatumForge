#include "Database.hpp"
#include <QFileInfo>
namespace cad::storage {
void sqlcheck(int status, sqlite3 *db) {
    if (status == SQLITE_OK || status == SQLITE_ROW || status == SQLITE_DONE)
        return;
    int code = status & 255;
    QString category = "StorageError";
    if (code == SQLITE_FULL)
        category = "StorageFull";
    if (code == SQLITE_READONLY || code == SQLITE_PERM || code == SQLITE_CANTOPEN)
        category = "PermissionDenied";
    if (code == SQLITE_BUSY || code == SQLITE_LOCKED)
        category = "ProjectLocked";
    if (code == SQLITE_CORRUPT || code == SQLITE_NOTADB)
        category = "InvalidProject";
    throw domain::Error(category,
                        db ? QString::fromUtf8(sqlite3_errmsg(db)) : "SQLite operation failed");
}
namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3 *db, const char *sql, const QVariantList &values) {
    sqlite3_stmt *raw = nullptr;
    sqlcheck(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr), db);
    Statement out(raw, sqlite3_finalize);
    for (int n = 0; n < values.size(); ++n) {
        const auto &v = values[n];
        int code;
        if (v.isNull())
            code = sqlite3_bind_null(raw, n + 1);
        else if (v.metaType().id() == QMetaType::QByteArray) {
            auto b = v.toByteArray();
            code = sqlite3_bind_blob(raw, n + 1, b.constData(), b.size(), SQLITE_TRANSIENT);
        } else if (v.metaType().id() == QMetaType::QString) {
            auto b = v.toString().toUtf8();
            code = sqlite3_bind_text(raw, n + 1, b.constData(), b.size(), SQLITE_TRANSIENT);
        } else if (v.metaType().id() == QMetaType::Double)
            code = sqlite3_bind_double(raw, n + 1, v.toDouble());
        else
            code = sqlite3_bind_int64(raw, n + 1, v.toLongLong());
        sqlcheck(code, db);
    }
    return out;
}
} // namespace
Database::Database(QString path, bool readonly) {
    domain::check(!readonly || (QFileInfo(path).isFile() &&
                                QFileInfo(path).size() <= qint64(limits::project)),
                  "ResourceLimit", "Project is missing or exceeds the 2 GiB admission limit");
    int code = sqlite3_open_v2(
        path.toUtf8().constData(), &handle,
        readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (code != SQLITE_OK) {
        auto message = QString::fromUtf8(sqlite3_errmsg(handle));
        sqlite3_close(handle);
        handle = nullptr;
        throw domain::Error("PermissionDenied", message);
    }
    try {
        sqlite3_extended_result_codes(handle, 1);
        sqlite3_busy_timeout(handle, 100);
        sqlite3_db_config(handle, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr);
        sqlite3_db_config(handle, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr);
        sqlite3_limit(handle, SQLITE_LIMIT_LENGTH, int(limits::object + limits::control));
        sqlite3_limit(handle, SQLITE_LIMIT_SQL_LENGTH, int(limits::control));
        sqlite3_limit(handle, SQLITE_LIMIT_ATTACHED, 0);
        exec("PRAGMA foreign_keys=ON; PRAGMA trusted_schema=OFF; PRAGMA temp_store=FILE; PRAGMA "
             "cache_size=-8192");
    } catch (...) {
        sqlite3_close(handle);
        handle = nullptr;
        throw;
    }
}
Database::~Database() {
    if (handle)
        sqlite3_close(handle);
}
void Database::exec(const char *sql) {
    sqlcheck(sqlite3_exec(handle, sql, nullptr, nullptr, nullptr), handle);
}
void Database::run(const char *sql, const QVariantList &values) {
    auto statement = prepare(handle, sql, values);
    sqlcheck(sqlite3_step(statement.get()), handle);
}
void Database::forEachRow(const char *sql, const std::function<void(const QVariantList &)> &visitor,
                          const QVariantList &values) const {
    auto statement = prepare(handle, sql, values);
    int count = 0;
    int code;
    while ((code = sqlite3_step(statement.get())) == SQLITE_ROW) {
        domain::check(count++ < int(limits::collection), "ResourceLimit",
                      "Query result exceeds the collection limit");
        QVariantList row;
        for (int col = 0; col < sqlite3_column_count(statement.get()); ++col) {
            int type = sqlite3_column_type(statement.get(), col);
            int size = sqlite3_column_bytes(statement.get(), col);
            domain::check(size <= int(limits::control), "ResourceLimit",
                          "Control record too large; object payloads require streaming");
            if (type == SQLITE_INTEGER)
                row.append(qlonglong(sqlite3_column_int64(statement.get(), col)));
            else if (type == SQLITE_FLOAT)
                row.append(sqlite3_column_double(statement.get(), col));
            else if (type == SQLITE_BLOB)
                row.append(QByteArray(
                    static_cast<const char *>(sqlite3_column_blob(statement.get(), col)), size));
            else if (type == SQLITE_TEXT)
                row.append(QString::fromUtf8(
                    reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), col)),
                    size));
            else
                row.append(QVariant());
        }
        visitor(row);
    }
    sqlcheck(code, handle);
}
QList<QVariantList> Database::rows(const char *sql, const QVariantList &values) const {
    QList<QVariantList> out;
    qint64 retained = 0;
    forEachRow(
        sql,
        [&](const QVariantList &row) {
            retained += 64 + row.size() * 128;
            for (const auto &value : row) {
                if (value.metaType().id() == QMetaType::QByteArray)
                    retained += value.toByteArray().size();
                else if (value.metaType().id() == QMetaType::QString)
                    retained += value.toString().size() * qint64(sizeof(QChar));
            }
            domain::check(retained <= (32LL << 20), "ResourceLimit",
                          "Query result exceeds memory budget; use row streaming");
            out.append(row);
        },
        values);
    return out;
}
QVariant Database::scalar(const char *sql, const QVariantList &values) const {
    auto result = rows(sql, values);
    return result.empty() ? QVariant() : result.first().first();
}
void Database::working() {
    exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA fullfsync=ON; PRAGMA "
         "checkpoint_fullfsync=ON; PRAGMA wal_autocheckpoint=128; PRAGMA max_page_count=524288");
    domain::check(scalar("PRAGMA foreign_keys").toInt() == 1 &&
                      scalar("PRAGMA synchronous").toInt() == 2 &&
                      scalar("PRAGMA journal_mode").toString() == "wal",
                  "StorageError", "Required WAL/FULL/foreign-key settings unavailable");
}
void backup(Database &source, Database &target, const Boundary &observer) {
    const bool ownRead = sqlite3_get_autocommit(source.handle) != 0;
    if (ownRead)
        source.exec("BEGIN");
    try {
        source.scalar("SELECT revision FROM project_meta");
        auto *copy = sqlite3_backup_init(target.handle, "main", source.handle, "main");
        sqlcheck(copy ? SQLITE_OK : sqlite3_errcode(target.handle), target.handle);
        int code = SQLITE_OK;
        try {
            do {
                code = sqlite3_backup_step(copy, 128);
                if (observer)
                    observer("snapshot_chunk");
            } while (code == SQLITE_OK);
        } catch (...) {
            sqlite3_backup_finish(copy);
            throw;
        }
        auto finish = sqlite3_backup_finish(copy);
        sqlcheck(code, target.handle);
        sqlcheck(finish, target.handle);
        if (ownRead)
            source.exec("COMMIT");
    } catch (...) {
        if (ownRead)
            sqlite3_exec(source.handle, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
} // namespace cad::storage

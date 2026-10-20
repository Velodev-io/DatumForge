#pragma once
#include "domain/Documents.hpp"
#include <QList>
#include <QVariant>
#include <memory>
#include <sqlite3.h>
namespace cad::storage {
void sqlcheck(int status, sqlite3 *db);
class Database {
  public:
    sqlite3 *handle = nullptr;
    explicit Database(QString path, bool readonly = false);
    ~Database();
    Database(const Database &) = delete;
    void exec(const char *sql);
    void run(const char *sql, const QVariantList &values = {});
    void forEachRow(const char *sql, const std::function<void(const QVariantList &)> &visitor,
                    const QVariantList &values = {}) const;
    QList<QVariantList> rows(const char *sql, const QVariantList &values = {}) const;
    QVariant scalar(const char *sql, const QVariantList &values = {}) const;
    void working();
};
class Transaction {
    Database &db;
    bool done = false;

  public:
    explicit Transaction(Database &database) : db(database) {
        db.exec("BEGIN IMMEDIATE");
    }
    ~Transaction() {
        if (!done)
            sqlite3_exec(db.handle, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    void commit() {
        db.exec("COMMIT");
        done = true;
    }
};
using Boundary = std::function<void(QString)>;
void backup(Database &source, Database &target, const Boundary &observer = {});
} // namespace cad::storage

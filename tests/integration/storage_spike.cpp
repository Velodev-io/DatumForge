#include "support/check.hpp"
#include "support/limits.hpp"
#include "support/test_root.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QThread>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace {
class WriterLock {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    OVERLAPPED overlap{};
#else
    int fd = -1;
#endif
  public:
    WriterLock(const QString &path, const QString &lockRoot) {
#ifdef _WIN32
        auto file =
            CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_ALWAYS, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("writer identity open failed");
        BY_HANDLE_FILE_INFORMATION info{};
        auto identified = GetFileInformationByHandle(file, &info);
        CloseHandle(file);
        if (!identified)
            throw std::runtime_error("writer identity failed");
        auto lockPath = lockRoot + QString("/writer-%1-%2-%3.lock")
                                       .arg(info.dwVolumeSerialNumber)
                                       .arg(info.nFileIndexHigh)
                                       .arg(info.nFileIndexLow);
        handle =
            CreateFileW(reinterpret_cast<LPCWSTR>(lockPath.utf16()), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("writer lock open failed");
        if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                        &overlap)) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            throw std::runtime_error("ProjectLocked");
        }
#else
        auto file = ::open(path.toUtf8(), O_RDWR | O_CREAT, 0600);
        if (file < 0)
            throw std::runtime_error("writer identity open failed");
        struct stat info{};
        auto identified = ::fstat(file, &info);
        ::close(file);
        if (identified)
            throw std::runtime_error("writer identity failed");
        auto lockPath =
            lockRoot +
            QString("/writer-%1-%2.lock").arg(qulonglong(info.st_dev)).arg(qulonglong(info.st_ino));
        fd = ::open(lockPath.toUtf8(), O_RDWR | O_CREAT, 0600);
        if (fd < 0)
            throw std::runtime_error("writer lock open failed");
        if (::flock(fd, LOCK_EX | LOCK_NB)) {
            ::close(fd);
            fd = -1;
            throw std::runtime_error("ProjectLocked");
        }
#endif
    }
    ~WriterLock() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
#else
        if (fd >= 0)
            ::close(fd);
#endif
    }
};
void sqlcheck(int status, sqlite3 *db) {
    if (status != SQLITE_OK && status != SQLITE_DONE && status != SQLITE_ROW)
        throw std::runtime_error(db ? sqlite3_errmsg(db) : "SQLite failure");
}
class Db {
  public:
    sqlite3 *db = nullptr;
    explicit Db(const QString &path, bool readonly = false) {
        auto code = sqlite3_open_v2(
            path.toUtf8().constData(), &db,
            readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        if (code != SQLITE_OK) {
            auto message = std::string(sqlite3_errmsg(db));
            sqlite3_close(db);
            throw std::runtime_error(message);
        }
    }
    ~Db() {
        sqlite3_close(db);
    }
    Db(const Db &) = delete;
    Db &operator=(const Db &) = delete;
    void exec(const std::string &sql) {
        sqlcheck(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr), db);
    }
    std::string scalar(const std::string &sql) {
        sqlite3_stmt *statement = nullptr;
        sqlcheck(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr), db);
        auto cleanup =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(statement, sqlite3_finalize);
        auto code = sqlite3_step(statement);
        sqlcheck(code, db);
        return code == SQLITE_ROW && sqlite3_column_text(statement, 0)
                   ? reinterpret_cast<const char *>(sqlite3_column_text(statement, 0))
                   : "";
    }
    void bind(const char *sql, const QList<QByteArray> &values) {
        sqlite3_stmt *statement = nullptr;
        sqlcheck(sqlite3_prepare_v2(db, sql, -1, &statement, nullptr), db);
        auto cleanup =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(statement, sqlite3_finalize);
        for (qsizetype i = 0; i < values.size(); ++i)
            sqlcheck(sqlite3_bind_text(statement, int(i + 1), values[i].constData(),
                                       int(values[i].size()), SQLITE_TRANSIENT),
                     db);
        sqlcheck(sqlite3_step(statement), db);
    }
};
void validate(Db &db) {
    CHECK(db.scalar("PRAGMA integrity_check") == "ok");
    CHECK(db.scalar("PRAGMA foreign_key_check").empty());
    CHECK(db.scalar("SELECT count(*) FROM objects WHERE length(payload) != size") == "0");
    CHECK(db.scalar("SELECT count(*) FROM object_refs r LEFT JOIN objects o ON r.object_id=o.id "
                    "WHERE o.id IS NULL") == "0");
}
void compatible(Db &db) {
    CHECK(db.scalar("PRAGMA user_version") == "1");
    CHECK(db.scalar("PRAGMA application_id") == "1128350800");
}
void flush(const QString &path, bool directory = false) {
#ifdef _WIN32
    auto handle =
        CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle))
        throw std::runtime_error("FlushFileBuffers failed");
    CloseHandle(handle);
#else
    (void)directory;
    auto fd = ::open(path.toUtf8(), O_RDONLY);
    if (fd < 0)
        throw std::runtime_error("flush open failed");
    auto code = ::fsync(fd);
    ::close(fd);
    if (code)
        throw std::runtime_error("fsync failed");
#endif
}
void snapshot(Db &source, const QString &root, const QString &phase) {
    auto tmp = root + "/snapshot.tmp";
    QFile::remove(tmp);
    source.exec("BEGIN");
    auto pinned = source.scalar("SELECT revision FROM project_meta");
    {
        Db target(tmp);
        auto *backup = sqlite3_backup_init(target.db, "main", source.db, "main");
        CHECK(backup);
        int code;
        do {
            code = sqlite3_backup_step(backup, 16);
            if (phase == "during_snapshot")
                std::_Exit(72);
        } while (code == SQLITE_OK);
        auto finish = sqlite3_backup_finish(backup);
        sqlcheck(code, target.db);
        sqlcheck(finish, target.db);
        target.exec("PRAGMA journal_mode=DELETE");
        validate(target);
        CHECK(target.scalar("SELECT revision FROM project_meta") == pinned);
    }
    source.exec("COMMIT");
    flush(tmp);
    if (phase == "write_error")
        throw std::runtime_error("Injected write failure before publication");
    if (phase == "permission_error")
        throw std::runtime_error("Injected permission denial before publication");
    auto destination = root + "/saved.cadproj";
#ifdef _WIN32
    CHECK(MoveFileExW(reinterpret_cast<LPCWSTR>(tmp.utf16()),
                      reinterpret_cast<LPCWSTR>(destination.utf16()),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
#else
    CHECK(::rename(tmp.toUtf8(), destination.toUtf8()) == 0);
    flush(root, true);
#endif
    if (phase == "after_publication")
        std::_Exit(73);
    std::cout << "Saved revision " << pinned << '\n';
}
void commit(Db &db, const QString &operation, const QString &payload, const QString &phase) {
    auto hash = QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha256).toHex();
    db.exec("BEGIN IMMEDIATE");
    db.bind("CREATE TEMP TABLE IF NOT EXISTS request(op TEXT, hash TEXT)", {});
    db.exec("DELETE FROM request");
    db.bind("INSERT INTO request VALUES(?,?)", {operation.toUtf8(), hash});
    auto prior =
        db.scalar("SELECT request_hash FROM commands WHERE operation_id=(SELECT op FROM request)");
    if (!prior.empty()) {
        CHECK(prior == hash.toStdString());
        db.exec("ROLLBACK");
        std::cout << "Deduplicated\n";
        return;
    }
    db.exec("UPDATE project_meta SET revision=revision+1");
    db.bind("INSERT INTO commands SELECT ?,?,revision FROM project_meta",
            {operation.toUtf8(), hash});
    db.bind("INSERT INTO semantic VALUES(?,?)", {operation.toUtf8(), payload.toUtf8()});
    if (phase == "before_commit")
        std::_Exit(70);
    if (phase == "pause_before_commit") {
        std::cout << "BOUNDARY" << std::endl;
        QThread::sleep(60);
    }
    if (phase == "disk_full") {
        db.exec("PRAGMA max_page_count=16");
        db.exec("INSERT INTO objects VALUES('full','authoritative',1048576,zeroblob(1048576))");
    }
    db.exec("COMMIT");
    if (phase == "after_commit")
        std::_Exit(71);
    if (phase == "pause_after_commit") {
        std::cout << "BOUNDARY" << std::endl;
        QThread::sleep(60);
    }
    std::cout << "Recoverable revision " << db.scalar("SELECT revision FROM project_meta") << '\n';
}
void object(Db &db) {
    db.exec("BEGIN IMMEDIATE");
    constexpr int size = 4 * 1024 * 1024;
    db.exec("INSERT INTO objects VALUES('source','authoritative',4194304,zeroblob(4194304))");
    auto rowid = sqlite3_last_insert_rowid(db.db);
    sqlite3_blob *blob = nullptr;
    sqlcheck(sqlite3_blob_open(db.db, "main", "objects", "payload", rowid, 1, &blob), db.db);
    QByteArray chunk(64 * 1024, 'A');
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (int offset = 0; offset < size; offset += chunk.size()) {
        sqlcheck(sqlite3_blob_write(blob, chunk.constData(), int(chunk.size()), offset), db.db);
        hash.addData(chunk);
    }
    sqlcheck(sqlite3_blob_close(blob), db.db);
    db.exec(
        "INSERT INTO object_refs VALUES('recovery','source'),('undo','source'),('job','source')");
    db.exec("INSERT INTO objects VALUES('mesh','cache',1,x'00')");
    db.exec("COMMIT");
    std::cout << "source_bytes=" << size << " chunk_bytes=" << chunk.size()
              << " sha256=" << hash.result().toHex().constData() << '\n';
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([&] {
        CHECK(argc >= 3);
        auto root = testRoot(QString::fromLocal8Bit(argv[1]));
        auto action = QString::fromLocal8Bit(argv[2]);
        auto phase = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();
        auto path = root + "/working.sqlite";
        if (action == "lock") {
            CHECK(argc > 3);
            auto alias = QFileInfo(root + '/' + phase).canonicalFilePath();
            CHECK(alias.startsWith(root + '/'));
            WriterLock held(alias, root);
            std::cout << "LOCKED" << std::endl;
            QThread::sleep(3);
            return;
        }
        if (action == "inspect") {
            Db db(root + "/saved.cadproj", true);
            compatible(db);
            validate(db);
            std::cout << "revision=" << db.scalar("SELECT revision FROM project_meta") << '\n';
            return;
        }
        WriterLock writer(path, root);
        Db db(path);
        if (action == "init") {
            db.exec("PRAGMA user_version=1; PRAGMA application_id=1128350800; PRAGMA "
                    "journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON");
            db.exec("CREATE TABLE project_meta(id TEXT PRIMARY KEY, revision INTEGER NOT NULL); "
                    "INSERT INTO project_meta VALUES('fixture-project',0);"
                    "CREATE TABLE commands(operation_id TEXT PRIMARY KEY,request_hash TEXT NOT "
                    "NULL,revision INTEGER NOT NULL UNIQUE);"
                    "CREATE TABLE semantic(id TEXT PRIMARY KEY,payload TEXT NOT NULL);"
                    "CREATE TABLE objects(id TEXT PRIMARY KEY,kind TEXT CHECK(kind IN "
                    "('cache','authoritative')),size INTEGER CHECK(size>=0 AND "
                    "size<=134217728),payload BLOB);"
                    "CREATE TABLE object_refs(root TEXT,object_id TEXT REFERENCES "
                    "objects(id),PRIMARY KEY(root,object_id));");
            CHECK(db.scalar("PRAGMA journal_mode") == "wal");
            CHECK(db.scalar("PRAGMA synchronous") == "2");
            CHECK(db.scalar("PRAGMA foreign_keys") == "1");
            std::cout << "SQLite " << sqlite3_libversion() << " WAL/FULL/FK\n";
            return;
        }
        compatible(db);
        db.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL");
        if (action == "commit") {
            CHECK(argc == 6);
            commit(db, QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[5]), phase);
        } else if (action == "save")
            snapshot(db, root, phase);
        else if (action == "object")
            object(db);
        else if (action == "gc") {
            db.exec("DELETE FROM objects WHERE kind='cache' AND id NOT IN (SELECT object_id FROM "
                    "object_refs)");
            db.exec("DELETE FROM objects WHERE id NOT IN (SELECT object_id FROM object_refs)");
            CHECK(db.scalar("SELECT length(payload) FROM objects WHERE id='source'") == "4194304");
            validate(db);
        } else if (action == "missing")
            db.exec("INSERT INTO object_refs VALUES('bad','missing')");
        else if (action == "query") {
            validate(db);
            std::cout << "revision=" << db.scalar("SELECT revision FROM project_meta")
                      << " commands=" << db.scalar("SELECT count(*) FROM commands") << '\n';
        } else if (action == "migrate_fail") {
            // Prototype migration always stages a copy and fails before publication.
            QFile::remove(root + "/migration.tmp");
            Db copy(root + "/migration.tmp");
            auto *backup = sqlite3_backup_init(copy.db, "main", db.db, "main");
            CHECK(backup);
            auto code = sqlite3_backup_step(backup, -1);
            auto finished = sqlite3_backup_finish(backup);
            sqlcheck(code, copy.db);
            sqlcheck(finished, copy.db);
            copy.exec("PRAGMA user_version=2");
            throw std::runtime_error("Injected migration failure on copy; original untouched");
        } else
            throw std::runtime_error("Unknown spike action");
    });
}

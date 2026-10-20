#pragma once
#include "Files.hpp"
#include <QIODevice>
#include <QSet>
namespace cad::storage {
struct StoreOptions {
    Boundary boundary;
    int historyEntries = 128;
    qint64 historyBytes = 8LL << 20;
    int maximumPages = 524288;
};
class ProjectStore {
    QString root, workspace, projectId, activeDocument, sourcePath;
    QStringList opened;
    std::unique_ptr<Lock> projectLock, workingLock;
    std::unique_ptr<DestinationLocks> destination;
    std::unique_ptr<Database> db;
    StoreOptions options;
    void point(QString stage) const;
    void createWorkspace();
    void activate();
    void configureWorking();
    void validate(Database &candidate, bool saved = false) const;
    void apply(const QCborMap &delta, qint64 revision);
    void retainHistory();
    void reconcile();
    void cleanupStaging();
    QCborMap mutate(const wire::Envelope &e);
    QCborMap deltaFor(const wire::Envelope &e, QCborMap &inverse);
    void record(const wire::Envelope &e, QByteArray hash, QCborMap before, QCborMap after,
                qint64 revision, QString category);

  public:
    explicit ProjectStore(QString recoveryDirectory, StoreOptions settings = {});
    ~ProjectStore();
    void create(QString project);
    void open(QString file, QString choice);
    void recover(QString project);
    void close();
    bool isOpen() const {
        return bool(db);
    }
    QString workingPath() const {
        return workspace + "/working.sqlite";
    }
    QString project() const {
        return projectId;
    }
    QString active() const {
        return activeDocument;
    }
    QString savedPath() const {
        return sourcePath;
    }
    qint64 revision() const;
    QCborMap state() const;
    QCborArray sketches(QString document) const;
    QCborArray features(QString document) const;
    QCborMap execute(const wire::Envelope &e);
    QCborMap outcome(QString operation) const;
    QCborArray recoveryOffers() const;
    QCborMap inspect(QString path) const;
    QCborMap save(const wire::Envelope &e, QString path, bool saveAs = false);
    QString attach(const wire::Envelope &e, QIODevice &input, qint64 declared,
                   QString classification = "authoritative");
    void retain(QString rootType, QString rootId, QString hash);
    void release(QString rootType, QString rootId);
    void collect();
    static void migrationCopy(QString source, QString destination, const Boundary &observer = {});
};
} // namespace cad::storage

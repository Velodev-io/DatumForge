#pragma once
#include "Files.hpp"
#include "assembly/Geometry.hpp"
#include "drawing/Geometry.hpp"
#include "exchange/Exchange.hpp"
#include "geometry/occt/Part.hpp"
#include <QIODevice>
#include <QSet>
#include <optional>
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
    mutable kernel::ResultCache resultCache;
    mutable QMap<QString, assembly::Definition> exactDefinitions;
    mutable QMap<QString, DisplayMesh> assemblyMeshes;
    void point(QString stage) const;
    void createWorkspace();
    void activate();
    void configureWorking();
    void validate(Database &candidate, bool saved = false) const;
    void apply(const QCborMap &delta, qint64 revision);
    void retainHistory();
    void reconcile();
    void cleanupStaging();
    QCborMap mutate(const wire::Envelope &e, const exchange::Control &control);
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
    QCborMap featureState(QString document) const;
    QCborArray profileSketches(QString document, QCborArray definitions) const;
    kernel::Part evaluatePart(QString document, std::optional<QCborArray> candidate = std::nullopt,
                              bool allowFailure = false) const;
    QCborMap referenceCandidates(QString document, QString feature, QString token) const;
    QCborMap cacheStatistics() const {
        return resultCache.statistics();
    }
    void trimResults() {
        resultCache.trim();
        exactDefinitions.clear();
    }
    assembly::Model assemblyModel(QString document) const;
    assembly::Loader assemblyLoader() const;
    QCborMap assemblyResult(QString document, std::optional<QCborMap> candidate = std::nullopt,
                            bool exact = false) const;
    QCborMap assemblyReferences(QString document, QString occurrence) const;
    QCborArray assemblyInterference(QString document) const;
    QCborMap execute(const wire::Envelope &e, const exchange::Control &control = {});
    QCborMap outcome(QString operation) const;
    QCborArray recoveryOffers() const;
    QCborMap inspect(QString path) const;
    QCborMap save(const wire::Envelope &e, QString path, bool saveAs = false);
    QCborMap importGeometry(const wire::Envelope &e, QString path,
                            const exchange::Control &control = {});
    QCborMap exportGeometry(const wire::Envelope &e, QString path,
                            const exchange::Control &control = {});
    drawing::Model drawingModel(QString document) const;
    QCborMap drawingResult(QString document,
                           std::optional<QCborMap> candidate = std::nullopt) const;
    QCborMap exportDrawing(const wire::Envelope &e, QString path,
                           const exchange::Control &control = {});
    QCborMap closure() const;
    QString attach(const wire::Envelope &e, QIODevice &input, qint64 declared,
                   QString classification = "authoritative");
    void retain(QString rootType, QString rootId, QString hash);
    void release(QString rootType, QString rootId);
    void collect();
    static void migrationCopy(QString source, QString destination, const Boundary &observer = {});
};
} // namespace cad::storage

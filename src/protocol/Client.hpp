#pragma once
#include "Mesh.hpp"
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTemporaryDir>
#include <QTimer>
#include <optional>
namespace cad {
using MeshPtr = std::shared_ptr<const DisplayMesh>;
inline bool sameRequest(const wire::Envelope &a, const wire::Envelope &b) {
    return a.request == b.request && a.operation == b.operation && a.epoch == b.epoch &&
           a.context == b.context && a.generation == b.generation && a.revision == b.revision &&
           a.command == b.command && a.payload == b.payload;
}
inline bool visibleResult(const WorkspaceState &state, const wire::Envelope &e, qint64 result) {
    return e.epoch == state.epoch && e.context == state.context &&
           e.generation == state.generation && result >= state.revision;
}
class Client : public QObject {
    Q_OBJECT
    WorkspaceState projection;
    MeshPtr display;
    QTemporaryDir privateRoot;
    QLocalServer server;
    QProcess process;
    QLocalSocket *peer = nullptr;
    wire::Frames frames;
    Transfers transfers;
    QString executable, recoveryDirectory;
    QByteArray secret;
    bool authenticated = false, stopping = false, alive = false;
    struct Pending {
        wire::Envelope request;
        qint64 deadline;
        qint64 result = 0;
        qint64 started = 0;
        bool timed = false;
    };
    QMap<QString, Pending> pending;
    QQueue<wire::Envelope> ordinary;
    std::optional<wire::Envelope> preview;
    QElapsedTimer clock;
    QTimer deadlines;
    int referenceInstances = 1;
    QString lastOperation;
    QCborMap partReply;
    void receive(const QCborMap &map);
    void installProject(const QCborMap &state);
    bool projectReady();
    void fileOperation(QString command, QString path, QCborMap payload);
    void pump();
    void fail(QString code, QString message);
    void stop();
    void enqueue(wire::Envelope request);
    wire::Envelope request(QString command, QCborMap payload = {});

  public:
    explicit Client(QObject *parent = nullptr, QString workerExecutable = {},
                    QString recoveryRoot = {});
    ~Client() override;
    const WorkspaceState &state() const {
        return projection;
    }
    MeshPtr mesh() const {
        return display;
    }
    bool ready() const {
        return authenticated;
    }
    qint64 workerPid() const {
        return process.processId();
    }
    int outstanding() const {
        return pending.size() + ordinary.size() + (preview ? 1 : 0);
    }
    quint64 transferBytes() const {
        return transfers.bytesReserved();
    }
    QString latestOperation() const {
        return lastOperation;
    }
    void start();
    void restart();
    void loadReference(int instances = 1);
    void closeReference();
    void previewReference(double x, double y, double z);
    void cancelPreview();
    void select(Selection selected);
    void measure();
    void queryOperation(QString operation);
    void newProject(QString type, QString name, QString units = "mm", bool replace = false);
    void createDocument(QString type, QString name, QString units = "mm");
    void projectCommand(QString command, QCborMap payload = {});
    void discardSketchPreview();
    void displayPart();
    void displayAssembly();
    void exportDrawing(QString path, bool overwrite = false);
    void migrateProject(QString source, QString destination);
    void cancelPartPreview();
    void switchDocument(QString id);
    void closeDocument();
    void closeProject();
    void openProject(QString path, QString choice = "Saved", bool replace = false);
    void inspectProject(QString path);
    void saveProject(QString path = {}, bool saveAs = false);
    void importGeometry(QString path, QString format, QString units, QString healing,
                        QString expectedSha256);
    void exportGeometry(QString path, QString format, QString units, double deflectionMM,
                        QString body = {}, bool overwrite = false);
    void portableCopy(QString path);
    void cancelExchange();
    void listRecoveries();
    void recoverProject(QString id);
  signals:
    void jobFinished(QString command, QString status, qint64 milliseconds, qint64 transferBytes);
    void changed();
    void meshChanged(cad::MeshPtr mesh);
    void measured(QString text);
    void recoveryOffers(QCborArray offers);
    void projectInspection(QCborMap inspection);
    void projectChanged();
    void sketchPreview(QCborArray sketches);
    void sketchEditPreview(QCborArray sketches, QCborArray affectedConstraints);
    void partResult(QCborMap result);
    void referenceResolution(QCborMap result);
    void exchangeProgress(QString stage, int percent);
    void exchangeReport(QCborMap result);
    void drawingInformation(QCborMap result);
    void drawingAnchors(QCborArray anchors);
    void assemblyInformation(QCborMap result);
    void sketchDiagnosed(QCborArray diagnostics);
    void operationOutcome(QString operation, QString outcome);
};
} // namespace cad
Q_DECLARE_METATYPE(cad::MeshPtr)

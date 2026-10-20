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
    };
    QMap<QString, Pending> pending;
    QQueue<wire::Envelope> ordinary;
    std::optional<wire::Envelope> preview;
    QElapsedTimer clock;
    QTimer deadlines;
    int referenceInstances = 1;
    QString lastOperation;
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
    void switchDocument(QString id);
    void closeDocument();
    void closeProject();
    void openProject(QString path, QString choice = "Saved", bool replace = false);
    void inspectProject(QString path);
    void saveProject(QString path = {}, bool saveAs = false);
    void listRecoveries();
    void recoverProject(QString id);
  signals:
    void changed();
    void meshChanged(cad::MeshPtr mesh);
    void measured(QString text);
    void recoveryOffers(QCborArray offers);
    void projectInspection(QCborMap inspection);
    void projectChanged();
    void operationOutcome(QString operation, QString outcome);
};
} // namespace cad
Q_DECLARE_METATYPE(cad::MeshPtr)

#pragma once
#include <QCborArray>
#include <QMetaType>
#include <QString>
#include <QUuid>
#include <functional>

namespace cad {
inline QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
struct Context {
    QString project = newId();
    QString document = newId();
    QString session = newId();
    bool operator==(const Context &) const = default;
};
struct Selection {
    QString kind, entity, occurrence;
    bool empty() const {
        return entity.isEmpty();
    }
    bool operator==(const Selection &) const = default;
};
enum class Phase { Empty, Loading, Ready, PreviewPending, PreviewReady, Failed, WorkerUnavailable };
struct WorkspaceState {
    Context context;
    QString epoch;
    qint64 revision = 0, generation = 0;
    Phase phase = Phase::Empty;
    Selection selection;
    QString editMode = "Inspect";
    QString job = "Idle";
    QString save = "Reference scene — not a saved project";
    QString message;
    bool stale = false;
    bool projectOpen = false;
    qint64 savedRevision = -1;
    QString savedPath, activeDocument;
    QCborArray documentRecords;
    QStringList openDocuments;
    bool canUndo = false, canRedo = false;
    bool current() const {
        return !stale && (phase == Phase::Ready || phase == Phase::PreviewReady);
    }
};
} // namespace cad
Q_DECLARE_METATYPE(cad::Selection)

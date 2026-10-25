#include "Journal.hpp"
#include "domain/Documents.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
namespace cad::diagnostics {
namespace {
QString safeCommand(QString value) {
    static const QStringList known{"MigrateProject",
                                   "AssemblyInterference",
                                   "AssemblyReferences",
                                   "AttachObject",
                                   "CancelJob",
                                   "CloseDocument",
                                   "CloseProject",
                                   "CollectObjects",
                                   "CommitHistory",
                                   "CreateDocument",
                                   "DetachObject",
                                   "DisplayAssembly",
                                   "DisplayDrawing",
                                   "DisplayPart",
                                   "DrawingAnchors",
                                   "ExportDrawing",
                                   "ExportGeometry",
                                   "GrantFile",
                                   "ImportGeometry",
                                   "InspectClosure",
                                   "InspectProject",
                                   "ListRecoveries",
                                   "LoadReference",
                                   "Measure",
                                   "MeasurePart",
                                   "NewProject",
                                   "OpenProject",
                                   "PortableCopy",
                                   "PreviewAssembly",
                                   "PreviewDrawing",
                                   "PreviewFeatures",
                                   "PreviewHistory",
                                   "PreviewReference",
                                   "PreviewSketches",
                                   "QueryOperation",
                                   "RecoverProject",
                                   "Redo",
                                   "ReferenceCandidates",
                                   "RemoveParameter",
                                   "RenameDocument",
                                   "ResolveReference",
                                   "SaveAs",
                                   "SaveProject",
                                   "SetAssembly",
                                   "SetDocumentUnits",
                                   "SetDrawing",
                                   "SetFeatures",
                                   "SetParameter",
                                   "SetSketches",
                                   "SwitchDocument",
                                   "TrimResults",
                                   "Undo",
                                   "UpdateDrawing"};
    return known.contains(value) ? value : QString("Redacted");
}
} // namespace

Journal::Journal(QString root) : directory(std::move(root)) {
    domain::check(QDir().mkpath(directory), "PermissionDenied",
                  "Cannot create local diagnostic directory");
    QFile::setPermissions(directory,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}
void Journal::record(QString command, QString status, qint64 ms, qint64 bytes) {
    command = safeCommand(command);
    if (!QStringList{"Succeeded", "Failed", "Canceled"}.contains(status))
        status = "Failed";
    QJsonObject row{{"command", command},
                    {"status", status},
                    {"milliseconds", qBound<qint64>(0LL, ms, 86400000LL)},
                    {"transfer_bytes", qBound<qint64>(0LL, bytes, 256LL << 20)},
                    {"utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    auto line = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
    auto path = directory + "/jobs-0.jsonl";
    if (QFileInfo(path).size() + line.size() > fileCap) {
        auto last = directory + "/jobs-3.jsonl";
        if (QFileInfo::exists(last) && !QFile::remove(last))
            return;
        for (int i = 2; i >= 0; --i) {
            auto old = directory + "/jobs-" + QString::number(i) + ".jsonl",
                 next = directory + "/jobs-" + QString::number(i + 1) + ".jsonl";
            if (QFileInfo::exists(old) && !QFile::rename(old, next))
                return;
        }
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(line);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}
QJsonObject Journal::report() const {
    QJsonArray rows;
    for (int i = files - 1; i >= 0; --i) {
        QFile f(directory + "/jobs-" + QString::number(i) + ".jsonl");
        if (!f.open(QIODevice::ReadOnly) || f.size() > fileCap)
            continue;
        while (!f.atEnd()) {
            auto row = QJsonDocument::fromJson(f.readLine(1024)).object();
            if (row.isEmpty())
                continue;
            auto command = row.value("command").toString(), status = row.value("status").toString();
            command = safeCommand(command);
            if (!QStringList{"Succeeded", "Failed", "Canceled"}.contains(status))
                status = "Failed";
            rows.append(QJsonObject{
                {"command", command},
                {"status", status},
                {"milliseconds",
                 qBound<qint64>(0LL, row.value("milliseconds").toInteger(), 86400000LL)},
                {"transfer_bytes",
                 qBound<qint64>(0LL, row.value("transfer_bytes").toInteger(), 256LL << 20)}});
        }
    }
    return {{"format", "datumforge-diagnostics-v1"},
            {"privacy",
             "No model data, names, source paths, document text, raw inputs or error messages"},
            {"jobs", rows},
            {"retention_bytes", files * fileCap}};
}
void Journal::exportTo(QString path) const {
    QSaveFile file(path);
    domain::check(file.open(QIODevice::WriteOnly), "PermissionDenied",
                  "Cannot open diagnostics export");
    auto data = QJsonDocument(report()).toJson();
    domain::check(file.write(data) == data.size() && file.commit(), "StorageError",
                  "Diagnostic export failed");
}
} // namespace cad::diagnostics

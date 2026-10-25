#pragma once
#include <QJsonObject>
#include <QString>
namespace cad::diagnostics {
// Deliberately has no API accepting paths, model text, raw payloads or error messages.
class Journal {
    QString directory;

  public:
    explicit Journal(QString root);
    void record(QString command, QString status, qint64 milliseconds, qint64 transferBytes);
    QJsonObject report() const;
    void exportTo(QString path) const;
    static constexpr qint64 fileCap = 65536;
    static constexpr int files = 4;
};
} // namespace cad::diagnostics

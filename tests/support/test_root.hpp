#pragma once
#include <QDir>
#include <QFileInfo>
#include <stdexcept>
inline QString testRoot(const QString &input) {
    const auto path = QFileInfo(input).canonicalFilePath();
    const auto temp = QFileInfo(QDir::tempPath()).canonicalFilePath();
    if (path.isEmpty() || !path.startsWith(temp + '/') ||
        !QFileInfo(path).fileName().startsWith("cad-step-zero-") ||
        !QFileInfo::exists(path + "/.cad-test-root"))
        throw std::runtime_error("Refused non-disposable/production test root");
    return path;
}

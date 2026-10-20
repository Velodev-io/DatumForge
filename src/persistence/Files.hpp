#pragma once
#include "Database.hpp"
#include <QString>
#include <memory>
namespace cad::storage {
QString fileIdentity(QString path);
QString canonicalDestination(QString path);
QString recoveryRoot(QString path, bool initializeDevelopment = false);
void flushFile(QString path, bool directory = false);
void publish(QString source, QString destination);
void spaceFor(QString directory, qint64 bytes);
class Lock {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Lock(QString directory, QString identity);
    ~Lock();
    Lock(const Lock &) = delete;
};
struct DestinationLocks {
    QString path;
    std::unique_ptr<Lock> name, file;
    DestinationLocks(QString root, QString destination);
};
} // namespace cad::storage

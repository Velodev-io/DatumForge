#include "Files.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef Q_OS_MACOS
#include <sys/mount.h>
#else
#include <sys/statvfs.h>
#endif
#endif
namespace cad::storage {
using domain::check;
QString fileIdentity(QString path) {
#ifdef Q_OS_WIN
    auto h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, 0, nullptr);
    check(h != INVALID_HANDLE_VALUE, "PermissionDenied", "Cannot identify staged file");
    BY_HANDLE_FILE_INFORMATION info{};
    auto ok = GetFileInformationByHandle(h, &info);
    CloseHandle(h);
    check(ok, "StorageError", "Cannot identify staged file");
    return QString("%1:%2:%3")
        .arg(info.dwVolumeSerialNumber)
        .arg(info.nFileIndexHigh)
        .arg(info.nFileIndexLow);
#else
    struct stat info{};
    check(::lstat(path.toUtf8(), &info) == 0 && S_ISREG(info.st_mode), "PermissionDenied",
          "Cannot identify staged regular file");
    return QString("%1:%2").arg(qulonglong(info.st_dev)).arg(qulonglong(info.st_ino));
#endif
}
QString canonicalDestination(QString path) {
    check(QDir::isAbsolutePath(path) && !path.contains(QChar::Null) &&
              path.toUtf8().size() <= int(limits::text),
          "InvalidInput", "Choose an absolute local file path");
    QFileInfo info(path);
    if (info.exists()) {
        check(info.isFile(), "InvalidInput", "Destination must be a regular file");
        return info.canonicalFilePath();
    }
    auto parent = QFileInfo(info.absolutePath()).canonicalFilePath();
    check(!parent.isEmpty() && !info.fileName().isEmpty(), "PermissionDenied",
          "Destination directory must already exist");
    return QDir(parent).filePath(info.fileName());
}
QString recoveryRoot(QString path, bool initializeDevelopment) {
    auto info = QFileInfo(path);
    check(QDir::isAbsolutePath(path) && !path.contains(QChar::Null), "PermissionDenied",
          "Invalid recovery root");
    if (initializeDevelopment) {
        check(info.fileName() == "recovery-v1", "PermissionDenied",
              "Development root must be namespaced");
        check(QDir().mkpath(path), "PermissionDenied", "Cannot create development recovery root");
        QFile marker(path + "/.cad-development-root");
        if (!marker.exists()) {
            check(marker.open(QIODevice::WriteOnly | QIODevice::NewOnly), "PermissionDenied",
                  "Cannot mark development recovery root");
            marker.write("local.solidworks-clone.dev/recovery-v1\n");
            marker.close();
        }
    }
    path = QFileInfo(path).canonicalFilePath();
    check(!path.isEmpty(), "PermissionDenied", "Recovery root does not exist");
    bool test = QFileInfo::exists(path + "/.cad-test-root");
    if (test) {
        auto temp = QFileInfo(QDir::tempPath()).canonicalFilePath();
        check(path.startsWith(temp + '/') &&
                  (QFileInfo(path).fileName().startsWith("cad-module2-") ||
                   QFileInfo(path).fileName().startsWith("cad-module3-") ||
                   QFileInfo(path).fileName().startsWith("cad-module4-")),
              "PermissionDenied", "Refused unmarked/non-disposable test root");
    } else {
        QFile marker(path + "/.cad-development-root");
        check(marker.open(QIODevice::ReadOnly) &&
                  marker.readAll() == "local.solidworks-clone.dev/recovery-v1\n",
              "PermissionDenied", "Refused release/unmarked recovery root");
    }
    check(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                          QFileDevice::ExeOwner),
          "PermissionDenied", "Cannot secure recovery root");
    check(QDir().mkpath(path + "/locks"), "PermissionDenied", "Cannot create lock directory");
    QFile::setPermissions(path + "/locks",
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return path;
}
void spaceFor(QString path, qint64 bytes) {
    qint64 available = -1;
    bool readonly = true;
#ifdef Q_OS_MACOS
    struct statfs info{};
    check(::statfs(path.toUtf8(), &info) == 0, "StorageError",
          "Cannot query destination filesystem");
    available = qint64(info.f_bavail) * info.f_bsize;
    readonly = bool(info.f_flags & MNT_RDONLY);
#elif defined(Q_OS_UNIX)
    struct statvfs info{};
    check(::statvfs(path.toUtf8(), &info) == 0, "StorageError",
          "Cannot query destination filesystem");
    available = qint64(info.f_bavail) * info.f_frsize;
    readonly = bool(info.f_flag & ST_RDONLY);
#else
    QStorageInfo disk(path);
    disk.refresh();
    check(disk.isValid() && disk.isReady(), "StorageError", "Cannot query destination filesystem");
    available = disk.bytesAvailable();
    readonly = disk.isReadOnly();
#endif
    check(!readonly, "PermissionDenied", "Destination filesystem is read-only");
    check(bytes >= 0 && available >= bytes + (64LL << 20), "StorageFull",
          "Insufficient free space for working, snapshot, staging and backup files");
}
void flushFile(QString path, bool directory) {
#ifdef Q_OS_WIN
    auto h =
        CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr);
    check(h != INVALID_HANDLE_VALUE, "PermissionDenied", "Cannot open file for flush");
    bool okay = FlushFileBuffers(h);
    CloseHandle(h);
    check(okay, "StorageError", "FlushFileBuffers failed");
#else
    int fd = ::open(path.toUtf8(), O_RDONLY);
    check(fd >= 0, "PermissionDenied", "Cannot open file for flush");
    int result = ::fsync(fd);
#ifdef Q_OS_MACOS
    if (!directory && result == 0)
        result = ::fcntl(fd, F_FULLFSYNC);
#endif
    ::close(fd);
    check(result == 0, "StorageError", "Filesystem flush failed");
#endif
}
void publish(QString source, QString destination) {
#ifdef Q_OS_WIN
    check(MoveFileExW(reinterpret_cast<LPCWSTR>(source.utf16()),
                      reinterpret_cast<LPCWSTR>(destination.utf16()),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
          "StorageError", "Atomic snapshot replacement failed");
#else
    check(::rename(source.toUtf8(), destination.toUtf8()) == 0, "StorageError",
          "Atomic snapshot replacement failed");
    flushFile(QFileInfo(destination).absolutePath(), true);
#endif
}
struct Lock::Impl {
#ifdef Q_OS_WIN
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~Impl() {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
#else
    int fd = -1;
    ~Impl() {
        if (fd >= 0)
            ::close(fd);
    }
#endif
};
Lock::Lock(QString root, QString identity) : impl(std::make_unique<Impl>()) {
    auto digest = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
    auto path = root + "/locks/" + QString::fromLatin1(digest) + ".lock";
#ifdef Q_OS_WIN
    impl->handle =
        CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    check(impl->handle != INVALID_HANDLE_VALUE, "PermissionDenied", "Cannot open writer lock");
    OVERLAPPED overlap{};
    check(LockFileEx(impl->handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                     &overlap),
          "ProjectLocked", "Project is already open for editing");
#else
    impl->fd = ::open(path.toUtf8(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    check(impl->fd >= 0, "PermissionDenied", "Cannot open writer lock");
    check(::flock(impl->fd, LOCK_EX | LOCK_NB) == 0, "ProjectLocked",
          "Project is already open for editing");
#endif
}
Lock::~Lock() = default;
DestinationLocks::DestinationLocks(QString root, QString destination)
    : path(canonicalDestination(destination)) {
    auto key = path.normalized(QString::NormalizationForm_C).toCaseFolded();
#ifdef Q_OS_WIN
    key = key.toCaseFolded();
#endif
    name = std::make_unique<Lock>(root, "path:" + key);
    if (QFileInfo::exists(path)) {
#ifdef Q_OS_WIN
        auto h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, 0, nullptr);
        check(h != INVALID_HANDLE_VALUE, "PermissionDenied", "Cannot identify saved project");
        BY_HANDLE_FILE_INFORMATION stat{};
        auto ok = GetFileInformationByHandle(h, &stat);
        CloseHandle(h);
        check(ok, "StorageError", "Cannot identify saved project");
        auto identity = QString("inode:%1:%2:%3")
                            .arg(stat.dwVolumeSerialNumber)
                            .arg(stat.nFileIndexHigh)
                            .arg(stat.nFileIndexLow);
#else
        struct stat stat{};
        check(::stat(path.toUtf8(), &stat) == 0 && S_ISREG(stat.st_mode), "InvalidInput",
              "Cannot identify saved project");
        auto identity =
            QString("inode:%1:%2").arg(qulonglong(stat.st_dev)).arg(qulonglong(stat.st_ino));
#endif
        file = std::make_unique<Lock>(root, identity);
    }
}
} // namespace cad::storage

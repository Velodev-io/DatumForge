#pragma once
#include "Envelope.hpp"
#include <QMap>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <memory>
namespace cad {
struct Instance {
    QString id;
    QVector3D translation;
    QQuaternion rotation;
    int firstTriangle = 0, triangleCount = -1, firstEdge = 0, edgeCount = -1;
    QVector3D map(QVector3D point) const {
        return rotation.rotatedVector(point) + translation;
    }
};
struct DisplayMesh {
    QVector<QVector3D> vertices, edges;
    QVector<quint32> indices, triangleRoles;
    QStringList faceIds, edgeIds;
    QVector<Instance> instances;
    QVector3D minimum, maximum;
    void validate() const;
};
struct PackedMesh {
    QCborMap manifest;
    QByteArray bytes;
};
PackedMesh pack(const DisplayMesh &mesh);
std::shared_ptr<const DisplayMesh> unpack(const QCborMap &manifest, const QByteArray &bytes);
class Transfers {
    struct Entry {
        wire::Envelope envelope;
        QCborMap manifest;
        QByteArray bytes;
        quint64 size;
    };
    QMap<QString, Entry> active;
    quint64 reserved = 0;

  public:
    void begin(const wire::Envelope &envelope, const QCborMap &manifest);
    std::shared_ptr<const DisplayMesh> append(const wire::Envelope &envelope,
                                              const QCborMap &payload);
    void clear() {
        active.clear();
        reserved = 0;
    }
    quint64 bytesReserved() const {
        return reserved;
    }
};
} // namespace cad

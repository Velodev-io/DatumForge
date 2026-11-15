#include "Mesh.hpp"
#include <bit>
namespace cad {
using wire::require;
namespace {
void word(QByteArray &out, quint32 value) {
    quint32 le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char *>(&le), 4);
}
void point(QByteArray &out, QVector3D v) {
    for (float x : {v.x(), v.y(), v.z()})
        word(out, std::bit_cast<quint32>(x));
}
quint32 read(const QByteArray &b, qsizetype &offset) {
    require(offset + 4 <= b.size(), "truncated mesh");
    auto value = qFromLittleEndian<quint32>(b.constData() + offset);
    offset += 4;
    return value;
}
QVector3D readPoint(const QByteArray &b, qsizetype &o) {
    auto x = std::bit_cast<float>(read(b, o));
    auto y = std::bit_cast<float>(read(b, o));
    auto z = std::bit_cast<float>(read(b, o));
    return {x, y, z};
}
bool finite(QVector3D v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}
QStringList ids(const QCborMap &m, QString key) {
    require(m.value(key).isArray(), "ID array");
    QStringList out;
    QSet<QString> seen;
    for (const auto &v : m.value(key).toArray()) {
        require(v.isString() && !v.toString().isEmpty() && !seen.contains(v.toString()),
                "unique entity IDs");
        seen.insert(v.toString());
        out.append(v.toString());
    }
    return out;
}
} // namespace
void DisplayMesh::validate() const {
    require(!vertices.empty() && indices.size() % 3 == 0 &&
                triangleRoles.size() == indices.size() / 3 && edges.size() % 2 == 0,
            "mesh cardinality");
    require(faceIds.size() <= int(limits::collection) && edgeIds.size() == edges.size() / 2,
            "entity count");
    require(instances.size() > 0 && instances.size() <= 1000, "instance count");
    require(finite(minimum) && finite(maximum) && minimum.x() <= maximum.x() &&
                minimum.y() <= maximum.y() && minimum.z() <= maximum.z(),
            "bounds");
    for (auto v : vertices) {
        require(finite(v), "nonfinite vertex");
        for (int i = 0; i < 3; ++i)
            require(v[i] >= minimum[i] - 1e-4 && v[i] <= maximum[i] + 1e-4,
                    "vertex outside bounds");
    }
    for (auto v : edges) {
        require(finite(v), "nonfinite edge");
        for (int i = 0; i < 3; ++i)
            require(v[i] >= minimum[i] - 1e-4 && v[i] <= maximum[i] + 1e-4, "edge outside bounds");
    }
    for (auto i : indices)
        require(i < quint32(vertices.size()), "mesh index range");
    for (auto role : triangleRoles)
        require(role < quint32(faceIds.size()), "face role range");
    QSet<QString> seen;
    for (const auto &i : instances) {
        require(!i.id.isEmpty() && !seen.contains(i.id) && finite(i.translation),
                "instance identity/transform");
        require(std::isfinite(i.rotation.scalar()) && std::isfinite(i.rotation.x()) &&
                    std::isfinite(i.rotation.y()) && std::isfinite(i.rotation.z()) &&
                    std::abs(i.rotation.lengthSquared() - 1) < 1e-5,
                "normalized instance rotation");
        require(i.firstTriangle >= 0 && i.firstTriangle <= triangleRoles.size() &&
                    i.triangleCount >= -1 &&
                    (i.triangleCount < 0
                         ? i.firstTriangle == 0
                         : i.triangleCount <= triangleRoles.size() - i.firstTriangle),
                "instance triangle range");
        require(
            i.firstEdge >= 0 && i.firstEdge <= edgeIds.size() && i.edgeCount >= -1 &&
                (i.edgeCount < 0 ? i.firstEdge == 0 : i.edgeCount <= edgeIds.size() - i.firstEdge),
            "instance edge range");
        seen.insert(i.id);
    }
}
PackedMesh pack(const DisplayMesh &mesh) {
    mesh.validate();
    require(quint64(mesh.vertices.size()) * 12 + quint64(mesh.indices.size()) * 4 +
                    quint64(mesh.edges.size()) * 12 + quint64(mesh.triangleRoles.size()) * 4 <=
                limits::segment,
            "display segment cap");
    PackedMesh out;
    for (auto v : mesh.vertices)
        point(out.bytes, v);
    for (auto i : mesh.indices)
        word(out.bytes, i);
    for (auto v : mesh.edges)
        point(out.bytes, v);
    for (auto i : mesh.triangleRoles)
        word(out.bytes, i);
    require(out.bytes.size() <= qsizetype(limits::segment), "display segment cap");
    auto &m = out.manifest;
    m.insert(QStringLiteral("transfer_id"), newId());
    bool extended = false;
    for (auto &i : mesh.instances)
        extended = extended || !i.rotation.isIdentity() || i.triangleCount >= 0 || i.edgeCount >= 0;
    m.insert(QStringLiteral("format"), extended ? "xyz-f32le/u32le-v2" : "xyz-f32le/u32le-v1");
    m.insert(QStringLiteral("bytes"), out.bytes.size());
    m.insert(QStringLiteral("vertices"), mesh.vertices.size());
    m.insert(QStringLiteral("indices"), mesh.indices.size());
    m.insert(QStringLiteral("edges"), mesh.edges.size());
    m.insert(QStringLiteral("checksum"),
             QCryptographicHash::hash(out.bytes, QCryptographicHash::Sha256));
    m.insert(QStringLiteral("faces"), QCborArray::fromStringList(mesh.faceIds));
    m.insert(QStringLiteral("edge_ids"), QCborArray::fromStringList(mesh.edgeIds));
    QCborArray bounds;
    for (float x : {mesh.minimum.x(), mesh.minimum.y(), mesh.minimum.z(), mesh.maximum.x(),
                    mesh.maximum.y(), mesh.maximum.z()})
        bounds.append(double(x));
    m.insert(QStringLiteral("bounds"), bounds);
    QCborArray instances;
    for (const auto &i : mesh.instances) {
        QCborMap item;
        item.insert(QStringLiteral("id"), i.id);
        QCborArray v;
        for (float x : {i.translation.x(), i.translation.y(), i.translation.z()})
            v.append(double(x));
        item.insert(QStringLiteral("translation_mm"), v);
        if (extended) {
            item.insert(QStringLiteral("rotation_wxyz"),
                        QCborArray{double(i.rotation.scalar()), double(i.rotation.x()),
                                   double(i.rotation.y()), double(i.rotation.z())});
            item.insert(QStringLiteral("first_triangle"), i.firstTriangle);
            item.insert(QStringLiteral("triangle_count"),
                        i.triangleCount < 0 ? mesh.triangleRoles.size() : i.triangleCount);
            item.insert(QStringLiteral("first_edge"), i.firstEdge);
            item.insert(QStringLiteral("edge_count"),
                        i.edgeCount < 0 ? mesh.edgeIds.size() : i.edgeCount);
        }
        instances.append(item);
    }
    m.insert(QStringLiteral("instances"), instances);
    return out;
}
namespace {
void manifestCheck(const QCborMap &m) {
    wire::keys(m, {"transfer_id", "format", "bytes", "vertices", "indices", "edges", "checksum",
                   "faces", "edge_ids", "bounds", "instances"});
    require(wire::uuid(wire::text(m, "transfer_id")) &&
                QStringList{"xyz-f32le/u32le-v1", "xyz-f32le/u32le-v2"}.contains(
                    wire::text(m, "format")),
            "transfer format");
    auto size = wire::integer(m, "bytes", limits::segment),
         v = wire::integer(m, "vertices", limits::segment / 12),
         i = wire::integer(m, "indices", limits::segment / 4),
         e = wire::integer(m, "edges", limits::segment / 12);
    require(v > 0 && i > 0 && i % 3 == 0 && e % 2 == 0 &&
                v * 12 + i * 4 + e * 12 + (i / 3) * 4 == size,
            "mesh declared size");
    require(m.value("checksum").isByteArray() && m.value("checksum").toByteArray().size() == 32,
            "checksum type");
    require(m.value("bounds").isArray() && m.value("bounds").toArray().size() == 6 &&
                m.value("instances").isArray() && m.value("instances").toArray().size() <= 1000,
            "mesh metadata");
    ids(m, "faces");
    require(ids(m, "edge_ids").size() == e / 2, "edge ID count");
}
} // namespace
std::shared_ptr<const DisplayMesh> unpack(const QCborMap &m, const QByteArray &bytes) {
    manifestCheck(m);
    require(wire::integer(m, "bytes") == bytes.size() &&
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256) ==
                    m.value("checksum").toByteArray(),
            "mesh checksum/size");
    auto out = std::make_shared<DisplayMesh>();
    qsizetype offset = 0;
    for (qint64 n = wire::integer(m, "vertices"); n--;)
        out->vertices.append(readPoint(bytes, offset));
    for (qint64 n = wire::integer(m, "indices"); n--;)
        out->indices.append(read(bytes, offset));
    for (qint64 n = wire::integer(m, "edges"); n--;)
        out->edges.append(readPoint(bytes, offset));
    for (qint64 n = out->indices.size() / 3; n--;)
        out->triangleRoles.append(read(bytes, offset));
    out->faceIds = ids(m, "faces");
    out->edgeIds = ids(m, "edge_ids");
    auto bounds = m.value("bounds").toArray();
    for (const auto &x : bounds)
        require(x.isDouble() && std::isfinite(x.toDouble()), "finite bounds");
    out->minimum = {float(bounds[0].toDouble()), float(bounds[1].toDouble()),
                    float(bounds[2].toDouble())};
    out->maximum = {float(bounds[3].toDouble()), float(bounds[4].toDouble()),
                    float(bounds[5].toDouble())};
    for (const auto &x : m.value("instances").toArray()) {
        require(x.isMap(), "instance map");
        auto item = x.toMap();
        bool extended = wire::text(m, "format") == "xyz-f32le/u32le-v2";
        wire::keys(item, extended ? QSet<QString>{"id", "translation_mm", "rotation_wxyz",
                                                  "first_triangle", "triangle_count", "first_edge",
                                                  "edge_count"}
                                  : QSet<QString>{"id", "translation_mm"});
        auto v = item.value("translation_mm").toArray();
        require(v.size() == 3, "translation");
        for (const auto &f : v)
            require(f.isDouble() && std::isfinite(f.toDouble()) && std::abs(f.toDouble()) < 1e6,
                    "translation range");
        out->instances.append(
            {wire::text(item, "id"),
             {float(v[0].toDouble()), float(v[1].toDouble()), float(v[2].toDouble())}});
    }
    if (wire::text(m, "format") == "xyz-f32le/u32le-v2") {
        auto entries = m.value("instances").toArray();
        for (int i = 0; i < entries.size(); ++i) {
            auto item = entries[i].toMap();
            auto q = item.value("rotation_wxyz").toArray();
            require(q.size() == 4, "rotation coordinates");
            for (auto n : q)
                require((n.isDouble() || n.isInteger()) && std::isfinite(n.toDouble()),
                        "finite rotation");
            auto &o = out->instances[i];
            o.rotation = QQuaternion(float(q[0].toDouble()), float(q[1].toDouble()),
                                     float(q[2].toDouble()), float(q[3].toDouble()));
            o.firstTriangle = int(wire::integer(item, "first_triangle", out->triangleRoles.size()));
            o.triangleCount = int(wire::integer(item, "triangle_count", out->triangleRoles.size()));
            o.firstEdge = int(wire::integer(item, "first_edge", out->edgeIds.size()));
            o.edgeCount = int(wire::integer(item, "edge_count", out->edgeIds.size()));
        }
    }
    out->validate();
    return out;
}
void Transfers::begin(const wire::Envelope &e, const QCborMap &m) {
    manifestCheck(m);
    auto id = wire::text(m, "transfer_id");
    auto size = quint64(wire::integer(m, "bytes"));
    require(!active.contains(id) && active.size() < limits::outstanding &&
                size <= limits::transfer - reserved,
            "aggregate transfer budget");
    reserved += size;
    active.insert(id, {e, m, {}, size});
}
std::shared_ptr<const DisplayMesh> Transfers::append(const wire::Envelope &e, const QCborMap &p) {
    wire::keys(p, {"transfer_id", "offset", "bytes"});
    auto id = wire::text(p, "transfer_id");
    require(active.contains(id), "unknown transfer");
    auto &a = active[id];
    require(a.envelope.request == e.request && a.envelope.operation == e.operation &&
                a.envelope.epoch == e.epoch && a.envelope.context == e.context &&
                a.envelope.generation == e.generation && a.envelope.revision == e.revision,
            "transfer correlation");
    auto offset = quint64(wire::integer(p, "offset"));
    require(p.value("bytes").isByteArray(), "chunk bytes");
    auto chunk = p.value("bytes").toByteArray();
    require(!chunk.isEmpty() && chunk.size() <= qsizetype(limits::text) &&
                offset == quint64(a.bytes.size()) && offset <= a.size &&
                quint64(chunk.size()) <= a.size - offset,
            "chunk range/order");
    a.bytes.append(chunk);
    if (quint64(a.bytes.size()) < a.size)
        return {};
    auto result = unpack(a.manifest, a.bytes);
    reserved -= a.size;
    active.remove(id);
    return result;
}
} // namespace cad

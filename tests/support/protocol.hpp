#pragma once
#include "limits.hpp"
#include <QByteArray>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QMap>
#include <QSet>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace wire {
inline void require(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
// Definite-length, bounded CBOR profile. Scan before Qt allocates a value tree.
class Preflight {
    const QByteArray &bytes;
    qsizetype pos = 0;
    quint8 byte() {
        require(pos < bytes.size(), "truncated CBOR");
        return quint8(bytes[pos++]);
    }
    quint64 argument(unsigned info) {
        if (info < 24)
            return info;
        require(info <= 27, "indefinite/reserved CBOR forbidden");
        quint64 value = 0;
        for (unsigned i = 0; i < (1U << (info - 24)); ++i)
            value = (value << 8) | byte();
        return value;
    }
    void value(int depth) {
        require(depth <= limits::nesting, "CBOR nesting cap");
        auto initial = byte();
        auto major = initial >> 5;
        auto info = initial & 31;
        auto length = argument(info);
        if (major <= 1)
            return;
        if (major == 2 || major == 3) {
            require(length <= limits::text, "CBOR string cap; bulk uses chunk contract");
            require(length <= quint64(bytes.size() - pos), "truncated string");
            pos += qsizetype(length);
        } else if (major == 4 || major == 5) {
            require(length <= limits::collection, "CBOR collection cap");
            for (quint64 i = 0; i < length * (major == 5 ? 2 : 1); ++i)
                value(depth + 1);
        } else if (major == 7) {
            require(info == 20 || info == 21 || info == 22 || info == 26 || info == 27,
                    "unsupported CBOR scalar");
            if (info == 27) {
                double number;
                std::memcpy(&number, &length, 8);
                require(std::isfinite(number), "nonfinite CBOR");
            } else if (info == 26) {
                auto bits = quint32(length);
                float number;
                std::memcpy(&number, &bits, 4);
                require(std::isfinite(number), "nonfinite CBOR");
            }
        } else
            require(false, "CBOR tags forbidden");
    }

  public:
    explicit Preflight(const QByteArray &input) : bytes(input) {}
    void check() {
        value(0);
        require(pos == bytes.size(), "trailing CBOR");
    }
};
inline QByteArray frame(const QCborMap &map) {
    auto data = QCborValue(map).toCbor();
    require(data.size() <= qsizetype(limits::control), "control cap");
    QByteArray result(4, '\0');
    qToBigEndian(quint32(data.size()), result.data());
    return result + data;
}
inline QCborMap decode(const QByteArray &payload) {
    require(payload.size() <= qsizetype(limits::control), "control cap");
    Preflight(payload).check();
    QCborParserError error;
    auto value = QCborValue::fromCbor(payload, &error);
    require(error.error == QCborError::NoError && value.isMap(), "invalid map");
    QSet<QString> seen;
    auto map = value.toMap();
    for (auto it = map.begin(); it != map.end(); ++it) {
        require(it.key().isString() && !seen.contains(it.key().toString()),
                "duplicate/nontext key");
        seen.insert(it.key().toString());
    }
    return map;
}
class Frames {
    QByteArray buffer;
    quint32 expected = 0;

  public:
    QList<QCborMap> feed(const QByteArray &input) {
        QList<QCborMap> result;
        qsizetype cursor = 0;
        while (cursor < input.size()) {
            auto wanted = expected ? qsizetype(expected) : 4;
            auto take = qMin(wanted - buffer.size(), input.size() - cursor);
            buffer.append(input.constData() + cursor, take);
            cursor += take;
            if (buffer.size() != wanted)
                continue;
            if (!expected) {
                expected = qFromBigEndian<quint32>(buffer.constData());
                require(expected > 0 && expected <= limits::control,
                        "frame length rejected before payload allocation");
                buffer.clear();
            } else {
                result.append(decode(buffer));
                buffer.clear();
                expected = 0;
                require(result.size() <= limits::outstanding, "request batch cap");
            }
        }
        return result;
    }
};
inline QCborMap request(QString epoch, qint64 generation = 1, QString command = "Box") {
    QCborMap m;
    m.insert(QStringLiteral("protocol_version"), 1);
    m.insert(QStringLiteral("worker_epoch"), epoch);
    m.insert(QStringLiteral("preview_generation"), generation);
    m.insert(QStringLiteral("command_type"), command);
    return m;
}
inline void validate(const QCborMap &map, const QString &epoch) {
    const QSet<QString> fields{"protocol_version", "worker_epoch", "preview_generation",
                               "command_type"};
    require(map.size() == fields.size(), "unknown/missing mandatory fields");
    for (auto it = map.begin(); it != map.end(); ++it)
        require(fields.contains(it.key().toString()), "unknown field");
    require(map.value("protocol_version").isInteger() &&
                map.value("protocol_version").toInteger() == 1,
            "version");
    require(map.value("worker_epoch").isString() && map.value("worker_epoch").toString() == epoch,
            "epoch");
    require(map.value("preview_generation").isInteger() &&
                map.value("preview_generation").toInteger() >= 0,
            "generation");
    require(map.value("command_type").toString() == "Box" ||
                map.value("command_type").toString() == "Measure",
            "unknown command");
}
struct Session {
    QString epoch;
    qint64 generation = 0;
    qint64 revision = 0;
    bool accept(const QString &incomingEpoch, qint64 gen, qint64 rev) {
        if (incomingEpoch != epoch || gen < generation || rev < revision)
            return false;
        generation = gen;
        revision = rev;
        return true;
    }
};
struct Mesh {
    QByteArray vertices; // packed float xyz; no native pointers or BRep
    QByteArray indices;  // packed uint32 triangles; local byte order in spike only
    QByteArray checksum;
    int faces = 0;
    void check() const {
        require(vertices.size() <= qsizetype(limits::segment) &&
                    indices.size() <= qsizetype(limits::segment),
                "segment cap");
        require(vertices.size() % 12 == 0 && indices.size() % 12 == 0, "mesh stride");
        require(QCryptographicHash::hash(vertices + indices, QCryptographicHash::Sha256) ==
                    checksum,
                "mesh checksum");
        for (qsizetype i = 0; i < vertices.size(); i += 4) {
            float value;
            std::memcpy(&value, vertices.constData() + i, 4);
            require(std::isfinite(value), "nonfinite vertex");
        }
        for (qsizetype i = 0; i < indices.size(); i += 4) {
            quint32 index;
            std::memcpy(&index, indices.constData() + i, 4);
            require(index < quint32(vertices.size() / 12), "mesh index range");
        }
    }
};
inline void validateEdges(const QByteArray &edges) {
    require(edges.size() % 24 == 0 && edges.size() <= qsizetype(limits::segment),
            "edge mapping cap/stride");
    for (qsizetype i = 0; i < edges.size(); i += 4) {
        float value;
        std::memcpy(&value, edges.constData() + i, 4);
        require(std::isfinite(value), "nonfinite edge vertex");
    }
}
class Buffers {
    QMap<QString, QByteArray> buffers;
    std::size_t used = 0;

  public:
    void add(const QString &id, QByteArray data) {
        require(!id.contains('/') && !id.contains('\\') && !id.isEmpty() && !buffers.contains(id),
                "opaque unique buffer ID required");
        require(std::size_t(data.size()) <= limits::segment &&
                    std::size_t(data.size()) <= limits::transfer - used,
                "buffer budget");
        used += data.size();
        buffers.insert(id, std::move(data));
    }
    void release(const QString &id) {
        used -= buffers.take(id).size();
    }
    void restart() {
        buffers.clear();
        used = 0;
    }
    std::size_t bytes() const {
        return used;
    }
    static bool range(quint64 offset, quint64 size, quint64 capacity) {
        return offset <= capacity && size <= capacity - offset;
    }
};
class Chunks {
    QByteArray data;
    quint64 expected = 0;
    QByteArray checksum;
    QString epoch;

  public:
    void begin(const QString &newEpoch, quint64 size, QByteArray digest) {
        require(size <= limits::segment && digest.size() == 32, "chunk manifest cap/digest");
        epoch = newEpoch;
        expected = size;
        checksum = std::move(digest);
        data.clear();
    }
    void append(const QString &incomingEpoch, quint64 offset, const QByteArray &chunk) {
        require(incomingEpoch == epoch && offset == quint64(data.size()), "old/out-of-order chunk");
        require(chunk.size() <= qsizetype(limits::text) &&
                    Buffers::range(offset, chunk.size(), expected),
                "chunk range/cap");
        data.append(chunk);
    }
    bool complete() const {
        return quint64(data.size()) == expected;
    }
    QByteArray finish() {
        require(complete() &&
                    QCryptographicHash::hash(data, QCryptographicHash::Sha256) == checksum,
                "incomplete/corrupt transfer");
        auto result = std::move(data);
        expected = 0;
        epoch.clear();
        checksum.clear();
        return result;
    }
    void abandon() {
        data.clear();
        expected = 0;
        epoch.clear();
        checksum.clear();
    }
};
} // namespace wire

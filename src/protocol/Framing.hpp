#pragma once
#include "Limits.hpp"
#include <QByteArray>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QSet>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace cad::wire {
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
    auto map = value.toMap();
    const auto checkKeys = [](auto &&self, const QCborValue &item) -> void {
        if (item.isMap()) {
            QSet<QString> seen;
            const auto nested = item.toMap();
            for (auto it = nested.begin(); it != nested.end(); ++it) {
                require(it.key().isString() && !seen.contains(it.key().toString()),
                        "duplicate/nontext key");
                seen.insert(it.key().toString());
                self(self, it.value());
            }
        } else if (item.isArray())
            for (const auto &entry : item.toArray())
                self(self, entry);
    };
    checkKeys(checkKeys, value);
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
                require(result.size() <= limits::outstanding, "frame batch cap");
            }
        }
        return result;
    }
};
} // namespace cad::wire

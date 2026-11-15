#pragma once
#include "features/Model.hpp"
#include <QPointF>
#include <QtEndian>
#include <bit>
namespace cad::drawing {
inline void appendPoint(QByteArray &bytes, double x, double y) {
    for (double v : {x, y}) {
        quint32 bits = qToLittleEndian(std::bit_cast<quint32>(float(v)));
        bytes.append(reinterpret_cast<const char *>(&bits), 4);
    }
}
inline QList<QPointF> points(const QCborValue &path) {
    domain::check(path.isByteArray(), "InvalidInput", "Packed vector path required");
    auto bytes = path.toByteArray();
    domain::check(bytes.size() >= 16 && bytes.size() % 8 == 0 && bytes.size() <= 480000,
                  "ResourceLimit", "Invalid vector path size");
    QList<QPointF> out;
    for (qsizetype at = 0; at < bytes.size(); at += 8) {
        auto x = std::bit_cast<float>(qFromLittleEndian<quint32>(bytes.constData() + at)),
             y = std::bit_cast<float>(qFromLittleEndian<quint32>(bytes.constData() + at + 4));
        domain::check(std::isfinite(x) && std::isfinite(y) && std::abs(x) <= 2000 &&
                          std::abs(y) <= 2000,
                      "InvalidInput", "Invalid vector coordinates");
        out.append({x, y});
    }
    return out;
}
} // namespace cad::drawing

namespace cad::drawing {
inline void validateDisplay(const QCborMap &result) {
    wire::keys(result,
               {"sheets", "current", "raster_fallback", "vector_chord_error_mm", "bom_policy"});
    domain::check(
        result.value("sheets").isArray() && result.value("sheets").toArray().size() <= 8 &&
            result.value("current").isBool() && result.value("raster_fallback").isBool() &&
            !result.value("raster_fallback").toBool(),
        "InvalidInput", "Invalid drawing display header");
    int total = 0;
    for (auto value : result.value("sheets").toArray()) {
        domain::check(value.isMap(), "InvalidInput", "Drawing sheet map required");
        auto s = value.toMap();
        double w = features::number(s, "width_mm", 100, 2000),
               h = features::number(s, "height_mm", 100, 2000);
        features::number(s, "scale", .001, 100);
        domain::check(s.value("views").isArray() && s.value("views").toArray().size() <= 8 &&
                          s.value("annotations").isArray() &&
                          s.value("annotations").toArray().size() <= 30,
                      "ResourceLimit", "Drawing display collection cap exceeded");
        for (auto v : s.value("views").toArray())
            for (auto key : {"visible", "hidden_paths"}) {
                auto paths = v.toMap().value(QString::fromLatin1(key));
                if (paths.isUndefined())
                    continue;
                domain::check(paths.isArray(), "InvalidInput", "Vector path array required");
                for (auto p : paths.toArray())
                    for (auto point : points(p)) {
                        domain::check(++total <= 60000 && point.x() >= 0 && point.x() <= w &&
                                          point.y() >= 0 && point.y() <= h,
                                      "ResourceLimit", "Drawing path count/bounds exceeded");
                    }
            }
        for (auto v : s.value("annotations").toArray()) {
            auto a = v.toMap();
            domain::check(a.value("label").isString() && a.value("label").toString().size() <= 1100,
                          "ResourceLimit", "Drawing label cap exceeded");
            features::number(a, "x_mm", 0, w);
            features::number(a, "y_mm", 0, h);
        }
        domain::check(!s.contains(QStringLiteral("bom")) ||
                          (s.value("bom").isArray() && s.value("bom").toArray().size() <= 100),
                      "ResourceLimit", "Drawing BOM cap exceeded");
    }
}
} // namespace cad::drawing

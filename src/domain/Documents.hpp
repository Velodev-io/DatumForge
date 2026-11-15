#pragma once
#include "protocol/Envelope.hpp"
#include <QRegularExpression>
namespace cad::domain {
class Error : public std::runtime_error {
  public:
    QString code;
    Error(QString c, QString message)
        : std::runtime_error(message.toStdString()), code(std::move(c)) {}
};
inline void check(bool condition, QString code, QString message) {
    if (!condition)
        throw Error(code, message);
}
inline QString label(QString value) {
    check(!value.trimmed().isEmpty() && value.toUtf8().size() <= 128 &&
              !value.contains(QChar::Null),
          "InvalidInput", "Names must contain 1–128 UTF-8 bytes");
    return value;
}
inline QString id(QString value) {
    check(wire::uuid(value), "InvalidInput", "A canonical UUID is required");
    return value;
}
inline QString kind(QString value) {
    check(QStringList{"Part", "Assembly", "Drawing"}.contains(value), "UnsupportedOperation",
          "Document type must be Part, Assembly or Drawing");
    return value;
}
inline QString units(QString value) {
    check(QStringList{"mm", "cm", "m", "in"}.contains(value), "UnitMismatch",
          "Unsupported document display units");
    return value;
}
inline double toSI(double value, QString dimension, QString unit) {
    check(std::isfinite(value), "InvalidInput", "Parameter must be finite");
    double factor = 0;
    if (dimension == "length") {
        if (unit == "m")
            factor = 1;
        if (unit == "mm")
            factor = .001;
        if (unit == "cm")
            factor = .01;
        if (unit == "in")
            factor = .0254;
    } else if (dimension == "angle") {
        if (unit == "rad")
            factor = 1;
        if (unit == "deg")
            factor = 3.14159265358979323846 / 180;
    } else if (dimension == "mass") {
        if (unit == "kg")
            factor = 1;
        if (unit == "g")
            factor = .001;
    } else if (dimension == "scalar" && unit == "1")
        factor = 1;
    check(factor > 0, "UnitMismatch", "Display unit does not match the parameter dimension");
    double result = value * factor;
    check(std::isfinite(result) && std::abs(result) <= 1e9, "InvalidInput",
          "Parameter is outside the supported SI range");
    return result;
}
struct Parameter {
    QString name, dimension;
    double value = 0;
    static Parameter parse(const QCborMap &p) {
        wire::keys(p, {"name", "dimension", "value_si"});
        Parameter out;
        out.name = wire::text(p, "name");
        out.dimension = wire::text(p, "dimension");
        check(QRegularExpression("^[A-Za-z][A-Za-z0-9_]{0,63}$").match(out.name).hasMatch(),
              "InvalidInput", "Parameter names use 1–64 ASCII letters, digits or underscores");
        auto value = p.value("value_si");
        check(value.isDouble() || value.isInteger(), "InvalidInput", "Numeric SI value required");
        out.value = value.toDouble();
        toSI(out.value, out.dimension,
             out.dimension == "length"  ? "m"
             : out.dimension == "angle" ? "rad"
             : out.dimension == "mass"  ? "kg"
                                        : "1");
        if (out.name == "MaterialDensityKgM3")
            check(out.dimension == "scalar" && out.value > 0 && out.value <= 1e6, "InvalidInput",
                  "Material density must be positive kg/m³, at most 1,000,000");
        return out;
    }
    QCborMap map() const {
        QCborMap p;
        p.insert(QStringLiteral("name"), name);
        p.insert(QStringLiteral("dimension"), dimension);
        p.insert(QStringLiteral("value_si"), value);
        return p;
    }
};
// Stable semantic intent excludes request/epoch/UI-session/generation transport identities.
inline QByteArray requestHash(const wire::Envelope &e) {
    QCborMap value;
    value.insert(QStringLiteral("schema"), 1);
    value.insert(QStringLiteral("project"), e.context.project);
    value.insert(QStringLiteral("document"), e.context.document);
    value.insert(QStringLiteral("base_revision"), e.revision);
    value.insert(QStringLiteral("command"), e.command);
    value.insert(QStringLiteral("payload"), e.payload);
    return QCryptographicHash::hash(QCborValue(value).toCbor(QCborValue::SortKeysInMaps),
                                    QCryptographicHash::Sha256);
}
} // namespace cad::domain

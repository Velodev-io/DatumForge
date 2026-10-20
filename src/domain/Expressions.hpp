#pragma once
#include "Documents.hpp"
#include <array>
namespace cad::expression {
using Dimension = std::array<int, 3>; // length, angle, mass exponents
inline constexpr Dimension scalar{0, 0, 0}, length{1, 0, 0}, angle{0, 1, 0}, mass{0, 0, 1};
struct Quantity {
    double value = 0;
    Dimension dimension = scalar;
};
class Environment {
    QMap<QString, QString> definitions;
    QMap<QString, Quantity> cache;
    QSet<QString> visiting;

  public:
    explicit Environment(QMap<QString, QString> names = {});
    Quantity name(QString name);
    Quantity evaluate(QString text);
    void validateAll();
};
QString format(Quantity quantity, QString unit, int precision = 10);
Quantity unit(QString name);
} // namespace cad::expression

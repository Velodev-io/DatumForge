#pragma once
#include "model/Supports.hpp"
#include "support/check.hpp"
namespace support_fixture {
using namespace cad;
inline domain::GeometryReference ref(QString object, QString role, QString entity = {}) {
    domain::GeometryReference r;
    r.object = object;
    r.role = role;
    r.entity = entity;
    return r;
}
template <class F> void error(QString expected, F &&action) {
    QString code, message;
    try {
        action();
    } catch (const domain::Error &e) {
        code = e.code;
        message = QString::fromUtf8(e.what());
    }
    if (code != expected)
        throw std::runtime_error("Expected " + expected.toStdString() + ", got " +
                                 code.toStdString() + ": " + message.toStdString());
}
inline void vector(sketch::Vec3 actual, sketch::Vec3 expected, double tolerance = 1e-10) {
    for (int i = 0; i < 3; ++i)
        near(actual[i], expected[i], tolerance);
}
} // namespace support_fixture

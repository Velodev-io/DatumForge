#include "GeometryReference.hpp"
namespace cad::domain {
void GeometryReference::validate() const {
    id(object);
    if (document != "Self")
        id(document);
    check(
        QStringList{"Object", "Point", "Curve", "Plane", "Axis", "Face", "Edge", "Vertex"}.contains(
            role),
        "InvalidInput", "Typed geometry role required");
    bool topological = role == "Face" || role == "Edge" || role == "Vertex";
    if (topological) {
        check(entity.isEmpty() && QRegularExpression("^" + role.toLower() + "/[0-9a-f]{64}$")
                                      .match(topology)
                                      .hasMatch(),
              "InvalidInput", "A matching typed topology token is required");
    } else {
        check(topology.isEmpty(), "InvalidInput", "Topology token does not match geometry role");
        if (role == "Point")
            id(entity);
        else if (role == "Curve") {
            if (!entity.isEmpty())
                id(entity);
        } else
            check(entity.isEmpty(), "InvalidInput", "This role has no member entity");
    }
    bool curve = role == "Curve" || role == "Edge";
    check(endpoint.isEmpty() || (curve && (endpoint == "Start" || endpoint == "End") && !parameter),
          "InvalidInput", "Choose one curve endpoint or parameter");
    if (parameter)
        check(curve && std::isfinite(*parameter) && std::abs(*parameter) <= 1e6, "InvalidInput",
              "A finite bounded curve parameter is required");
}
QCborMap GeometryReference::map() const {
    validate();
    QCborMap m;
    m.insert(QStringLiteral("version"), 1);
    m.insert(QStringLiteral("document"), document);
    m.insert(QStringLiteral("object"), object);
    m.insert(QStringLiteral("role"), role);
    if (!entity.isEmpty())
        m.insert(QStringLiteral("entity"), entity);
    if (!topology.isEmpty())
        m.insert(QStringLiteral("topology"), topology);
    if (!endpoint.isEmpty())
        m.insert(QStringLiteral("endpoint"), endpoint);
    if (parameter)
        m.insert(QStringLiteral("parameter"), *parameter);
    return m;
}
GeometryReference GeometryReference::parse(const QCborMap &m) {
    QSet<QString> keys{"version", "document", "object", "role"};
    for (auto key : {"entity", "topology", "endpoint", "parameter"})
        if (m.contains(QString::fromLatin1(key)))
            keys.insert(QString::fromLatin1(key));
    wire::keys(m, keys);
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported geometry reference version");
    GeometryReference r;
    r.document = wire::text(m, "document");
    r.object = wire::text(m, "object");
    r.role = wire::text(m, "role");
    if (m.contains(QStringLiteral("entity")))
        r.entity = wire::text(m, "entity");
    if (m.contains(QStringLiteral("topology")))
        r.topology = wire::text(m, "topology");
    if (m.contains(QStringLiteral("endpoint")))
        r.endpoint = wire::text(m, "endpoint");
    if (m.contains(QStringLiteral("parameter"))) {
        auto value = m.value("parameter");
        check(value.isDouble() || value.isInteger(), "InvalidInput",
              "Numeric geometry reference parameter required");
        r.parameter = value.toDouble();
    }
    r.validate();
    return r;
}
} // namespace cad::domain

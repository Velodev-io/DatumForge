#include "Model.hpp"
namespace cad::drawing {
using domain::check;
using sketch::fields;
namespace {
void optionalId(QString value) {
    if (!value.isEmpty())
        domain::id(value);
}
QString boundedText(const QCborMap &m, QString key) {
    auto s = wire::text(m, key);
    check(s.size() <= 512 && !s.contains(QChar::Null), "InvalidInput",
          "Drawing text exceeds 512 characters");
    for (auto c : s)
        check(c.unicode() >= 32, "InvalidInput", "Drawing text must be a single printable line");
    return s;
}
} // namespace
QCborMap View::map() const {
    return fields({{"id", id},
                   {"source", source},
                   {"orientation", orientation},
                   {"parent", parent},
                   {"x_mm", x},
                   {"y_mm", y},
                   {"source_revision", sourceRevision},
                   {"hidden", hidden}});
}
View View::parse(QCborMap m) {
    wire::keys(
        m, {"id", "source", "orientation", "parent", "x_mm", "y_mm", "source_revision", "hidden"});
    View v;
    v.id = domain::id(wire::text(m, "id"));
    v.source = domain::id(wire::text(m, "source"));
    v.parent = wire::text(m, "parent");
    optionalId(v.parent);
    v.orientation = wire::text(m, "orientation");
    check(QStringList{"Front", "Top", "Right", "Isometric"}.contains(v.orientation),
          "UnsupportedOperation", "Unsupported drawing orientation");
    v.x = features::number(m, "x_mm", 0, 2000);
    v.y = features::number(m, "y_mm", 0, 2000);
    v.sourceRevision = wire::integer(m, "source_revision");
    check(v.sourceRevision >= 0 && m.value("hidden").isBool(), "InvalidInput",
          "Invalid drawing revision/hidden-line policy");
    v.hidden = m.value("hidden").toBool();
    return v;
}
QCborMap Annotation::map() const {
    return fields({{"id", id},
                   {"view", view},
                   {"kind", kind},
                   {"body", body},
                   {"token", token},
                   {"second_token", secondToken},
                   {"text", text},
                   {"override", overrideText},
                   {"x_mm", x},
                   {"y_mm", y}});
}
Annotation Annotation::parse(QCborMap m) {
    wire::keys(m, {"id", "view", "kind", "body", "token", "second_token", "text", "override",
                   "x_mm", "y_mm"});
    Annotation a;
    a.id = domain::id(wire::text(m, "id"));
    a.view = wire::text(m, "view");
    optionalId(a.view);
    a.kind = wire::text(m, "kind");
    check(QStringList{"Text", "Linear", "Angular", "Radius", "Diameter", "Center"}.contains(a.kind),
          "UnsupportedOperation", "Unsupported drawing annotation");
    a.body = wire::text(m, "body");
    optionalId(a.body);
    a.token = boundedText(m, "token");
    a.secondToken = boundedText(m, "second_token");
    a.text = boundedText(m, "text");
    a.overrideText = boundedText(m, "override");
    a.x = features::number(m, "x_mm", 0, 2000);
    a.y = features::number(m, "y_mm", 0, 2000);
    check(a.kind == "Text" || (!a.view.isEmpty() && !a.body.isEmpty() && !a.token.isEmpty()),
          "InvalidInput", "Select a view and exact edge anchor");
    check(a.kind != "Angular" || !a.secondToken.isEmpty(), "InvalidInput",
          "Angular annotation requires two straight edges");
    return a;
}
QCborMap Sheet::map() const {
    QCborArray v, a;
    for (auto &e : views)
        v.append(e.map());
    for (auto &e : annotations)
        a.append(e.map());
    return fields({{"id", id},
                   {"name", name},
                   {"units", units},
                   {"projection", projection},
                   {"bom_source", bomSource},
                   {"bom_revision", bomRevision},
                   {"width_mm", width},
                   {"height_mm", height},
                   {"scale", scale},
                   {"views", v},
                   {"annotations", a}});
}
Sheet Sheet::parse(QCborMap m) {
    wire::keys(m, {"id", "name", "units", "projection", "bom_source", "bom_revision", "width_mm",
                   "height_mm", "scale", "views", "annotations"});
    Sheet s;
    s.id = domain::id(wire::text(m, "id"));
    s.name = domain::label(wire::text(m, "name"));
    s.units = domain::units(wire::text(m, "units"));
    s.projection = wire::text(m, "projection");
    check(QStringList{"third-angle", "first-angle"}.contains(s.projection), "InvalidInput",
          "Choose first-angle or third-angle projection");
    s.bomRevision = wire::integer(m, "bom_revision");
    check(s.bomRevision >= 0, "InvalidInput", "Invalid BOM revision");
    s.bomSource = wire::text(m, "bom_source");
    optionalId(s.bomSource);
    s.width = features::number(m, "width_mm", 100, 2000);
    s.height = features::number(m, "height_mm", 100, 2000);
    s.scale = features::number(m, "scale", .001, 100);
    check(m.value("views").isArray() && m.value("annotations").isArray(), "InvalidInput",
          "Drawing arrays required");
    check(m.value("views").toArray().size() <= 8 && m.value("annotations").toArray().size() <= 30,
          "ResourceLimit", "Sheet view/annotation admission exceeded");
    for (auto e : m.value("views").toArray())
        s.views.push_back(View::parse(e.toMap()));
    for (auto e : m.value("annotations").toArray())
        s.annotations.push_back(Annotation::parse(e.toMap()));
    return s;
}
QCborMap Model::map() const {
    QCborArray s;
    for (auto &e : sheets)
        s.append(e.map());
    return fields({{"version", 1}, {"sheets", s}});
}
Model Model::parse(QCborMap m) {
    wire::keys(m, {"version", "sheets"});
    check(wire::integer(m, "version") == 1, "IncompatibleSchema",
          "Unsupported drawing definition version");
    check(m.value("sheets").isArray() && m.value("sheets").toArray().size() <= 8, "ResourceLimit",
          "At most eight drawing sheets");
    Model out;
    QSet<QString> ids;
    auto unique = [&](QString id) {
        check(!ids.contains(id), "InvalidInput", "Duplicate drawing identity");
        ids.insert(id);
    };
    for (auto e : m.value("sheets").toArray()) {
        auto s = Sheet::parse(e.toMap());
        unique(s.id);
        QMap<QString, View> seen;
        for (auto &v : s.views) {
            unique(v.id);
            check(v.x < s.width && v.y < s.height, "InvalidInput",
                  "View origin is outside the sheet");
            if (!v.parent.isEmpty()) {
                check(seen.contains(v.parent) && seen[v.parent].source == v.source &&
                          seen[v.parent].orientation == "Front" &&
                          QStringList{"Top", "Right"}.contains(v.orientation),
                      "InvalidInput",
                      "Projected views require an earlier Front base of the same source");
                auto base = seen[v.parent];
                double sign = s.projection == "third-angle" ? 1 : -1;
                check(v.orientation == "Top"
                          ? std::abs(v.x - base.x) < 1e-8 && (base.y - v.y) * sign > 0
                          : std::abs(v.y - base.y) < 1e-8 && (v.x - base.x) * sign > 0,
                      "InvalidInput", "Projected placement contradicts the projection convention");
            }
            seen[v.id] = v;
        }
        for (auto &a : s.annotations) {
            unique(a.id);
            check(a.x < s.width && a.y < s.height && (a.view.isEmpty() || seen.contains(a.view)),
                  "InvalidInput", "Annotation has an invalid sheet/view anchor");
        }
        out.sheets.push_back(s);
    }
    return out;
}
} // namespace cad::drawing

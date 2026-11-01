#pragma once
#include "domain/GeometryReference.hpp"
#include "evaluation/Graph.hpp"
#include "sketch/Sketch.hpp"
namespace cad::model {
// Transient engine contracts. CM4.c owns durable support collections/manifests.
struct Datum {
    QString id = newId(), kind = "DatumPlane", mode = "Offset";
    std::vector<domain::GeometryReference> references;
    QCborMap parameters;
    bool reversed = false;
    void validate() const;
    QStringList outputs() const;
    QCborMap map() const;
    static Datum parse(const QCborMap &map);
};
struct Attachment {
    QString sketch;
    domain::GeometryReference reference;
    double offset = 0, angle = 0;
    bool reversed = false;
    QCborMap map() const;
    static Attachment parse(const QCborMap &map);
};
struct Conversion {
    QString id = newId(), sketch;
    std::vector<domain::GeometryReference> sources;
    QMap<QString, QString>
        members; // Generated identities/roles, checked against recomputed output.
    double angle = 0, scale = 1;
    sketch::Vec2 center, translation;
    bool construction = false;
    QCborMap map() const;
    static Conversion parse(const QCborMap &map);
};
struct SupportDefinition {
    QString document = "Self";
    QCborArray sketches, features;
    std::vector<Datum> datums;
    std::vector<Attachment> attachments;
    std::vector<Conversion> conversions;
    QStringList history;
    evaluation::Graph graph() const;
    QCborMap map() const;
    static SupportDefinition parse(const QCborMap &map);
};
} // namespace cad::model

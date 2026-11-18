#pragma once
#include "Datums.hpp"
#include "exchange/Exchange.hpp"
namespace cad::geometry {
using SupportBuild = std::function<kernel::Part(const QCborArray &, const QCborArray &)>;
struct SupportResult {
    std::vector<sketch::Sketch> sketches;
    QMap<QString, sketch::SpatialSketch> spatialSketches;
    QMap<QString, sketch::Frame> planes;
    QMap<QString, SupportAxis> axes;
    QMap<QString, std::vector<SupportCurve>> derived;
    QCborArray conversions;
    QStringList order;
    kernel::Part part;
    QCborArray evolution;
    QCborArray modelingProvenance;
    QCborMap inspection;
    QCborArray datumGeometry() const;
    QCborMap map(qint64 revision = 0) const;
};
sketch::Frame attachmentFrame(const model::Attachment &attachment,
                              const SupportReferences &references);
SupportResult evaluateSupports(const model::SupportDefinition &definition,
                               const SupportBuild &build = {},
                               const exchange::Control &control = {}, QString inspectOwner = {},
                               int inspectSlot = -1, QString inspectPath = {});
} // namespace cad::geometry

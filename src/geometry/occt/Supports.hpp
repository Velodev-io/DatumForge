#pragma once
#include "Datums.hpp"
#include "exchange/Exchange.hpp"
namespace cad::geometry {
using SupportBuild = std::function<kernel::Part(const QCborArray &, const QCborArray &)>;
struct SupportResult {
    std::vector<sketch::Sketch> sketches;
    QMap<QString, sketch::Frame> planes;
    QMap<QString, SupportAxis> axes;
    QCborArray conversions;
    QStringList order;
    kernel::Part part;
    QCborMap map(qint64 revision = 0) const;
};
sketch::Frame attachmentFrame(const model::Attachment &attachment,
                              const SupportReferences &references);
SupportResult evaluateSupports(const model::SupportDefinition &definition,
                               const SupportBuild &build = {},
                               const exchange::Control &control = {});
} // namespace cad::geometry

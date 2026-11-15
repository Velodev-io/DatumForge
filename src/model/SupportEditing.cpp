#include "SupportEditing.hpp"
#include <algorithm>
namespace cad::model {
SupportDefinition editableSupports(const QCborMap &document) {
    if (document.contains(QStringLiteral("model_supports")))
        return SupportDefinition::parse(document.value("model_supports").toMap());
    SupportDefinition model;
    model.document = domain::id(document.value("id").toString());
    model.sketches = document.value("sketches").toArray();
    model.features = document.value("features").toArray();
    model.sequence();
    return model;
}
SupportDefinition editSupportedFeatures(const QCborMap &document, QCborArray features) {
    auto model = editableSupports(document);
    model.features = features;
    model.sequence();
    model.map();
    return model;
}
bool convertedSketch(const QCborMap &document, QString sketch) {
    if (!document.contains(QStringLiteral("model_supports")))
        return false;
    auto model = editableSupports(document);
    return std::any_of(model.conversions.begin(), model.conversions.end(),
                       [&](auto &c) { return c.sketch == sketch; });
}
SupportDefinition editSupportedSketches(const QCborMap &document, QCborArray sketches) {
    auto model = editableSupports(document);
    auto original = sketch::parse(model.sketches);
    auto accepted = sketch::parse(document.value("sketches").toArray());
    auto edited = sketch::parse(sketches);
    for (auto &s : edited) {
        auto old =
            std::find_if(accepted.begin(), accepted.end(), [&](auto &v) { return v.id == s.id; });
        auto base =
            std::find_if(original.begin(), original.end(), [&](auto &v) { return v.id == s.id; });
        if (old != accepted.end() && old->map() == s.map() && base != original.end()) {
            s = *base; // Retain original definitions of every untouched descendant.
            continue;
        }
        domain::check(
            !convertedSketch(document, s.id), "UnsupportedOperation",
            "Converted members are read-only; edit their source or conversion definition");
        if (base != original.end() &&
            std::any_of(model.attachments.begin(), model.attachments.end(),
                        [&](auto &a) { return a.sketch == s.id; }))
            s.frame = base->frame; // Attachment reevaluation owns the world frame.
    }
    model.sketches = sketch::encode(edited);
    model.sequence();
    model.map();
    return model;
}
} // namespace cad::model

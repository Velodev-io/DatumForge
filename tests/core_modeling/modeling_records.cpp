#include "ModelingFixture.hpp"
#include "features/ModelingRecords.hpp"
#include <QCoreApplication>
using namespace cad;
using namespace modeling_fixture;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        auto f = extrude(newId());
        auto r = support_fixture::ref(newId(), "Face");
        r.topology = "face/" + QString(64, 'a');
        f.insert(QStringLiteral("direction1"), fields({{"mode", "UpToSurface"},
                                                       {"reference", r.map()},
                                                       {"draft", blind().value("draft")}}));
        auto replacement = r;
        replacement.topology = "face/" + QString(64, 'b');
        auto fixed = features::replaceIntentReference(QCborArray{f}, f.value("id").toString(),
                                                      "direction1/reference", replacement);
        CHECK(features::intentReference(fixed[0].toMap(), "direction1/reference").map() ==
              replacement.map());
        CHECK(features::intentReference(f, "direction1/reference").map() == r.map());
        rejects([&] {
            features::replaceIntentReference(QCborArray{f}, newId(), "direction1/reference",
                                             replacement);
        });
        rejects([&] { features::intentReference(f, "profile/sketch"); });
        rejects([&] { features::intentReference(f, "direction2/reference"); });
        auto provenance = fields({{"feature", f.value("id")},
                                  {"kind", "Extrude"},
                                  {"operation", "NewBody"},
                                  {"target", QCborValue(nullptr)},
                                  {"outputs", QCborArray{f.value("id")}},
                                  {"history", QCborArray{}}});
        auto record = fields(
            {{"version", 1}, {"intents", QCborArray{f}}, {"provenance", QCborArray{provenance}}});
        features::validateModelingRecords(record);
        auto bad = record;
        bad.insert(QStringLiteral("version"), 2);
        rejects([&] { features::validateModelingRecords(bad); });
        bad = record;
        bad.insert(QStringLiteral("provenance"), QCborArray{});
        rejects([&] { features::validateModelingRecords(bad); });
        bad = record;
        bad.insert(QStringLiteral("intents"), QCborArray{f, f});
        rejects([&] { features::validateModelingRecords(bad); });
    });
}

#include "SupportFixture.hpp"
#include "geometry/occt/Supports.hpp"
#include "persistence/ProjectStore.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
using namespace cad;
using namespace support_fixture;
using sketch::fields;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module5-cm4-support-store-XXXXXX"),
            files(QDir::tempPath() + "/cad-module5-cm4-support-saved-XXXXXX");
        for (auto *r : {&root, &files}) {
            CHECK(r->isValid());
            QFile marker(r->filePath(".cad-test-root"));
            CHECK(marker.open(QIODevice::WriteOnly));
            marker.write("disposable\n");
        }
        storage::ProjectStore store(root.path());
        auto project = newId(), doc = newId();
        store.create(project);
        auto command = [&](QString name, QCborMap payload = {}) {
            wire::Envelope e;
            e.command = name;
            e.payload = payload;
            e.context.project = project;
            e.context.document = doc;
            e.revision = store.revision();
            return e;
        };
        store.execute(command("CreateDocument", fields({{"id", doc},
                                                        {"properties_entity", newId()},
                                                        {"name", "Persistent supports"},
                                                        {"type", "Part"},
                                                        {"units", "mm"}})));
        sketch::Sketch base, target;
        base.rectangle({0, 0}, {.04, .03});
        target.rectangle({0, 0}, {.01, .01});
        features::Feature first, second;
        first.parameters = fields({{"sketch", base.id},
                                   {"operation", "NewBody"},
                                   {"target", ""},
                                   {"depth", .01},
                                   {"extent", "Forward"}});
        second.parameters = first.parameters;
        second.parameters.insert(QStringLiteral("sketch"), target.id);
        second.parameters.insert(QStringLiteral("depth"), .005);
        auto part = kernel::Part::build(sketch::encode({base}), features::encode({first}));
        auto source = ref(first.id, "Face");
        for (TopExp_Explorer it(part.stages.value(first.id), TopAbs_FACE); it.More(); it.Next()) {
            BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
            if (surface.GetType() == GeomAbs_Plane &&
                std::abs(surface.Plane().Location().Z() - 10) < 1e-8)
                source.topology = geometry::supportToken(it.Current());
        }
        CHECK(!source.topology.isEmpty());
        model::SupportDefinition model;
        model.document = doc;
        model.sketches = sketch::encode({base, target});
        model.features = features::encode({first, second});
        model::Datum plane;
        plane.references = {source};
        plane.parameters = fields({{"distance", .03}, {"count", 1}});
        model.datums = {plane};
        model::Attachment attachment;
        attachment.sketch = target.id;
        attachment.reference = ref(plane.id, "Plane");
        model.attachments = {attachment};
        model.history = {base.id, first.id, plane.id, target.id, second.id};
        auto commit = command("SetModelSupports", fields({{"model", model.map()}}));
        store.execute(commit);
        auto revision = store.revision();
        CHECK(store.featureState(doc).value("supports").toMap() == model.map());
        CHECK(!store.featureState(doc).value("support_evolution").toArray().empty());
        near(sketch::parse(store.sketches(doc))[1].frame.origin[2], .04);
        near(store.evaluatePart(doc).measure(0).value("volume_m3").toDouble(), .0000125, 1e-12);
        store.execute(commit); // Idempotent replay.
        CHECK(store.revision() == revision);
        auto stale = command("SetModelSupports", fields({{"model", model.map()}}));
        --stale.revision;
        error("StaleRevision", [&] { store.execute(stale); });
        store.execute(command("Undo"));
        CHECK(store.sketches(doc).empty() && store.features(doc).empty());
        store.execute(command("Redo"));
        CHECK(store.featureState(doc).value("supports").toMap() == model.map());
        auto shifted = model;
        shifted.datums[0].parameters.insert(QStringLiteral("distance"), .06);
        store.execute(command("SetModelSupports", fields({{"model", shifted.map()}})));
        near(sketch::parse(store.sketches(doc))[1].frame.origin[2], .07);
        auto path = files.filePath("supports.cadproj");
        store.save(command("SaveProject"), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.featureState(doc).value("supports").toMap() == shifted.map());
        near(sketch::parse(store.sketches(doc))[1].frame.origin[2], .07);
        near(store.evaluatePart(doc).measure(0).value("volume_m3").toDouble(), .0000125, 1e-12);
        revision = store.revision();
        auto broken = shifted;
        broken.datums[0].references[0].topology = "face/" + QString(64, 'a');
        error("ReferenceMissing", [&] {
            store.execute(command("SetModelSupports", fields({{"model", broken.map()}})));
        });
        CHECK(store.revision() == revision);
        auto inspection = store.supportReferenceCandidates(doc, broken.map(), plane.id, 0);
        CHECK(inspection.value("state") == "Missing");
        CHECK(inspection.value("replacements").toArray().contains(source.map()));
        auto repairPayload = fields({{"model", broken.map()},
                                     {"owner", plane.id},
                                     {"slot", 0},
                                     {"replacement", source.map()},
                                     {"fingerprint", inspection.value("fingerprint")}});
        auto forged = repairPayload;
        forged.insert(QStringLiteral("replacement"), broken.datums[0].references[0].map());
        error("InvalidInput", [&] { store.execute(command("ResolveSupportReference", forged)); });
        forged = repairPayload;
        forged.insert(QStringLiteral("fingerprint"), QByteArray("obsolete"));
        error("StaleRevision", [&] { store.execute(command("ResolveSupportReference", forged)); });
        CHECK(store.revision() == revision);
        store.execute(command("ResolveSupportReference", repairPayload));
        CHECK(store.featureState(doc).value("supports").toMap() == shifted.map());
        revision = store.revision();
        error("UnsupportedOperation", [&] {
            store.execute(command("SetSketches", fields({{"sketches", model.sketches}})));
        });
        error("UnsupportedOperation", [&] {
            store.execute(command("SetFeatures", fields({{"features", model.features}})));
        });
        CHECK(store.revision() == revision);
        exchange::Control canceled;
        canceled.canceled = [] { return true; };
        error("Canceled", [&] {
            store.execute(command("SetModelSupports", fields({{"model", model.map()}})), canceled);
        });
        CHECK(store.revision() == revision);
        store.close();
        store.recover(project);
        CHECK(store.featureState(doc).value("supports").toMap() == shifted.map());
        sketch::Sketch cutter;
        cutter.rectangle({.005, .005}, {.01, .01});
        features::Feature cut;
        cut.parameters = fields({{"sketch", cutter.id},
                                 {"operation", "Cut"},
                                 {"target", first.id},
                                 {"depth", .01},
                                 {"extent", "Forward"}});
        auto evolved = shifted;
        evolved.sketches.append(cutter.map());
        evolved.features = features::encode({first, cut, second});
        evolved.datums[0].references[0].object = cut.id;
        evolved.history = {base.id, first.id, cutter.id, cut.id, plane.id, target.id, second.id};
        store.execute(command("SetModelSupports", fields({{"model", evolved.map()}})));
        auto evolution = store.featureState(doc).value("support_evolution").toArray();
        CHECK(evolution.size() == 1 && evolution[0].toMap().value("state") == "History");
        CHECK(evolution[0].toMap().value("resolved") != source.topology);
        near(store.evaluatePart(doc).measure(0).value("volume_m3").toDouble(), .00001225, 1e-12);
        store.save(command("SaveProject"), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.featureState(doc).value("support_evolution").toArray() == evolution);
        near(sketch::parse(store.sketches(doc))[1].frame.origin[2], .07);
        auto split = evolved;
        sketch::Sketch slit;
        slit.id = cutter.id;
        slit.frame.origin[2] = .005;
        slit.rectangle({.019, -.001}, {.021, .031});
        split.sketches[2] = slit.map();
        auto groove = cut;
        groove.parameters.insert(QStringLiteral("depth"), .005);
        split.features = features::encode({first, groove, second});
        revision = store.revision();
        error("ReferenceAmbiguous", [&] {
            store.execute(command("SetModelSupports", fields({{"model", split.map()}})));
        });
        CHECK(store.revision() == revision);
        auto splitInspection = store.supportReferenceCandidates(doc, split.map(), plane.id, 0);
        CHECK(splitInspection.value("state") == "Split");
        auto choices = splitInspection.value("replacements").toArray();
        auto splitTokens = splitInspection.value("candidates").toArray();
        CHECK(splitTokens.size() == 2);
        QCborMap chosen;
        for (auto value : choices)
            if (value.toMap().value("topology") == splitTokens[0])
                chosen = value.toMap();
        CHECK(!chosen.empty());
        store.execute(command("ResolveSupportReference",
                              fields({{"model", split.map()},
                                      {"owner", plane.id},
                                      {"slot", 0},
                                      {"replacement", chosen},
                                      {"fingerprint", splitInspection.value("fingerprint")}})));
        near(store.evaluatePart(doc).measure(0).value("volume_m3").toDouble(), .0000122, 1e-12);
        store.execute(command("Undo"));
        CHECK(store.featureState(doc).value("supports").toMap() == evolved.map());
        store.execute(command("Redo"));
        CHECK(store.featureState(doc).value("supports").toMap() != evolved.map());
        auto complete =
            model::SupportDefinition::parse(store.featureState(doc).value("supports").toMap());
        sketch::Sketch projected;
        complete.sketches.append(projected.map());
        model::Conversion conversion;
        conversion.sketch = projected.id;
        conversion.sources = {ref(base.id, "Curve", base.entities.back().id)};
        conversion.scale = .75;
        complete.conversions = {conversion};
        complete.history.append(conversion.id);
        complete.history.append(projected.id);
        model::Datum axis;
        axis.kind = "DatumAxis";
        axis.mode = "Principal";
        axis.parameters = fields({{"axis", "Z"}});
        complete.datums.push_back(axis);
        complete.history.append(axis.id);
        model::Datum offsets;
        offsets.references = {ref(base.id, "Plane")};
        offsets.parameters = fields({{"distance", .001}, {"count", 3}});
        complete.datums.push_back(offsets);
        complete.history.append(offsets.outputs());
        store.execute(command("SetModelSupports", fields({{"model", complete.map()}})));
        auto acceptedSketches = store.sketches(doc);
        auto generated = sketch::parse(acceptedSketches).back();
        CHECK(generated.entities.size() == 3 && generated.constraints.size() == 2);
        auto line = generated.entities.back();
        near(sketch::length(generated.entity(line.a).point, generated.entity(line.b).point),
             .03 * .75);
        auto attachedConversion = complete;
        model::Attachment generatedSupport;
        generatedSupport.sketch = projected.id;
        generatedSupport.reference = source;
        generatedSupport.reference.topology = "face/" + QString(64, 'a');
        attachedConversion.attachments.push_back(generatedSupport);
        auto attachmentInspection =
            store.supportReferenceCandidates(doc, attachedConversion.map(), projected.id, 0);
        CHECK(attachmentInspection.value("state") == "Missing");
        CHECK(attachmentInspection.value("replacements").toArray().contains(source.map()));
        store.save(command("SaveProject"), path);
        store.close();
        store.open(path, "Saved");
        CHECK(store.sketches(doc) == acceptedSketches);
        CHECK(store.featureState(doc).value("supports").toMap() == complete.map());
        auto copy = files.filePath("copy.cadproj"), copyId = newId();
        store.save(command("SaveAs", fields({{"new_project_id", copyId}})), copy, true);
        project = copyId;
        store.close();
        store.open(copy, "Saved");
        CHECK(store.project() == copyId);
        CHECK(store.sketches(doc) == acceptedSketches);
        CHECK(store.closure().value("documents").toArray().size() == 1);
        // Reject a changed base frame/definition, forged evolution, or downgraded
        // reader requirement without disturbing the already accepted project.
        auto tampered = files.filePath("tampered.cadproj");
        CHECK(QFile::copy(copy, tampered));
        {
            storage::Database database(tampered);
            auto entity = database.scalar("SELECT entity_id FROM features");
            auto bytes = database.scalar("SELECT definition FROM features").toByteArray();
            auto collection = wire::decode(bytes);
            auto forgedModel = complete;
            forgedModel.datums[0].parameters.insert(QStringLiteral("distance"), .09);
            collection.insert(QStringLiteral("supports"), forgedModel.map());
            database.run("UPDATE features SET definition=? WHERE entity_id=?",
                         {QCborValue(collection).toCbor(), entity});
        }
        error("InvalidProject", [&] { store.inspect(tampered); });
        auto downgraded = files.filePath("downgraded.cadproj");
        CHECK(QFile::copy(copy, downgraded));
        {
            storage::Database database(downgraded);
            auto manifest = database.scalar("SELECT manifest FROM project_meta").toString();
            CHECK(manifest.endsWith(";model-supports=1"));
            manifest.chop(QString(";model-supports=1").size());
            database.run("UPDATE project_meta SET manifest=?", {manifest});
        }
        error("IncompatibleSchema", [&] { store.inspect(downgraded); });
        CHECK(store.sketches(doc) == acceptedSketches);
    });
}

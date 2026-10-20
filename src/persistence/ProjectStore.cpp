#include "ProjectStore.hpp"
#include "geometry/occt/Part.hpp"
#include "evaluation/Plan.hpp"
#include "sketch/Solver.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryFile>
#include <exception>
namespace cad::storage {
using domain::check;
namespace {
constexpr int application = 1128350800;
const QString sketchManifest = "cad-semantic-v1;SI;objects=opaque-v1;strict-schema;sketch-v1;"
                               "PlaneGCS=256fc7eff3379911ab5daf88e10182c509aa8052;adapter=1";
const QString featureManifest = sketchManifest + ";features-v1;OCCT=7.9.2;part-adapter=1";
const QString historyManifest = featureManifest+";history-v2;fingerprint=1";
const QString manifest = "cad-semantic-v1;SI;objects=opaque-v1;strict-schema;no-feature-engine";
QByteArray encode(const QCborMap &map) {
    auto bytes = QCborValue(map).toCbor(QCborValue::SortKeysInMaps);
    check(bytes.size() <= int(limits::control), "ResourceLimit",
          "Semantic delta exceeds control limit");
    return bytes;
}
QCborMap decode(const QVariant &bytes) {
    return wire::decode(bytes.toByteArray());
}
QCborMap values(std::initializer_list<std::pair<QString, QCborValue>> fields) {
    QCborMap m;
    for (const auto &f : fields)
        m.insert(f.first, f.second);
    return m;
}
QString pointer(QString root, QString project) {
    return root + "/index/" + domain::id(project);
}
QString workspaceFor(QString root, QString project) {
    QFile file(pointer(root, project));
    check(file.open(QIODevice::ReadOnly) && file.size() == 37, "RecoveryMissing",
          "No verified recovery workspace is indexed");
    auto id = QString::fromUtf8(file.readAll()).trimmed();
    return root + "/workspaces/" + domain::id(id);
}
void initializeSchema(Database &db, QString id) {
    db.exec("PRAGMA application_id=1128350800; PRAGMA user_version=1");
    db.exec(R"SQL(
CREATE TABLE project_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),id TEXT NOT NULL UNIQUE,revision INTEGER NOT NULL CHECK(revision>=0),saved_revision INTEGER NOT NULL CHECK(saved_revision>=-1),save_generation INTEGER NOT NULL CHECK(save_generation>=0),history_cursor INTEGER NOT NULL CHECK(history_cursor>=0),manifest TEXT NOT NULL) STRICT;
CREATE TABLE documents(id TEXT PRIMARY KEY,type TEXT NOT NULL CHECK(type IN ('Part','Assembly','Drawing')),name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 128),units TEXT NOT NULL CHECK(units IN ('mm','cm','m','in')),revision INTEGER NOT NULL CHECK(revision>0)) STRICT;
CREATE TABLE entities(id TEXT PRIMARY KEY,document_id TEXT NOT NULL REFERENCES documents ON DELETE CASCADE,kind TEXT NOT NULL,version INTEGER NOT NULL CHECK(version=1)) STRICT;
CREATE UNIQUE INDEX entities_document ON entities(document_id,kind);
CREATE TABLE parameters(owner TEXT NOT NULL REFERENCES entities ON DELETE CASCADE,name TEXT NOT NULL,dimension TEXT NOT NULL CHECK(dimension IN ('length','angle','mass','scalar')),value_si REAL NOT NULL CHECK(abs(value_si)<=1e9),PRIMARY KEY(owner,name)) STRICT;
CREATE TABLE features(entity_id TEXT PRIMARY KEY REFERENCES entities,version INTEGER NOT NULL,ordinal INTEGER NOT NULL,definition BLOB NOT NULL) STRICT;
CREATE TABLE dependencies(owner TEXT NOT NULL REFERENCES entities,target TEXT NOT NULL REFERENCES entities,role TEXT NOT NULL,PRIMARY KEY(owner,target,role)) STRICT;
CREATE INDEX dependencies_target ON dependencies(target);
CREATE TABLE "references"(owner TEXT NOT NULL REFERENCES entities,target TEXT REFERENCES entities,role TEXT NOT NULL,token BLOB NOT NULL,state TEXT NOT NULL,PRIMARY KEY(owner,role)) STRICT;
CREATE TABLE sketch_entities(entity_id TEXT PRIMARY KEY REFERENCES entities,definition BLOB NOT NULL) STRICT;
CREATE TABLE constraints(entity_id TEXT PRIMARY KEY REFERENCES entities,definition BLOB NOT NULL) STRICT;
CREATE TABLE occurrences(entity_id TEXT PRIMARY KEY REFERENCES entities,definition_id TEXT NOT NULL REFERENCES documents,transform BLOB NOT NULL) STRICT;
CREATE TABLE mates(entity_id TEXT PRIMARY KEY REFERENCES entities,definition BLOB NOT NULL) STRICT;
CREATE TABLE drawing_views(entity_id TEXT PRIMARY KEY REFERENCES entities,source_id TEXT NOT NULL REFERENCES documents,source_revision INTEGER NOT NULL,definition BLOB NOT NULL) STRICT;
CREATE TABLE annotations(entity_id TEXT PRIMARY KEY REFERENCES entities,definition BLOB NOT NULL) STRICT;
CREATE TABLE objects(id TEXT NOT NULL UNIQUE CHECK(length(id)=64),kind TEXT NOT NULL CHECK(kind='attachment'),encoding TEXT NOT NULL CHECK(encoding='opaque-v1'),classification TEXT NOT NULL CHECK(classification IN ('authoritative','cache')),size INTEGER NOT NULL CHECK(size BETWEEN 1 AND 134217728),payload BLOB NOT NULL CHECK(length(payload)=size)) STRICT;
CREATE TABLE entity_objects(owner TEXT NOT NULL REFERENCES entities ON DELETE CASCADE,object_id TEXT NOT NULL REFERENCES objects(id),PRIMARY KEY(owner,object_id)) STRICT;
CREATE TABLE object_refs(root_type TEXT NOT NULL CHECK(root_type IN ('document','saved','history','job','export','recovery')),root_id TEXT NOT NULL,object_id TEXT NOT NULL REFERENCES objects(id),PRIMARY KEY(root_type,root_id,object_id)) STRICT;
CREATE INDEX object_refs_object ON object_refs(object_id);
CREATE TABLE commands(operation_id TEXT PRIMARY KEY,request_hash BLOB NOT NULL CHECK(length(request_hash)=32),document_id TEXT NOT NULL,base_revision INTEGER NOT NULL,new_revision INTEGER NOT NULL,category TEXT NOT NULL CHECK(category IN ('edit','save')),intent BLOB NOT NULL,before_delta BLOB NOT NULL,after_delta BLOB NOT NULL,outcome BLOB NOT NULL) STRICT;
CREATE UNIQUE INDEX commands_edit_revision ON commands(new_revision) WHERE category='edit';
CREATE TABLE history(position INTEGER PRIMARY KEY,operation_id TEXT NOT NULL UNIQUE REFERENCES commands(operation_id),bytes INTEGER NOT NULL CHECK(bytes>=0)) STRICT;
CREATE TABLE evaluation_results(feature_id TEXT NOT NULL REFERENCES entities,revision INTEGER NOT NULL,fingerprint BLOB NOT NULL,object_id TEXT REFERENCES objects(id),validity TEXT NOT NULL,PRIMARY KEY(feature_id,revision)) STRICT;
CREATE TABLE migrations(version INTEGER PRIMARY KEY,identity TEXT NOT NULL,checksum TEXT NOT NULL,applied_at TEXT NOT NULL) STRICT;
CREATE TABLE local_meta(singleton INTEGER PRIMARY KEY CHECK(singleton=1),saved_path TEXT NOT NULL) STRICT;
CREATE TABLE staging_files(path TEXT PRIMARY KEY,identity TEXT NOT NULL,destination TEXT NOT NULL,owner_operation TEXT NOT NULL) STRICT;
CREATE TABLE save_intents(operation_id TEXT PRIMARY KEY,request_hash BLOB NOT NULL,path TEXT NOT NULL,new_project_id TEXT NOT NULL,revision INTEGER NOT NULL,completed INTEGER NOT NULL CHECK(completed IN (0,1))) STRICT;
)SQL");
    db.run("INSERT INTO project_meta VALUES(1,?,0,-1,0,0,?)", {id, manifest});
    db.run("INSERT INTO local_meta VALUES(1,'')");
    db.run("INSERT INTO migrations VALUES(1,'initial-semantic-v1',?,?)",
           {QString::fromLatin1(
                QCryptographicHash::hash(manifest.toUtf8(), QCryptographicHash::Sha256).toHex()),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
}
void validateDelta(const QCborMap &d) {
    if (d.empty())
        return;
    auto type = wire::text(d, "type");
    check(QStringList{"restore_document", "delete_document", "rename", "units", "set_parameter",
                      "remove_parameter", "attach_object", "detach_object", "set_sketches",
                      "set_features", "set_feature_state"}
              .contains(type),
          "InvalidProject", "Unsupported semantic delta in project history");
    if (type == "restore_document") {
        wire::keys(d, {"type", "id", "kind", "name", "units", "entity"});
        domain::id(wire::text(d, "id"));
        domain::id(wire::text(d, "entity"));
        domain::kind(wire::text(d, "kind"));
        domain::label(wire::text(d, "name"));
        domain::units(wire::text(d, "units"));
    } else if (type == "delete_document") {
        wire::keys(d, {"type", "id"});
        domain::id(wire::text(d, "id"));
    } else if (type == "rename" || type == "units") {
        wire::keys(d, {"type", "id", "value"});
        domain::id(wire::text(d, "id"));
        if (type == "rename")
            domain::label(wire::text(d, "value"));
        else
            domain::units(wire::text(d, "value"));
    } else {
        domain::id(wire::text(d, "owner"));
        domain::id(wire::text(d, "document"));
        if(type=="set_feature_state") {
            wire::keys(d,{"type","owner","document","collection"});auto c=d.value("collection").toMap();wire::keys(c,{"features","checkpoint_features","checkpoint_sketches","stale"});
            check(c.value("features").isArray()&&c.value("checkpoint_features").isArray()&&c.value("checkpoint_sketches").isArray()&&c.value("stale").isBool(),"InvalidProject","Invalid history collection");features::parse(c.value("features").toArray());sketch::validateAccepted(c.value("checkpoint_sketches").toArray());kernel::Part::build(c.value("checkpoint_sketches").toArray(),c.value("checkpoint_features").toArray());
        } else if (type == "set_features") {
            wire::keys(d, {"type", "owner", "document", "features"});
            check(d.value("features").isArray(), "InvalidInput", "Feature array required");
            features::parse(d.value("features").toArray());
        } else if (type == "set_sketches") {
            wire::keys(d, {"type", "owner", "document", "sketches"});
            check(d.value("sketches").isArray(), "InvalidInput", "Sketch array required");
            sketch::validateAccepted(d.value("sketches").toArray());
        } else if (type == "set_parameter") {
            wire::keys(d, {"type", "owner", "document", "name", "dimension", "value_si"});
            domain::Parameter::parse(values({{"name", d.value("name")},
                                             {"dimension", d.value("dimension")},
                                             {"value_si", d.value("value_si")}}));
        } else if (type == "remove_parameter") {
            wire::keys(d, {"type", "owner", "document", "name"});
            check(QRegularExpression("^[A-Za-z][A-Za-z0-9_]{0,63}$")
                      .match(wire::text(d, "name"))
                      .hasMatch(),
                  "InvalidProject", "Invalid parameter history name");
        } else if (type == "attach_object" || type == "detach_object") {
            wire::keys(d, {"type", "owner", "document", "hash"});
            check(QRegularExpression("^[0-9a-f]{64}$").match(wire::text(d, "hash")).hasMatch(),
                  "InvalidProject", "Invalid history object identity");
        } else
            throw domain::Error("InvalidProject", "Unsupported semantic delta in project history");
    }
}
QByteArray schemaFingerprint(Database &db) {
    QCborArray entries;
    for (const auto &row : db.rows("SELECT type,name,tbl_name,coalesce(sql,'') FROM sqlite_schema "
                                   "WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name")) {
        QCborArray item;
        for (const auto &value : row)
            item.append(value.toString());
        entries.append(item);
    }
    return QCryptographicHash::hash(QCborValue(entries).toCbor(), QCryptographicHash::Sha256);
}
QByteArray expectedSchema() {
    static const QByteArray expected = [] {
        Database scratch(":memory:");
        initializeSchema(scratch, "00000000-0000-4000-8000-000000000001");
        return schemaFingerprint(scratch);
    }();
    return expected;
}
void compatible(Database &db) {
    check(db.scalar("PRAGMA application_id").toInt() == application, "InvalidProject",
          "This is not a supported CAD project");
    check(db.scalar("PRAGMA user_version").toInt() == 1, "IncompatibleSchema",
          "Project schema is unsupported; original file was not changed");
    check(QStringList{manifest, sketchManifest, featureManifest, historyManifest}.contains(
              db.scalar("SELECT manifest FROM project_meta").toString()),
          "IncompatibleSchema",
          "Unsupported semantic/object encoding; original file was not changed");
}
QString owner(Database &db, QString document) {
    return db
        .scalar("SELECT id FROM entities WHERE document_id=? AND kind='DocumentProperties'",
                {document})
        .toString();
}
QCborArray gather(Database &db,QString document,QCborArray definitions,QString changed={},QCborArray replacement={}) {
    QSet<QString> wanted;for(auto &f:features::parse(definitions)){auto id=f.parameters.value("sketch").toString();if(!id.isEmpty())wanted.insert(id);}
    QCborArray out;bool seen=false;
    for(auto row:db.rows("SELECT e.document_id,s.definition FROM sketch_entities s JOIN entities e ON e.id=s.entity_id")) {
        auto doc=row[0].toString();auto a=doc==changed?replacement:decode(row[1]).value("sketches").toArray();seen=seen||doc==changed;
        for(auto value:a)if(doc==document||wanted.contains(value.toMap().value("id").toString()))out.append(value);
    }
    if(!changed.isEmpty()&&!seen)for(auto value:replacement)if(changed==document||wanted.contains(value.toMap().value("id").toString()))out.append(value);
    sketch::parse(out);return out;
}
} // namespace
ProjectStore::ProjectStore(QString recoveryDirectory, StoreOptions settings)
    : root(recoveryRoot(recoveryDirectory)), options(std::move(settings)) {
    check(options.historyEntries > 0 && options.historyEntries <= 1024 &&
              options.historyBytes > 0 && options.historyBytes <= qint64(limits::history_disk),
          "InvalidInput", "Invalid history retention budget");
    check(QDir().mkpath(root + "/index") && QDir().mkpath(root + "/workspaces"), "PermissionDenied",
          "Cannot create private recovery directories");
}
ProjectStore::~ProjectStore() = default;
void ProjectStore::point(QString stage) const {
    if (options.boundary)
        options.boundary(stage);
}
void ProjectStore::createWorkspace() {
    check(QDir(root + "/workspaces").entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() < 128,
          "ResourceLimit",
          "Recovery workspace retention is full; preserve/export older recovery before creating "
          "more");
    spaceFor(root, 1 << 20);
    workspace = root + "/workspaces/" + newId();
    check(QDir().mkdir(workspace), "PermissionDenied", "Cannot create working workspace");
    check(QFile::setPermissions(workspace, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                               QFileDevice::ExeOwner),
          "PermissionDenied", "Cannot secure workspace");
    workingLock = std::make_unique<Lock>(root, "workspace:" + workspace);
}
void ProjectStore::activate() {
    QSaveFile index(pointer(root, projectId));
    check(index.open(QIODevice::WriteOnly), "PermissionDenied", "Cannot publish recovery index");
    auto data = QFileInfo(workspace).fileName().toUtf8() + '\n';
    check(index.write(data) == data.size() && index.commit(), "StorageError",
          "Cannot publish recovery index");
    flushFile(index.fileName());
    flushFile(root + "/index", true);
}
void ProjectStore::configureWorking() {
    db->working();
    check(options.maximumPages > 0 && options.maximumPages <= 524288, "InvalidInput",
          "Invalid database page budget");
    auto pageSize = db->scalar("PRAGMA page_size").toLongLong();
    check(pageSize >= 512 && pageSize <= 65536, "InvalidProject", "Invalid database page size");
    auto quota = QString("PRAGMA max_page_count=%1")
                     .arg(qMin<qint64>(options.maximumPages, qint64(limits::project) / pageSize))
                     .toUtf8();
    db->exec(quota.constData());
    QFile::setPermissions(workingPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}
void ProjectStore::close() {
    resultCache.trim();
    db.reset();
    destination.reset();
    workingLock.reset();
    projectLock.reset();
    workspace.clear();
    projectId.clear();
    activeDocument.clear();
    opened.clear();
    sourcePath.clear();
}
void ProjectStore::create(QString id) {
    check(!isOpen(), "InvalidInput", "Close the current project before creating another");
    domain::id(id);
    auto lock = std::make_unique<Lock>(root, "project:" + id);
    check(!QFileInfo::exists(pointer(root, id)), "InvalidInput",
          "Project identity already exists; choose recovery instead");
    projectLock = std::move(lock);
    projectId = id;
    try {
        createWorkspace();
        db = std::make_unique<Database>(workingPath());
        configureWorking();
        Transaction tx(*db);
        initializeSchema(*db, id);
        tx.commit();
        validate(*db);
        activate();
    } catch (...) {
        close();
        throw;
    }
}
qint64 ProjectStore::revision() const {
    return db ? db->scalar("SELECT revision FROM project_meta").toLongLong() : 0;
}
void ProjectStore::validate(Database &candidate, bool saved) const {
    compatible(candidate);
    check(candidate.scalar("PRAGMA page_count").toLongLong() *
                  candidate.scalar("PRAGMA page_size").toLongLong() <=
              qint64(limits::project),
          "ResourceLimit", "Project database exceeds the 2 GiB admission limit");
    check(schemaFingerprint(candidate) == expectedSchema(), "IncompatibleSchema",
          "Project schema differs from the compiled format; triggers/views and altered constraints "
          "are refused");
    check(candidate.scalar("PRAGMA integrity_check").toString() == "ok" &&
              candidate.rows("PRAGMA foreign_key_check").empty(),
          "InvalidProject", "Database integrity or referenced object validation failed");
    auto meta = candidate.rows(
        "SELECT id,revision,saved_revision,save_generation,history_cursor FROM project_meta");
    check(meta.size() == 1, "InvalidProject", "Project metadata must be a singleton");
    domain::id(meta[0][0].toString());
    check(meta[0][1].toLongLong() >= 0 && meta[0][2].toLongLong() <= meta[0][1].toLongLong(),
          "InvalidProject", "Invalid saved/current revisions");
    if (saved)
        check(candidate.scalar("PRAGMA journal_mode").toString() == "delete" &&
                  meta[0][2] == meta[0][1],
              "InvalidProject", "Saved project must be a standalone explicitly saved revision");
    auto docs = candidate.rows("SELECT id,type,name,units,revision FROM documents");
    check(docs.size() <= 128, "ResourceLimit", "Too many document records");
    for (const auto &d : docs) {
        domain::id(d[0].toString());
        domain::kind(d[1].toString());
        domain::label(d[2].toString());
        domain::units(d[3].toString());
        check(d[4].toLongLong() > 0 && d[4].toLongLong() <= meta[0][1].toLongLong(),
              "InvalidProject", "Invalid document revision");
    }
    for (const auto &e : candidate.rows("SELECT id,kind,version FROM entities")) {
        domain::id(e[0].toString());
        check(e[1].toString() == "DocumentProperties" && e[2].toInt() == 1, "UnsupportedOperation",
              "Unsupported authoritative entity definition");
    }
    check(candidate.scalar("SELECT count(*) FROM entities").toInt() == docs.size(),
          "InvalidProject", "Each document requires one stable properties entity");
    auto params = candidate.rows("SELECT name,dimension,value_si FROM parameters");
    check(params.size() <= 1024, "ResourceLimit", "Too many typed parameters");
    for (const auto &p : params)
        domain::Parameter::parse(values({{"name", p[0].toString()},
                                         {"dimension", p[1].toString()},
                                         {"value_si", p[2].toDouble()}}));
    for (auto table : {"dependencies", "\"references\"", "constraints", "occurrences", "mates",
                       "drawing_views", "annotations", "evaluation_results"}) {
        auto sql = QString("SELECT count(*) FROM %1").arg(table).toUtf8();
        check(candidate.scalar(sql.constData()).toLongLong() == 0, "UnsupportedOperation",
              "This module cannot validate future geometry/solver definitions");
    }
    QSet<QString> sketchIds;
    for (auto row : candidate.rows("SELECT id FROM documents UNION SELECT id FROM entities"))
        sketchIds.insert(row[0].toString());
    for (auto row :
         candidate.rows("SELECT s.entity_id,s.definition,d.type FROM sketch_entities s JOIN "
                        "entities e ON e.id=s.entity_id JOIN documents d ON d.id=e.document_id")) {
        check(row[2].toString() == "Part", "InvalidProject", "Sketches require a Part document");
        auto collection = decode(row[1]);
        wire::keys(collection, {"sketches"});
        check(collection.value("sketches").isArray(), "InvalidProject",
              "Invalid sketch collection");
        sketch::validateAccepted(collection.value("sketches").toArray());
        for (auto &sketch : sketch::parse(collection.value("sketches").toArray())) {
            QStringList ids{sketch.id};
            for (auto &e : sketch.entities)
                ids.append(e.id);
            for (auto &c : sketch.constraints)
                ids.append(c.id);
            for (auto id : ids) {
                check(!sketchIds.contains(id), "InvalidProject",
                      "Project sketch identity collision");
                sketchIds.insert(id);
            }
        }
    }
    for (auto row :
         candidate.rows("SELECT f.definition,d.id,d.type,f.version,f.ordinal FROM features f JOIN "
                        "entities e ON e.id=f.entity_id JOIN documents d ON d.id=e.document_id")) {
        check(row[2].toString() == "Part" && row[3].toInt() == 1 && row[4].toInt() == 0 &&
                  QStringList{featureManifest,historyManifest}.contains(candidate.scalar("SELECT manifest FROM project_meta").toString()),
              "InvalidProject", "Invalid feature collection context or manifest");
        auto collection = decode(row[0]);
        bool history=collection.contains(QStringLiteral("checkpoint_features"));
        wire::keys(collection,history?QSet<QString>{"features","checkpoint_features","checkpoint_sketches","stale"}:QSet<QString>{"features"});
        if(history){check(candidate.scalar("SELECT manifest FROM project_meta").toString()==historyManifest,"IncompatibleSchema","History manifest required");check(collection.value("stale").isBool(),"InvalidProject","Explicit result state required");sketch::validateAccepted(collection.value("checkpoint_sketches").toArray());kernel::Part::build(collection.value("checkpoint_sketches").toArray(),collection.value("checkpoint_features").toArray());}
        check(collection.value("features").isArray(), "InvalidProject",
              "Feature collection required");
        auto a = collection.value("features").toArray();
        for (auto &f : features::parse(a)) {
            check(!sketchIds.contains(f.id), "InvalidProject", "Feature identity collision");
            sketchIds.insert(f.id);
        }
        auto evaluated=kernel::Part::build(gather(candidate,row[1].toString(),a),a,&resultCache,history);
        if(history)check(evaluated.stale==collection.value("stale").toBool(),"InvalidProject","Stored stale status disagrees with semantic rebuild");
    }
    candidate.forEachRow("SELECT operation_id,request_hash,base_revision,new_revision,"
                         "intent,before_delta,after_delta,outcome FROM commands",
                         [&](const QVariantList &c) {
                             domain::id(c[0].toString());
                             check(c[1].toByteArray().size() == 32 && c[2].toLongLong() >= 0 &&
                                       c[3].toLongLong() <= meta[0][1].toLongLong(),
                                   "InvalidProject", "Invalid operation ledger");
                             for (int i = 4; i < 8; ++i) {
                                 auto value = decode(c[i]);
                                 if (i == 5 || i == 6)
                                     validateDelta(value);
                             }
                         });
    check(candidate
                  .scalar("SELECT count(*) FROM objects WHERE length(payload)!=size OR "
                          "size>134217728 OR size<1 OR encoding!='opaque-v1' OR kind!='attachment' "
                          "OR classification NOT IN ('authoritative','cache')")
                  .toInt() == 0,
          "InvalidProject", "Invalid object payload/encoding");
    check(
        candidate
            .rows(
                "SELECT owner FROM entity_objects WHERE object_id NOT IN (SELECT id FROM objects)")
            .empty(),
        "ReferenceMissing", "Required object is missing");
    check(candidate
                  .scalar("SELECT count(*) FROM documents d WHERE (SELECT count(*) FROM entities e "
                          "WHERE e.document_id=d.id)!=1")
                  .toInt() == 0,
          "InvalidProject", "Each document must have exactly one properties entity");
    check(candidate
                  .scalar("SELECT count(*) FROM entity_objects e WHERE NOT EXISTS(SELECT 1 FROM "
                          "object_refs r WHERE r.root_type='document' AND r.root_id=e.owner AND "
                          "r.object_id=e.object_id)")
                  .toInt() == 0,
          "InvalidProject", "Required retention root is missing");
    // Hash payloads incrementally; never materialize a large object in a result row.
    for (const auto &r : candidate.rows("SELECT rowid,id,size FROM objects")) {
        auto hashId = r[1].toString();
        check(QRegularExpression("^[0-9a-f]{64}$").match(hashId).hasMatch(), "InvalidProject",
              "Invalid content identifier");
        sqlite3_blob *raw = nullptr;
        sqlcheck(sqlite3_blob_open(candidate.handle, "main", "objects", "payload",
                                   r[0].toLongLong(), 0, &raw),
                 candidate.handle);
        std::unique_ptr<sqlite3_blob, decltype(&sqlite3_blob_close)> blob(raw, sqlite3_blob_close);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        QByteArray chunk(65536, 0);
        qint64 size = r[2].toLongLong();
        for (qint64 at = 0; at < size; at += chunk.size()) {
            int n = int(qMin<qint64>(chunk.size(), size - at));
            sqlcheck(sqlite3_blob_read(raw, chunk.data(), n, int(at)), candidate.handle);
            hash.addData(QByteArrayView(chunk.constData(), n));
        }
        check(hash.result().toHex() == hashId.toLatin1(), "InvalidProject",
              "Required object checksum failed");
    }
}
QCborMap ProjectStore::state() const {
    check(isOpen(), "InvalidInput", "No project is open");
    QCborArray docs;
    for (const auto &r :
         db->rows("SELECT id,type,name,units,revision FROM documents ORDER BY rowid")) {
        auto doc = r[0].toString();
        QCborArray params;
        auto entity = owner(*db, doc);
        for (const auto &p :
             db->rows("SELECT name,dimension,value_si FROM parameters WHERE owner=? ORDER BY name",
                      {entity}))
            params.append(values({{"name", p[0].toString()},
                                  {"dimension", p[1].toString()},
                                  {"value_si", p[2].toDouble()}}));
        docs.append(values({{"id", doc},
                            {"type", r[1].toString()},
                            {"name", r[2].toString()},
                            {"units", r[3].toString()},
                            {"revision", r[4].toLongLong()},
                            {"properties_entity", entity},
                            {"parameters", params},
                            {"features", features(doc)},
                            {"feature_status", featureState(doc).value("stale").toBool()?"Stale":"Current"},
                            {"sketches", sketches(doc)},
                            {"sketch_diagnostics", sketch::validateAccepted(sketches(doc))}}));
    }
    auto cursor = db->scalar("SELECT history_cursor FROM project_meta").toLongLong();
    return values(
        {{"project_id", projectId},
         {"revision", revision()},
         {"saved_revision", db->scalar("SELECT saved_revision FROM project_meta").toLongLong()},
         {"save_generation", db->scalar("SELECT save_generation FROM project_meta").toLongLong()},
         {"active_document", activeDocument},
         {"documents", docs},
         {"open_documents", QCborArray::fromStringList(opened)},
         {"undo_available",
          db->scalar("SELECT count(*) FROM history WHERE position<=?", {cursor}).toInt() > 0},
         {"redo_available",
          db->scalar("SELECT count(*) FROM history WHERE position>?", {cursor}).toInt() > 0},
         {"saved_path", sourcePath},
         {"recoverable", true}});
}
QCborArray ProjectStore::sketches(QString document) const {
    auto rows = db->rows("SELECT definition FROM sketch_entities WHERE entity_id=?",
                         {owner(*db, document)});
    return rows.empty() ? QCborArray{} : decode(rows[0][0]).value("sketches").toArray();
}
QCborArray ProjectStore::features(QString document) const {
    auto rows =
        db->rows("SELECT definition FROM features WHERE entity_id=?", {owner(*db, document)});
    return rows.empty() ? QCborArray{} : decode(rows[0][0]).value("features").toArray();
}
QCborMap ProjectStore::featureState(QString document)const{
    auto rows=db->rows("SELECT definition FROM features WHERE entity_id=?",{owner(*db,document)});return rows.empty()?values({{"features",QCborArray{}}}):decode(rows[0][0]);
}
QCborArray ProjectStore::profileSketches(QString document,QCborArray definitions)const{return gather(*db,document,definitions);}
kernel::Part ProjectStore::evaluatePart(QString document,std::optional<QCborArray> candidate,bool allowFailure)const{
    auto collection=featureState(document);auto definitions=candidate?*candidate:features(document);
    auto result=kernel::Part::build(profileSketches(document,definitions),definitions,&resultCache,allowFailure||collection.contains(QStringLiteral("checkpoint_features")));
    if(!candidate&&result.stale&&collection.contains(QStringLiteral("checkpoint_features"))){auto old=kernel::Part::build(collection.value("checkpoint_sketches").toArray(),collection.value("checkpoint_features").toArray(),&resultCache);old.stale=true;old.errors=result.errors;old.stages=result.stages;old.diagnostics=result.diagnostics;return old;}return result;
}
QCborMap ProjectStore::referenceCandidates(QString document,QString ownerId,QString token)const {
    auto definitions=features(document);auto collection=featureState(document);auto all=features::parse(definitions);auto f=std::find_if(all.begin(),all.end(),[&](auto &v){return v.id==ownerId;});check(f!=all.end()&&(f->kind=="Fillet"||f->kind=="Chamfer"),"UnsupportedOperation","Repair currently supports edge roles of fillet/chamfer");
    check(f->parameters.value("edges").toArray().contains(token),"ReferenceMissing","Former edge is not owned by this feature");auto source=f->parameters.value("source").toString();
    auto current=kernel::Part::build(profileSketches(document,definitions),definitions,&resultCache,true);auto former=kernel::Part::build(collection.contains(QStringLiteral("checkpoint_features"))?collection.value("checkpoint_sketches").toArray():profileSketches(document,definitions),collection.contains(QStringLiteral("checkpoint_features"))?collection.value("checkpoint_features").toArray():definitions,&resultCache);
    QCborMap result;
    if(current.stages.contains(source))result=current.reference(source,token,former);else result=values({{"state","Missing"},{"candidates",QCborArray{}}});
    evaluation::Plan plan(profileSketches(document,definitions),definitions);QByteArray hash;for(auto &n:plan.nodes)if(n.feature.id==source)hash=n.fingerprint;
    result.insert(QStringLiteral("owner"),ownerId);result.insert(QStringLiteral("source"),source);result.insert(QStringLiteral("token"),token);result.insert(QStringLiteral("source_fingerprint"),hash.toHex());result.insert(QStringLiteral("affected"),QCborArray::fromStringList(plan.descendants.value(ownerId)));return result;
}
QCborMap ProjectStore::outcome(QString op) const {
    domain::id(op);
    auto rows = db->rows("SELECT new_revision,outcome FROM commands WHERE operation_id=?", {op});
    return rows.empty() ? values({{"operation", op}, {"outcome", "Unknown"}, {"durable", false}})
                        : values({{"operation", op},
                                  {"outcome", "DurablyRecorded"},
                                  {"durable", true},
                                  {"revision", rows[0][0].toLongLong()},
                                  {"result", decode(rows[0][1])}});
}
QCborMap ProjectStore::deltaFor(const wire::Envelope &e, QCborMap &inverse) {
    auto &p = e.payload;
    auto doc = e.context.document;
    auto entity = owner(*db, doc);
    if (e.command == "CreateDocument" || e.command == "NewProject") {
        wire::keys(p, {"id", "type", "name", "units", "properties_entity"});
        auto id = domain::id(wire::text(p, "id")),
             properties = domain::id(wire::text(p, "properties_entity"));
        check(db->scalar("SELECT count(*) FROM documents").toInt() < 128, "ResourceLimit",
              "Document limit reached");
        auto type = domain::kind(wire::text(p, "type")),
             name = domain::label(wire::text(p, "name")),
             unit = domain::units(wire::text(p, "units"));
        inverse = values({{"type", "delete_document"}, {"id", id}});
        return values({{"type", "restore_document"},
                       {"id", id},
                       {"kind", type},
                       {"name", name},
                       {"units", unit},
                       {"entity", properties}});
    }
    check(!entity.isEmpty(), "ReferenceMissing", "Select an open document");
    if (e.command == "SetFeatures" || e.command=="CommitHistory" || e.command=="ResolveReference") {
        QCborArray incoming;
        if(e.command=="ResolveReference"){
            wire::keys(p,{"owner","token","replacement","source_fingerprint"});auto ownerId=domain::id(wire::text(p,"owner"));auto token=wire::text(p,"token");auto resolution=referenceCandidates(doc,ownerId,token);check(resolution.value("source_fingerprint")==p.value("source_fingerprint"),"StaleRevision","Source result changed before reference repair");check(resolution.value("candidates").toArray().contains(p.value("replacement")),"InvalidInput","Choose one of the current constrained candidates");auto all=features::parse(features(doc));for(auto &f:all)if(f.id==ownerId){auto edges=f.parameters.value("edges").toArray();for(qsizetype i=0;i<edges.size();++i)if(edges[i].toString()==token)edges[i]=p.value("replacement");f.parameters.insert(QStringLiteral("edges"),edges);}incoming=features::encode(all);
        }else{wire::keys(p,{"features"});check(p.value("features").isArray(),"InvalidInput","Feature array required");incoming=p.value("features").toArray();}
        check(db->scalar("SELECT type FROM documents WHERE id=?", {doc}).toString() == "Part",
              "UnsupportedOperation", "Features require a Part");
        QSet<QString> ids;
        for (auto row : db->rows("SELECT id FROM documents UNION SELECT id FROM entities"))
            ids.insert(row[0].toString());
        for (auto row : db->rows("SELECT id FROM documents")) {
            auto d = row[0].toString();
            for (auto &s : sketch::parse(sketches(d))) {
                ids.insert(s.id);
                for (auto &v : s.entities)
                    ids.insert(v.id);
                for (auto &v : s.constraints)
                    ids.insert(v.id);
            }
            if (d != doc)
                for (auto &f : features::parse(features(d)))
                    ids.insert(f.id);
        }
        for (auto &f : features::parse(incoming)) {
            check(!ids.contains(f.id), "InvalidInput", "Feature identity collision");
            ids.insert(f.id);
        }
        auto evaluated=kernel::Part::build(profileSketches(doc,incoming),incoming,&resultCache,e.command=="CommitHistory");
        if(e.command=="CommitHistory"){
            auto old=featureState(doc);auto checkpoint=old;
            if(!old.contains(QStringLiteral("checkpoint_features")))checkpoint=values({{"features",features(doc)},{"checkpoint_features",features(doc)},{"checkpoint_sketches",profileSketches(doc,features(doc))},{"stale",false}});
            auto next=values({{"features",incoming},{"checkpoint_features",evaluated.stale?checkpoint.value("checkpoint_features"):QCborValue(incoming)},{"checkpoint_sketches",evaluated.stale?checkpoint.value("checkpoint_sketches"):QCborValue(profileSketches(doc,incoming))},{"stale",evaluated.stale}});
            inverse=values({{"type","set_feature_state"},{"owner",entity},{"document",doc},{"collection",checkpoint}});
            return values({{"type","set_feature_state"},{"owner",entity},{"document",doc},{"collection",next}});
        }
        inverse = values({{"type", "set_features"},
                          {"owner", entity},
                          {"document", doc},
                          {"features", features(doc)}});
        return values({{"type", "set_features"},
                       {"owner", entity},
                       {"document", doc},
                       {"features", incoming}});
    }
    if (e.command == "SetSketches") {
        wire::keys(p, {"sketches"});
        check(p.value("sketches").isArray(), "InvalidInput", "Sketch array required");
        check(db->scalar("SELECT type FROM documents WHERE id=?", {doc}).toString() == "Part",
              "UnsupportedOperation", "Sketch editing requires a Part document");
        auto solved = sketch::evaluate(p.value("sketches").toArray(), sketches(doc));
        auto all = sketch::parse(solved.value("sketches").toArray());
        qint64 total = QCborValue(solved.value("sketches")).toCbor().size();
        for (auto row : db->rows("SELECT s.definition FROM sketch_entities s JOIN entities e ON "
                                 "e.id=s.entity_id WHERE e.document_id!=?",
                                 {doc}))
            total += row[0].toByteArray().size();
        check(total <= 262144, "ResourceLimit",
              "Project sketch data exceeds the 256 KiB control-projection budget");
        QSet<QString> ids;
        for (auto row : db->rows("SELECT id FROM documents UNION SELECT id FROM entities"))
            ids.insert(row[0].toString());
        for (auto row : db->rows("SELECT id FROM documents WHERE id!=?", {doc}))
            for (auto &other : sketch::parse(sketches(row[0].toString()))) {
                ids.insert(other.id);
                for (auto &e : other.entities)
                    ids.insert(e.id);
                for (auto &c : other.constraints)
                    ids.insert(c.id);
            }
        for (auto row : db->rows("SELECT id FROM documents"))
            for (auto &f : features::parse(features(row[0].toString())))
                ids.insert(f.id);
        for (auto &s : all) {
            QStringList incoming{s.id};
            for (auto &e : s.entities)
                incoming.append(e.id);
            for (auto &c : s.constraints)
                incoming.append(c.id);
            for (auto id : incoming) {
                check(!ids.contains(id), "InvalidInput", "Project identity collision");
                ids.insert(id);
            }
        }
        for(auto row:db->rows("SELECT id FROM documents")){auto other=row[0].toString();auto defs=features(other);kernel::Part::build(gather(*db,other,defs,doc,sketch::encode(all)),defs,&resultCache); }
        inverse = values({{"type", "set_sketches"},
                          {"owner", entity},
                          {"document", doc},
                          {"sketches", sketches(doc)}});
        return values({{"type", "set_sketches"},
                       {"owner", entity},
                       {"document", doc},
                       {"sketches", sketch::encode(all)}});
    }
    if (e.command == "RenameDocument" || e.command == "SetDocumentUnits") {
        bool name = e.command == "RenameDocument";
        wire::keys(p, {name ? "name" : "units"});
        auto sql = name ? "SELECT name FROM documents WHERE id=?"
                        : "SELECT units FROM documents WHERE id=?";
        auto value =
            name ? domain::label(wire::text(p, "name")) : domain::units(wire::text(p, "units"));
        inverse = values({{"type", name ? "rename" : "units"},
                          {"id", doc},
                          {"value", db->scalar(sql, {doc}).toString()}});
        return values({{"type", name ? "rename" : "units"}, {"id", doc}, {"value", value}});
    }
    if (e.command == "SetParameter" || e.command == "RemoveParameter") {
        QString name;
        domain::Parameter parameter;
        if (e.command == "SetParameter") {
            parameter = domain::Parameter::parse(p);
            name = parameter.name;
        } else {
            wire::keys(p, {"name"});
            name = wire::text(p, "name");
            check(QRegularExpression("^[A-Za-z][A-Za-z0-9_]{0,63}$").match(name).hasMatch(),
                  "InvalidInput", "Invalid parameter name");
        }
        auto old = db->rows("SELECT dimension,value_si FROM parameters WHERE owner=? AND name=?",
                            {entity, name});
        inverse = values(
            {{"type", "remove_parameter"}, {"owner", entity}, {"name", name}, {"document", doc}});
        if (!old.empty()) {
            inverse.insert(QStringLiteral("type"), "set_parameter");
            inverse.insert(QStringLiteral("dimension"), old[0][0].toString());
            inverse.insert(QStringLiteral("value_si"), old[0][1].toDouble());
        }
        if (e.command == "RemoveParameter") {
            check(!old.empty(), "ReferenceMissing", "Parameter does not exist");
            return values({{"type", "remove_parameter"},
                           {"owner", entity},
                           {"name", name},
                           {"document", doc}});
        }
        check(!old.empty() || db->scalar("SELECT count(*) FROM parameters").toInt() < 1024,
              "ResourceLimit", "Project parameter limit reached");
        auto delta = parameter.map();
        delta.insert(QStringLiteral("type"), "set_parameter");
        delta.insert(QStringLiteral("owner"), entity);
        delta.insert(QStringLiteral("document"), doc);
        return delta;
    }
    if (e.command == "DetachObject") {
        wire::keys(p, {"object"});
        auto hash = wire::text(p, "object");
        check(db->scalar("SELECT count(*) FROM entity_objects WHERE owner=? AND object_id=?",
                         {entity, hash})
                      .toInt() == 1,
              "ReferenceMissing", "Attachment is absent");
        inverse = values(
            {{"type", "attach_object"}, {"owner", entity}, {"hash", hash}, {"document", doc}});
        return values(
            {{"type", "detach_object"}, {"owner", entity}, {"hash", hash}, {"document", doc}});
    }
    throw domain::Error("UnsupportedOperation", "Command is not supported by Module 2");
}
void ProjectStore::apply(const QCborMap &d, qint64 revision) {
    validateDelta(d);
    auto type = wire::text(d, "type");
    if (type == "restore_document") {
        auto id = wire::text(d, "id");
        db->run(
            "INSERT INTO documents VALUES(?,?,?,?,?)",
            {id, wire::text(d, "kind"), wire::text(d, "name"), wire::text(d, "units"), revision});
        db->run("INSERT INTO entities VALUES(?,?,'DocumentProperties',1)",
                {wire::text(d, "entity"), id});
        activeDocument = id;
        if (!opened.contains(id))
            opened.append(id);
    } else if (type == "delete_document") {
        auto id = wire::text(d, "id");
        db->run("DELETE FROM documents WHERE id=?", {id});
        opened.removeAll(id);
        if (activeDocument == id)
            activeDocument = opened.isEmpty() ? QString() : opened.first();
    } else if (type == "rename" || type == "units")
        db->run(type == "rename" ? "UPDATE documents SET name=?,revision=? WHERE id=?"
                                 : "UPDATE documents SET units=?,revision=? WHERE id=?",
                {wire::text(d, "value"), revision, wire::text(d, "id")});
    else {
        auto entity = wire::text(d, "owner");
        check(db->scalar("SELECT count(*) FROM entities WHERE id=? AND document_id=?",
                         {entity, wire::text(d, "document")})
                      .toInt() == 1,
              "ReferenceMissing", "History entity/document context no longer resolves");
        if(type=="set_feature_state"){
            auto collection=d.value("collection").toMap();auto defs=collection.value("features").toArray();auto evaluated=kernel::Part::build(profileSketches(wire::text(d,"document"),defs),defs,&resultCache,true);
            check(evaluated.stale==collection.value("stale").toBool(),"StaleRevision","History result changed; refresh impact preview");
            if(defs.empty())db->run("DELETE FROM features WHERE entity_id=?",{entity});else db->run("INSERT INTO features VALUES(?,1,0,?) ON CONFLICT(entity_id) DO UPDATE SET definition=excluded.definition",{entity,encode(collection)});
            db->run("UPDATE project_meta SET manifest=?",{historyManifest});
        } else if (type == "set_features") {
            auto a = d.value("features").toArray();
            kernel::Part::build(profileSketches(wire::text(d,"document"),a),a,&resultCache);
            if (a.empty())
                db->run("DELETE FROM features WHERE entity_id=?", {entity});
            else
                db->run("INSERT INTO features VALUES(?,1,0,?) ON CONFLICT(entity_id) DO UPDATE SET "
                        "definition=excluded.definition",
                        {entity, encode(values({{"features", a}}))});
            db->run("UPDATE project_meta SET manifest=? WHERE manifest!=?", {featureManifest,historyManifest});
        } else if (type == "set_sketches") {
            kernel::Part::build(gather(*db,wire::text(d,"document"),features(wire::text(d,"document")),wire::text(d,"document"),d.value("sketches").toArray()),features(wire::text(d,"document")),&resultCache);
            auto bytes = encode(values({{"sketches", d.value("sketches")}}));
            if (d.value("sketches").toArray().empty())
                db->run("DELETE FROM sketch_entities WHERE entity_id=?", {entity});
            else
                db->run(
                    "INSERT INTO sketch_entities VALUES(?,?) ON CONFLICT(entity_id) DO UPDATE SET "
                    "definition=excluded.definition",
                    {entity, bytes});
            db->run("UPDATE project_meta SET manifest=? WHERE manifest NOT IN (?,?)",
                    {sketchManifest, featureManifest,historyManifest});
        } else if (type == "set_parameter")
            db->run("INSERT INTO parameters VALUES(?,?,?,?) ON CONFLICT(owner,name) DO UPDATE SET "
                    "dimension=excluded.dimension,value_si=excluded.value_si",
                    {entity, wire::text(d, "name"), wire::text(d, "dimension"),
                     d.value("value_si").toDouble()});
        else if (type == "remove_parameter")
            db->run("DELETE FROM parameters WHERE owner=? AND name=?",
                    {entity, wire::text(d, "name")});
        else if (type == "attach_object")
            db->run("INSERT INTO entity_objects VALUES(?,?)", {entity, wire::text(d, "hash")});
        else if (type == "detach_object")
            db->run("DELETE FROM entity_objects WHERE owner=? AND object_id=?",
                    {entity, wire::text(d, "hash")});
        else
            throw domain::Error("InvalidProject", "Unknown semantic delta");
        db->run("UPDATE documents SET revision=? WHERE id=?",
                {revision, wire::text(d, "document")});
    }
    db->exec("DELETE FROM object_refs WHERE root_type IN ('document','recovery')");
    db->exec("INSERT INTO object_refs SELECT 'document',owner,object_id FROM entity_objects");
    db->exec("INSERT INTO object_refs SELECT 'recovery','current',object_id FROM entity_objects "
             "GROUP BY object_id");
}
void ProjectStore::record(const wire::Envelope &e, QByteArray hash, QCborMap before, QCborMap after,
                          qint64 rev, QString category) {
    auto result =
        values({{"revision", rev}, {"status", category == "save" ? "Saved" : "Recoverable"}});
    db->run("INSERT INTO commands VALUES(?,?,?,?,?,?,?,?,?,?)",
            {e.operation, hash, e.context.document, e.revision, rev, category,
             encode(values({{"command", e.command}, {"payload", e.payload}})), encode(before),
             encode(after), encode(result)});
}
void ProjectStore::retainHistory() {
    while (db->scalar("SELECT count(*) FROM history").toInt() > options.historyEntries ||
           db->scalar("SELECT coalesce(sum(bytes),0) FROM history").toLongLong() >
               options.historyBytes)
        db->exec("DELETE FROM history WHERE position=(SELECT min(position) FROM history)");
    db->exec("DELETE FROM object_refs WHERE root_type='history'");
    for (const auto &r : db->rows("SELECT c.operation_id,c.before_delta,c.after_delta FROM history "
                                  "h JOIN commands c ON c.operation_id=h.operation_id"))
        for (int i = 1; i <= 2; ++i) {
            auto d = decode(r[i]);
            if (d.contains(QStringLiteral("hash")))
                db->run("INSERT OR IGNORE INTO object_refs VALUES('history',?,?)",
                        {r[0], wire::text(d, "hash")});
        }
}
QCborMap ProjectStore::mutate(const wire::Envelope &e) {
    auto hash = domain::requestHash(e);
    auto prior = db->rows("SELECT request_hash FROM commands WHERE operation_id=?", {e.operation});
    if (!prior.empty()) {
        check(prior[0][0].toByteArray() == hash, "InvalidInput",
              "Operation ID was reused with different semantic content");
        return outcome(e.operation);
    }
    check(e.context.project == projectId, "StaleSession", "Project identity changed");
    check(e.revision == revision(), "StaleRevision", "Project changed; refresh before editing");
    check(revision() < INT64_MAX - 1, "ResourceLimit", "Revision counter exhausted");
    check(db->scalar("SELECT count(*) FROM commands").toLongLong() < 100000, "ResourceLimit",
          "Durable operation ledger requires archival before further edits");
    auto previousActive = activeDocument;
    auto previousOpened = opened;
    bool committed = false;
    Transaction tx(*db);
    try {
        qint64 next = revision() + 1,
               cursor = db->scalar("SELECT history_cursor FROM project_meta").toLongLong();
        QCborMap before, after;
        if (e.command == "Undo" || e.command == "Redo") {
            wire::keys(e.payload, {});
            bool undo = e.command == "Undo";
            auto step =
                db->rows(undo ? "SELECT h.position,c.before_delta,c.after_delta FROM history h "
                                "JOIN commands c ON c.operation_id=h.operation_id WHERE "
                                "h.position<=? ORDER BY h.position DESC LIMIT 1"
                              : "SELECT h.position,c.before_delta,c.after_delta FROM history h "
                                "JOIN commands c ON c.operation_id=h.operation_id WHERE "
                                "h.position>? ORDER BY h.position LIMIT 1",
                         {cursor});
            check(!step.empty(), "HistoryBoundary",
                  undo ? "Undo retention boundary reached" : "No redo step is available");
            before = decode(step[0][undo ? 2 : 1]);
            after = decode(step[0][undo ? 1 : 2]);
            apply(after, next);
            cursor = step[0][0].toLongLong() - (undo ? 1 : 0);
        } else {
            after = deltaFor(e, before);
            apply(after, next);
            db->run("DELETE FROM history WHERE position>?", {cursor});
            ++cursor;
        }
        db->run("UPDATE project_meta SET revision=?,history_cursor=?", {next, cursor});
        record(e, hash, before, after, next, "edit");
        if (e.command != "Undo" && e.command != "Redo")
            db->run("INSERT INTO history VALUES(?,?,?)",
                    {cursor, e.operation, encode(before).size() + encode(after).size()});
        retainHistory();
        check(QCborValue(state()).toCbor().size() + QCborValue(e.map()).toCbor().size() + 4096 <=
                  qint64(limits::control),
              "ResourceLimit", "Project reply exceeds the bounded control-frame budget");
        point("before_commit");
        tx.commit();
        committed = true;
        point("after_commit");
        return outcome(e.operation);
    } catch (...) {
        if (!committed) {
            activeDocument = previousActive;
            opened = previousOpened;
        }
        throw;
    }
}
QCborMap ProjectStore::execute(const wire::Envelope &e) {
    check(isOpen(), "InvalidInput", "Open or create a project first");
    if (e.command == "SwitchDocument" || e.command == "CloseDocument") {
        wire::keys(e.payload, {});
        check(e.context.project == projectId && e.revision == revision(), "StaleRevision",
              "Document context changed");
        check(
            db->scalar("SELECT count(*) FROM documents WHERE id=?", {e.context.document}).toInt() ==
                1,
            "ReferenceMissing", "Document does not exist");
        if (e.command == "SwitchDocument") {
            activeDocument = e.context.document;
            if (!opened.contains(activeDocument))
                opened.append(activeDocument);
        } else {
            opened.removeAll(e.context.document);
            activeDocument = opened.isEmpty() ? QString() : opened.first();
        }
        return {};
    }
    return mutate(e);
}
QCborMap ProjectStore::inspect(QString path) const {
    path = canonicalDestination(path);
    QFile header(path);
    check(header.open(QIODevice::ReadOnly) && header.size() <= qint64(limits::project),
          "InvalidProject", "Saved project cannot be read");
    auto bytes = header.read(100);
    check(bytes.size() == 100 && bytes.startsWith(QByteArray("SQLite format 3\0", 16)) &&
              bytes[18] == 1 && bytes[19] == 1,
          "InvalidProject",
          "Saved projects must be standalone SQLite snapshots, not live WAL databases");
    header.close();
    Database saved(path, true);
    saved.exec("BEGIN");
    validate(saved, true);
    auto id = saved.scalar("SELECT id FROM project_meta").toString();
    auto savedRevision = saved.scalar("SELECT revision FROM project_meta").toLongLong();
    qint64 recoverable = -1;
    if (QFileInfo::exists(pointer(root, id))) {
        try {
            auto work = workspaceFor(root, id);
            Database candidate(work + "/working.sqlite", true);
            candidate.exec("BEGIN");
            validate(candidate);
            if (candidate.scalar("SELECT id FROM project_meta").toString() == id)
                recoverable = candidate.scalar("SELECT revision FROM project_meta").toLongLong();
        } catch (const domain::Error &) { /* An invalid recovery is never advertised as valid. Saved
                                             state remains readable. */
        }
    }
    return values({{"project_id", id},
                   {"saved_revision", savedRevision},
                   {"recoverable_revision", recoverable},
                   {"path", path}});
}
void ProjectStore::open(QString file, QString choice) {
    check(!isOpen(), "InvalidInput", "Close the current project first");
    check(choice == "Saved" || choice == "Recoverable", "InvalidInput",
          "Choose Saved or Recoverable explicitly");
    auto held = std::make_unique<DestinationLocks>(root, file);
    auto info = inspect(held->path);
    auto id = wire::text(info, "project_id");
    if (choice == "Recoverable") {
        held.reset();
        recover(id);
        return;
    }
    projectLock = std::make_unique<Lock>(root, "project:" + id);
    projectId = id;
    sourcePath = held->path;
    destination = std::move(held);
    try {
        createWorkspace();
        {
            Database source(sourcePath, true);
            Database target(workingPath());
            backup(source, target, options.boundary);
            validate(target, true);
            check(target.scalar("SELECT id FROM project_meta").toString() == id, "InvalidProject",
                  "Saved identity changed during open");
        }
        db = std::make_unique<Database>(workingPath());
        configureWorking();
        db->run("UPDATE local_meta SET saved_path=?", {sourcePath});
        opened.clear();
        for (const auto &r : db->rows("SELECT id FROM documents ORDER BY rowid"))
            opened.append(r[0].toString());
        activeDocument = opened.isEmpty() ? QString() : opened.first();
        activate();
    } catch (...) {
        close();
        throw;
    }
}
void ProjectStore::recover(QString id) {
    check(!isOpen(), "InvalidInput", "Close the current project first");
    domain::id(id);
    projectLock = std::make_unique<Lock>(root, "project:" + id);
    projectId = id;
    try {
        workspace = workspaceFor(root, id);
        check(!QFileInfo(workspace).isSymLink() && !QFileInfo(workingPath()).isSymLink(),
              "PermissionDenied", "Recovery cannot follow redirected workspace files");
        workingLock = std::make_unique<Lock>(root, "workspace:" + workspace);
        db = std::make_unique<Database>(workingPath());
        validate(*db);
        check(db->scalar("SELECT id FROM project_meta").toString() == id, "InvalidProject",
              "Recovery index identity mismatch");
        configureWorking();
        cleanupStaging();
        sourcePath = db->scalar("SELECT saved_path FROM local_meta").toString();
        if (!sourcePath.isEmpty())
            destination = std::make_unique<DestinationLocks>(root, sourcePath);
        reconcile();
        db->exec("DELETE FROM object_refs WHERE root_type IN ('job','export')");
        opened.clear();
        for (const auto &r : db->rows("SELECT id FROM documents ORDER BY rowid"))
            opened.append(r[0].toString());
        activeDocument = opened.isEmpty() ? QString() : opened.first();
    } catch (...) {
        close();
        throw;
    }
}
QCborArray ProjectStore::recoveryOffers() const {
    QCborArray offers;
    auto indices = QDir(root + "/index").entryList(QDir::Files | QDir::NoSymLinks);
    check(indices.size() <= 128, "ResourceLimit",
          "Recovery index exceeds the bounded project count");
    for (const auto &id : indices) {
        if (!wire::uuid(id))
            continue;
        try {
            auto work = workspaceFor(root, id);
            if (QFileInfo(work).isSymLink() || QFileInfo(work + "/working.sqlite").isSymLink())
                continue;
            Database candidate(work + "/working.sqlite", true);
            candidate.exec("BEGIN");
            validate(candidate);
            auto current = candidate.scalar("SELECT revision FROM project_meta").toLongLong(),
                 saved = candidate.scalar("SELECT saved_revision FROM project_meta").toLongLong();
            offers.append(
                values({{"project_id", id},
                        {"revision", current},
                        {"saved_revision", saved},
                        {"path", candidate.scalar("SELECT saved_path FROM local_meta").toString()},
                        {"newer_than_saved", current > saved}}));
        } catch (const domain::Error &) {
            offers.append(values({{"project_id", id}, {"invalid", true}}));
        }
    }
    return offers;
}
namespace {
void putSaveRecord(Database &db, const wire::Envelope &e, const QByteArray &hash, QString project,
                   qint64 rev) {
    auto answer = values({{"revision", rev}, {"status", "Saved"}, {"saved_project_id", project}});
    db.run("INSERT OR IGNORE INTO commands VALUES(?,?,?,?,?,'save',?,?,?,?)",
           {e.operation, hash, e.context.document, e.revision, rev,
            encode(values({{"command", e.command}, {"payload", e.payload}})), encode({}),
            encode({}), encode(answer)});
}
} // namespace
QCborMap ProjectStore::save(const wire::Envelope &request, QString path, bool saveAs) {
    check(isOpen(), "InvalidInput", "No project is open");
    path = canonicalDestination(path);
    check(path != workingPath() && !path.startsWith(root + '/'), "InvalidInput",
          "Saved destinations must be outside private recovery storage");
    validate(*db);
    cleanupStaging();
    auto e = request;
    QString newProject = saveAs ? domain::id(wire::text(e.payload, "new_project_id")) : projectId;
    e.payload =
        values({{"destination_hash",
                 QString::fromLatin1(
                     QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex())},
                {"new_project_id", newProject}});
    auto hash = domain::requestHash(e);
    auto existing =
        db->rows("SELECT request_hash FROM commands WHERE operation_id=?", {e.operation});
    if (!existing.empty()) {
        check(existing[0][0].toByteArray() == hash, "InvalidInput",
              "Operation ID reused with different save content");
        return outcome(e.operation);
    }
    check(!saveAs || newProject != projectId, "InvalidInput",
          "Save As needs a new project identity");
    check(db->scalar("SELECT count(*) FROM commands").toLongLong() < 100000, "ResourceLimit",
          "Durable ledger admission limit reached");
    check(db->scalar("SELECT count(*) FROM save_intents WHERE completed=0").toInt() < 32,
          "ResourceLimit", "Review uncertain saves before issuing more save requests");
    check(e.context.project == projectId && e.revision == revision(), "StaleRevision",
          "Requested snapshot revision is no longer current");
    check(!saveAs || sourcePath.isEmpty() || path != canonicalDestination(sourcePath),
          "InvalidInput", "Save As cannot replace its source project");
    std::unique_ptr<Lock> newIdentity;
    if (saveAs)
        newIdentity = std::make_unique<Lock>(root, "project:" + newProject);
    std::unique_ptr<DestinationLocks> newDestination;
    if (!destination || destination->path != path) {
        newDestination = std::make_unique<DestinationLocks>(root, path);
    }
    if (QFileInfo::exists(path)) {
        auto old = inspect(path);
        check(!saveAs && wire::text(old, "project_id") == projectId, "InvalidInput",
              "Destination belongs to another project; choose a different filename");
    }
    spaceFor(QFileInfo(path).absolutePath(),
             QFileInfo(workingPath()).size() * 2 + QFileInfo(path).size() * 2 + (8LL << 20));
    {
        Transaction tx(*db);
        db->run(
            "INSERT INTO save_intents VALUES(?,?,?,?,?,0) ON CONFLICT(operation_id) DO UPDATE SET "
            "request_hash=excluded.request_hash,path=excluded.path,new_project_id=excluded.new_"
            "project_id,revision=excluded.revision",
            {e.operation, hash, path, newProject, revision()});
        tx.commit();
    }
    // A failed attempt before publication has no uncertain saved outcome. Retain
    // intents once rename may have started, including a failed directory flush.
    struct SaveAttempt {
        Database &database;
        QString operation;
        int exceptions = std::uncaught_exceptions();
        bool publicationStarted = false;
        ~SaveAttempt() noexcept {
            if (!publicationStarted && std::uncaught_exceptions() > exceptions) {
                try {
                    database.run("DELETE FROM save_intents WHERE operation_id=?", {operation});
                } catch (...) {
                    // Preserve the original error; recovery can reconcile this intent.
                }
            }
        }
    } attempt{*db, e.operation};
    QTemporaryFile stage(QFileInfo(path).absolutePath() + "/.cad-snapshot-XXXXXX");
    check(stage.open(), "PermissionDenied", "Cannot create snapshot beside destination");
    auto temp = stage.fileName();
    db->run("INSERT INTO staging_files VALUES(?,?,?,?)",
            {temp, fileIdentity(temp), path, e.operation});
    stage.close();
    auto savedRevision = revision();
    {
        Database target(temp);
        backup(*db, target, options.boundary);
        {
            Transaction tx(target);
            target.run("UPDATE project_meta SET "
                       "id=?,saved_revision=revision,save_generation=save_generation+1",
                       {newProject});
            target.exec("UPDATE local_meta SET saved_path=''; DELETE FROM save_intents; DELETE "
                        "FROM staging_files; DELETE FROM object_refs WHERE root_type IN "
                        "('saved','job','export')");
            target.exec("INSERT INTO object_refs SELECT 'saved','snapshot',object_id FROM "
                        "entity_objects GROUP BY object_id");
            putSaveRecord(target, e, hash, newProject, savedRevision);
            tx.commit();
        }
        target.exec("PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL");
        validate(target, true);
        check(target.scalar("SELECT revision FROM project_meta").toLongLong() == savedRevision,
              "StaleRevision", "Snapshot did not retain the pinned revision");
    }
    point("snapshot_validated");
    flushFile(temp);
    if (QFileInfo::exists(path)) {
        QTemporaryFile backupStage(QFileInfo(path).absolutePath() + "/.cad-backup-XXXXXX");
        check(backupStage.open(), "PermissionDenied", "Cannot stage previous saved snapshot");
        auto prior = backupStage.fileName();
        backupStage.close();
        check(QFile::remove(prior) && QFile::copy(path, prior), "StorageError",
              "Cannot preserve previous saved snapshot");
        db->run("INSERT INTO staging_files VALUES(?,?,?,?)",
                {prior, fileIdentity(prior), path, e.operation});
        flushFile(prior);
        publish(prior, path + ".bak");
    }
    point("before_publication");
    attempt.publicationStarted = true;
    publish(temp, path);
    stage.setAutoRemove(false);
    point("after_publication");
    {
        Transaction tx(*db);
        putSaveRecord(*db, e, hash, newProject, savedRevision);
        db->run("UPDATE save_intents SET completed=1 WHERE operation_id=?", {e.operation});
        if (!saveAs) {
            db->run("UPDATE project_meta SET saved_revision=?,save_generation=save_generation+1",
                    {savedRevision});
            db->run("UPDATE local_meta SET saved_path=?", {path});
            db->exec("DELETE FROM object_refs WHERE root_type='saved'");
            db->exec("INSERT INTO object_refs SELECT 'saved','snapshot',object_id FROM "
                     "entity_objects GROUP BY object_id");
        }
        tx.commit();
    }
    db->run("DELETE FROM staging_files WHERE owner_operation=?", {e.operation});
    db->exec("DELETE FROM save_intents WHERE completed=1");
    point("save_recorded");
    auto answer = outcome(e.operation);
    if (saveAs) {
        newDestination.reset();
        newIdentity.reset();
        close();
        open(path, "Saved");
    } else {
        sourcePath = path;
        if (newDestination)
            destination = std::move(newDestination);
        destination->file.reset();
    }
    return answer;
}
void ProjectStore::cleanupStaging() {
    for (const auto &row : db->rows("SELECT path,identity,destination FROM staging_files")) {
        auto path = row[0].toString(), dest = row[2].toString();
        auto name = QFileInfo(path).fileName();
        check(QFileInfo(path).absolutePath() == QFileInfo(dest).absolutePath() &&
                  (name.startsWith(".cad-snapshot-") || name.startsWith(".cad-backup-")),
              "InvalidProject", "Unrecognized staging ownership record");
        if (QFileInfo::exists(path)) {
            check(!QFileInfo(path).isSymLink() && fileIdentity(path) == row[1].toString(),
                  "PermissionDenied", "Staged file identity changed; preserved for inspection");
            check(QFile::remove(path), "PermissionDenied", "Cannot clean abandoned staged file");
        }
        db->run("DELETE FROM staging_files WHERE path=?", {path});
    }
    for (const auto &file :
         QDir(workspace).entryInfoList({".object-*"}, QDir::Files | QDir::NoSymLinks))
        check(QFile::remove(file.absoluteFilePath()), "PermissionDenied",
              "Cannot release abandoned private object staging");
}
void ProjectStore::reconcile() {
    for (const auto &row : db->rows("SELECT operation_id,request_hash,path,new_project_id,revision "
                                    "FROM save_intents WHERE completed=0")) {
        auto op = row[0].toString(), path = row[2].toString(), newProject = row[3].toString();
        if (!QFileInfo::exists(path))
            continue;
        try {
            Database saved(path, true);
            validate(saved, true);
            auto recorded =
                saved.rows("SELECT request_hash,outcome FROM commands WHERE operation_id=?", {op});
            if (recorded.empty() || recorded[0][0].toByteArray() != row[1].toByteArray() ||
                saved.scalar("SELECT id FROM project_meta").toString() != newProject)
                continue;
            Transaction tx(*db);
            auto command = saved.rows(
                "SELECT "
                "operation_id,request_hash,document_id,base_revision,new_revision,category,intent,"
                "before_delta,after_delta,outcome FROM commands WHERE operation_id=?",
                {op});
            auto c = command.first();
            db->run("INSERT OR IGNORE INTO commands VALUES(?,?,?,?,?,?,?,?,?,?)", c);
            db->run("UPDATE save_intents SET completed=1 WHERE operation_id=?", {op});
            if (newProject == projectId) {
                db->run("UPDATE project_meta SET "
                        "saved_revision=?,save_generation=max(save_generation,?)",
                        {row[4], saved.scalar("SELECT save_generation FROM project_meta")});
                db->run("UPDATE local_meta SET saved_path=?", {path});
                sourcePath = path;
                db->exec("DELETE FROM object_refs WHERE root_type='saved'");
                for (const auto &object :
                     saved.rows("SELECT object_id FROM entity_objects GROUP BY object_id")) {
                    if (db->scalar("SELECT count(*) FROM objects WHERE id=?", {object[0]}).toInt())
                        db->run("INSERT INTO object_refs VALUES('saved','snapshot',?)",
                                {object[0]});
                }
            }
            tx.commit();
        } catch (const domain::Error
                     &) { /* Not a verified publication; report no fabricated Saved result. */
        }
    }
}
QString ProjectStore::attach(const wire::Envelope &e, QIODevice &input, qint64 declared,
                             QString classification) {
    check(isOpen() && e.context.project == projectId, "StaleSession",
          "Open the matching project before attaching data");
    wire::keys(e.payload, {"size", "expected_sha256", "classification"});
    check(wire::integer(e.payload, "size") == declared &&
              wire::text(e.payload, "classification") == classification,
          "InvalidInput", "Object metadata does not match the streamed request");
    check(classification == "authoritative" || classification == "cache", "InvalidInput",
          "Unknown object classification");
    check(declared > 0 && declared <= qint64(limits::object), "ResourceLimit",
          "Object exceeds the 128 MiB stream limit");
    auto hashRequest = domain::requestHash(e);
    auto previous =
        db->rows("SELECT request_hash FROM commands WHERE operation_id=?", {e.operation});
    if (!previous.empty()) {
        check(previous[0][0].toByteArray() == hashRequest, "InvalidInput",
              "Operation ID reused with other object content");
        return wire::text(decode(db->scalar("SELECT after_delta FROM commands WHERE operation_id=?",
                                            {e.operation})),
                          "hash");
    }
    auto entity = owner(*db, e.context.document);
    check(!entity.isEmpty(), "ReferenceMissing", "No document properties entity");
    check(e.revision == revision(), "StaleRevision", "Refresh before attaching data");
    check(db->scalar("SELECT count(*) FROM commands").toLongLong() < 100000, "ResourceLimit",
          "Durable ledger admission limit reached");
    spaceFor(workspace, declared * 3);
    QTemporaryFile staged(workspace + "/.object-XXXXXX");
    check(staged.open(), "PermissionDenied", "Cannot create bounded object staging file");
    QCryptographicHash digest(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (total < declared) {
        auto chunk = input.read(qMin<qint64>(65536, declared - total));
        check(!chunk.isEmpty(), "InvalidInput", "Object stream ended before its declared size");
        check(staged.write(chunk) == chunk.size(), "StorageFull", "Cannot stage object stream");
        digest.addData(chunk);
        total += chunk.size();
    }
    check(input.read(1).isEmpty(), "ResourceLimit", "Object stream exceeds its declared size");
    check(staged.flush(), "StorageFull", "Object staging flush failed");
    QString hash = QString::fromLatin1(digest.result().toHex());
    check(wire::text(e.payload, "expected_sha256") == hash, "InvalidInput",
          "Object source changed from the requested content hash");
    Transaction tx(*db);
    qint64 next = revision() + 1;
    auto before = values({{"type", "detach_object"},
                          {"owner", entity},
                          {"hash", hash},
                          {"document", e.context.document}}),
         after = values({{"type", "attach_object"},
                         {"owner", entity},
                         {"hash", hash},
                         {"document", e.context.document}});
    if (!db->scalar("SELECT count(*) FROM objects WHERE id=?", {hash}).toInt()) {
        db->run("INSERT INTO objects VALUES(?,'attachment','opaque-v1',?,?,zeroblob(?))",
                {hash, classification, declared, declared});
        auto row = sqlite3_last_insert_rowid(db->handle);
        sqlite3_blob *raw = nullptr;
        sqlcheck(sqlite3_blob_open(db->handle, "main", "objects", "payload", row, 1, &raw),
                 db->handle);
        std::unique_ptr<sqlite3_blob, decltype(&sqlite3_blob_close)> blob(raw, sqlite3_blob_close);
        check(staged.seek(0), "StorageError", "Cannot rewind staged object");
        qint64 at = 0;
        while (at < declared) {
            auto bytes = staged.read(65536);
            check(!bytes.isEmpty(), "StorageError", "Cannot read staged object");
            sqlcheck(sqlite3_blob_write(raw, bytes.constData(), bytes.size(), int(at)), db->handle);
            at += bytes.size();
        }
        blob.reset();
    } else if (classification == "authoritative")
        db->run("UPDATE objects SET classification='authoritative' WHERE id=?", {hash});
    check(db->scalar("SELECT count(*) FROM entity_objects WHERE owner=? AND object_id=?",
                     {entity, hash})
                  .toInt() == 0,
          "InvalidInput", "Object is already attached");
    apply(after, next);
    auto cursor = db->scalar("SELECT history_cursor FROM project_meta").toLongLong();
    db->run("DELETE FROM history WHERE position>?", {cursor});
    db->run("UPDATE project_meta SET revision=?,history_cursor=?", {next, cursor + 1});
    record(e, hashRequest, before, after, next, "edit");
    db->run("INSERT INTO history VALUES(?,?,?)",
            {cursor + 1, e.operation, encode(before).size() + encode(after).size()});
    retainHistory();
    point("before_commit");
    tx.commit();
    point("after_commit");
    return hash;
}
void ProjectStore::retain(QString type, QString id, QString hash) {
    check(type == "job" || type == "export", "InvalidInput",
          "Only active jobs/exports may add runtime roots");
    domain::id(id);
    check(
        db->scalar("SELECT count(*) FROM object_refs WHERE root_type IN ('job','export')").toInt() <
            256,
        "ResourceLimit", "Active retention root limit reached");
    db->run("INSERT OR IGNORE INTO object_refs VALUES(?,?,?)", {type, id, hash});
}
void ProjectStore::release(QString type, QString id) {
    check(type == "job" || type == "export", "InvalidInput",
          "Cannot release authoritative recovery/history roots");
    db->run("DELETE FROM object_refs WHERE root_type=? AND root_id=?", {type, id});
}
void ProjectStore::collect() {
    Transaction tx(*db);
    retainHistory();
    db->exec("DELETE FROM objects WHERE id NOT IN (SELECT object_id FROM object_refs UNION SELECT "
             "object_id FROM entity_objects)");
    tx.commit();
}
void ProjectStore::migrationCopy(QString source, QString destination, const Boundary &observer) {
    source = canonicalDestination(source);
    destination = canonicalDestination(destination);
    check(source != destination && !QFileInfo::exists(destination), "InvalidInput",
          "Migration must target a new file and preserve the source");
    Database old(source, true);
    check(old.scalar("PRAGMA application_id").toInt() == application, "InvalidProject",
          "Not a CAD project");
    check(old.scalar("PRAGMA user_version").toInt() < 1, "IncompatibleSchema",
          "Unsupported migration source; original unchanged");
    QTemporaryFile staged(QFileInfo(destination).absolutePath() + "/.cad-migration-XXXXXX");
    check(staged.open(), "PermissionDenied", "Cannot create migration copy");
    auto temp = staged.fileName();
    staged.close();
    {
        Database copy(temp);
        backup(old, copy, observer);
        if (observer)
            observer("migration_copy_ready");
    }
    throw domain::Error("IncompatibleSchema",
                        "Verified migration copy staged; no registered prior-version converter "
                        "exists yet. Original retained unchanged.");
}
} // namespace cad::storage

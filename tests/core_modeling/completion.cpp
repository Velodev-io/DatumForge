#include "geometry/occt/SketchCurve.hpp"
#include "persistence/ProjectStore.hpp"
#include "sketch/Curves.hpp"
#include "sketch/Dimensions.hpp"
#include "sketch/Editing.hpp"
#include "sketch/LineEditing.hpp"
#include "sketch/Slots.hpp"
#include "sketch/Solver.hpp"
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <numbers>
using namespace cad;
namespace {
void mark(QString path) {
    QFile file(path + "/.cad-test-root");
    if (!file.open(QIODevice::WriteOnly))
        qFatal("Cannot mark disposable test root");
    file.write("disposable\n");
}
void verifyDefinition(const sketch::Sketch &s) {
    auto input = sketch::encode({s});
    auto request = sketch::fields(
        {{"kind", "FullyDefine"}, {"sketch", s.id}, {"scope", "All"}, {"selection", QCborArray{}}});
    QCborMap response;
    try {
        response = sketch::interact(input, request);
    } catch (const std::exception &error) {
        QFAIL(error.what());
    }
    auto diagnostic = response.value("diagnostics").toArray()[0].toMap();
    QVERIFY2(diagnostic.value("valid").toBool(),
             qPrintable(diagnostic.value("message").toString()));
    QCOMPARE(diagnostic.value("freedom").toInteger(), qint64(0));
    auto result = sketch::parse(response.value("sketches").toArray())[0];
    for (auto &e : s.entities) {
        auto &after = result.entity(e.id);
        QCOMPARE(after.kind, e.kind);
        if (e.kind == "Point")
            QVERIFY(sketch::length(after.point, e.point) < 1e-9);
        else {
            auto range = sketch::curveRange(e), next = sketch::curveRange(after);
            for (int i = 0; i <= 32; ++i)
                QVERIFY(sketch::length(
                            sketch::curveValue(s, e,
                                               range.first + (range.second - range.first) * i / 32),
                            sketch::curveValue(result, after,
                                               next.first + (next.second - next.first) * i / 32)) <
                        1e-9);
        }
    }
    for (auto &c : result.constraints)
        if (c.kind == "Fixed" && std::none_of(s.constraints.begin(), s.constraints.end(),
                                              [&](auto &old) { return old.id == c.id; }))
            QVERIFY(std::none_of(s.entities.begin(), s.entities.end(),
                                 [&](auto &e) { return e.id == c.a; }));
    QCOMPARE(sketch::interact(input, request), response);
}
} // namespace
class Completion : public QObject {
    Q_OBJECT
  private slots:
    void expandedPathBudget() {
        sketch::Sketch s;
        auto center = s.point({0, 0});
        QStringList members;
        double tau = 2 * std::numbers::pi;
        for (int i = 0; i < 32; ++i)
            members << s.arc(center, .01, tau * i / 32, tau / 32);
        for (int i = 0; i < 15; ++i) {
            sketch::Constraint c;
            c.kind = "PathLength";
            c.a = members[0];
            c.members = members;
            c.directions.assign(32, false);
            c.closed = true;
            c.value = tau * .01;
            s.constraints.push_back(c);
        }
        s.validate(); // 975 path equations are within the explicit 1024 budget.
        auto extra = s.constraints.front();
        extra.id = newId();
        s.constraints.push_back(extra);
        bool rejected = false;
        try {
            s.validate();
        } catch (const domain::Error &error) {
            rejected = error.code == "ResourceLimit";
        }
        QVERIFY2(rejected,
                 "Expanded solver equations must be bounded before allocating path constraints");
    }
    void trimPathMemberDisclosure() {
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), c = s.point({.03, .04});
        auto first = s.line(a, b), second = s.line(b, c);
        auto boundary = s.line(s.point({0, .02}), s.point({.05, .02}), true);
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, second};
        path.directions = {false, false};
        path.value = .07;
        s.constraints.push_back(path);
        auto original = s.map();
        auto response =
            sketch::previewLineEdit(s, sketch::fields({{"mode", "Trim to closest"},
                                                       {"source", second},
                                                       {"boundaries", QCborArray{boundary}},
                                                       {"pick_x", .03},
                                                       {"pick_y", .03}}));
        auto disclosure = response.value("affected_constraints").toArray();
        QCOMPARE(disclosure.size(), qsizetype(1));
        QCOMPARE(disclosure[0].toMap().value("id").toString(), path.id);
        auto candidate = sketch::Sketch::parse(response.value("candidate").toMap());
        QVERIFY(candidate.constraints.empty());
        QCOMPARE(s.map(), original);
    }
    void pathProtectsGroup() {
        sketch::Sketch s;
        auto first = s.line(s.point({-.01, 0}), s.point({0, 0}));
        sketch::createRectangle(s, "Corner", {{0, 0}, {.03, .02}});
        auto member = s.groups[0].edges[0];
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, member};
        path.directions = {false, false};
        path.driving = false;
        s.constraints.push_back(path);
        s.validate();
        auto original = s.map();
        bool blocked = false;
        try {
            sketch::eraseEntity(s, member);
        } catch (const domain::Error &error) {
            blocked = error.code == "ConstrainedGeometry";
        }
        QVERIFY2(blocked, "A construction referenced by any path member must require explicit "
                          "relation repair before deletion");
        QCOMPARE(s.map(), original);
    }
    void deletePathMember() {
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), c = s.point({.03, .04});
        auto first = s.line(a, b), second = s.line(b, c);
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, second};
        path.directions = {false, false};
        path.value = .07;
        s.constraints.push_back(path);
        sketch::Sketch result;
        try {
            result = sketch::eraseEntity(s, second);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
        result.validate();
        QVERIFY(result.constraints.empty());
        QCOMPARE(result.entity(first).kind, QString("Line"));
    }
    void ambiguousInferenceContact() {
        sketch::Sketch s;
        auto ellipse = sketch::createEllipse(s, {0, 0}, {.03, 0}, {0, .01});
        s.circle(s.point({0, 0}), .03001);
        auto input = sketch::encode({s});
        auto response = sketch::interact(input, sketch::fields({{"kind", "Infer"},
                                                                {"sketch", s.id},
                                                                {"selection", QCborArray{ellipse}},
                                                                {"tolerance", .0002}}));
        QVERIFY(response.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
        QVERIFY(sketch::parse(response.value("sketches").toArray())[0].constraints.empty());
        QCOMPARE(sketch::encode({s}), input);
    }
    void inferenceCurvePair_data() {
        QTest::addColumn<QString>("kind");
        for (QString kind : {"Circle", "EllipticalArc", "Parabola", "Spline"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void inferenceCurvePair() {
        QFETCH(QString, kind);
        sketch::Sketch s;
        auto ellipse = sketch::createEllipse(s, {0, 0}, {.03, 0}, {0, .01});
        QString circle;
        if (kind == "Circle")
            circle = s.circle(s.point({.04001, 0}), .01);
        else if (kind == "EllipticalArc")
            circle =
                sketch::createEllipse(s, {.06001, 0}, {.09001, 0}, {.06001, .01}, true, 2.5, 1.2);
        else if (kind == "Parabola")
            circle =
                sketch::createParabola(s, {.04001, 0}, {.03001, 0}, {.04001, -.02}, {.04001, .02});
        else
            circle = sketch::createSpline(s, {{.04001, -.02},
                                              {.0266766666666667, -.0066666666666667},
                                              {.0266766666666667, .0066666666666667},
                                              {.04001, .02}});
        auto response = sketch::interact(sketch::encode({s}),
                                         sketch::fields({{"kind", "Infer"},
                                                         {"sketch", s.id},
                                                         {"selection", QCborArray{ellipse}},
                                                         {"tolerance", .0002}}));
        auto proposed = sketch::parse(response.value("sketches").toArray())[0];
        auto contact =
            std::find_if(proposed.constraints.begin(), proposed.constraints.end(), [&](auto &c) {
                return c.kind == "CurveTangent" &&
                       QSet<QString>({c.a, c.b}) == QSet<QString>({ellipse, circle});
            });
        QVERIFY2(
            contact != proposed.constraints.end(),
            "A unique near ellipse/circle tangent must be proposed through exact curve contact");
        QVERIFY(response.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
        QVERIFY(sketch::length(
                    sketch::curveValue(proposed, proposed.entity(contact->a), contact->value),
                    sketch::curveValue(proposed, proposed.entity(contact->b), contact->second)) <
                1e-8);
    }
    void rejectBranchingPath() {
        sketch::Sketch s;
        auto a = s.point({-.01, 0}), b = s.point({0, 0}), c = s.point({.01, 0});
        auto first = s.line(a, b), second = s.line(b, c), third = s.line(c, b);
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, second, third};
        path.directions = {false, false, false};
        path.driving = false;
        s.constraints.push_back(path);
        bool rejected = false;
        try {
            s.validate();
        } catch (const domain::Error &error) {
            rejected = error.code == "ReferenceAmbiguous";
        }
        QVERIFY2(rejected, "A serialized path must reject a branching/revisited junction even when "
                           "its stored traversal is connected");
    }
    void completionPersistence() {
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm3-history-XXXXXX"),
            files(QDir::tempPath() + "/cad-module3-cm3-portable-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        QString interrupted;
        storage::StoreOptions options;
        options.boundary = [&](QString stage) {
            if (stage == interrupted)
                throw domain::Error("TestInterruption", "Disposable CM3 command interruption");
        };
        storage::ProjectStore store(root.path(), options);
        auto project = newId(), document = newId();
        store.create(project);
        auto command = [&](QString kind, QCborMap payload = {}) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = document;
            e.revision = store.revision();
            e.command = kind;
            e.payload = payload;
            return e;
        };
        store.execute(command("CreateDocument", sketch::fields({{"id", document},
                                                                {"type", "Part"},
                                                                {"name", "CM3 persistence"},
                                                                {"units", "in"},
                                                                {"properties_entity", newId()}})));
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), c = s.point({.03, .04});
        auto first = s.line(a, b), second = s.line(b, c);
        s.arc(s.point({.08, .02}), .01, .2, 1.1);
        sketch::createEllipse(s, {.15, .02}, {.18, .02}, {.15, .03}, true, .2, 1.1);
        sketch::createParabola(s, {.01, .1}, {0, .1}, {.01, .08}, {.01, .12});
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, second};
        path.directions = {false, false};
        path.value = .07;
        path.expression = "70 mm";
        path.increment = .001;
        path.drawing = true;
        path.placement = sketch::Vec2{.01, .01};
        s.constraints.push_back(path);
        auto input = sketch::encode({s});
        store.execute(command("SetSketches", sketch::fields({{"sketches", input}})));
        auto baseline = store.sketches(document);
        auto inferred = sketch::interact(baseline, sketch::fields({{"kind", "Infer"},
                                                                   {"sketch", s.id},
                                                                   {"selection", QCborArray{first}},
                                                                   {"tolerance", .0002}}))
                            .value("sketches")
                            .toArray();
        auto defined = sketch::interact(inferred, sketch::fields({{"kind", "FullyDefine"},
                                                                  {"sketch", s.id},
                                                                  {"scope", "All"},
                                                                  {"selection", QCborArray{}}}))
                           .value("sketches")
                           .toArray();
        QCOMPARE(sketch::solve(sketch::parse(defined)[0]).freedom, 0);
        auto accept = command("SetSketches", sketch::fields({{"sketches", defined}}));
        auto old = store.revision();
        store.execute(accept);
        store.execute(accept);
        QCOMPARE(store.revision(), old + 1);
        auto accepted = store.sketches(document);
        auto saved = files.filePath("complete.cadproj");
        store.save(command("SaveProject"), saved);
        store.close();
        store.open(saved, "Saved");
        QCOMPARE(store.sketches(document), accepted);
        store.execute(command("Undo"));
        QCOMPARE(store.sketches(document), baseline);
        store.execute(command("Redo"));
        QCOMPARE(store.sketches(document), accepted);
        store.execute(command("Undo"));
        auto retry = command("SetSketches", sketch::fields({{"sketches", defined}}));
        old = store.revision();
        interrupted = "before_commit";
        QVERIFY_THROWS_EXCEPTION(domain::Error, store.execute(retry));
        QCOMPARE(store.revision(), old);
        QCOMPARE(store.sketches(document), baseline);
        interrupted.clear();
        store.execute(retry);
        QCOMPARE(store.sketches(document), accepted);
        auto edited = sketch::parse(accepted);
        edited[0].constraints[0].increment = .002;
        auto committed =
            command("SetSketches", sketch::fields({{"sketches", sketch::encode(edited)}}));
        old = store.revision();
        interrupted = "after_commit";
        QVERIFY_THROWS_EXCEPTION(domain::Error, store.execute(committed));
        interrupted.clear();
        QVERIFY(store.outcome(committed.operation).value("durable").toBool());
        store.execute(committed);
        QCOMPARE(store.revision(), old + 1);
        accepted = store.sketches(document);
        store.close();
        store.recover(project);
        QCOMPARE(store.sketches(document), accepted);
        auto copy = files.filePath("portable.cadproj"), copiedProject = newId();
        store.save(command("PortableCopy", sketch::fields({{"new_project_id", copiedProject},
                                                           {"policy", "Copied"}})),
                   copy, true);
        store.close();
        QVERIFY(QFile::remove(saved));
        store.open(copy, "Saved");
        QCOMPARE(store.project(), copiedProject);
        QCOMPARE(store.sketches(document), accepted);
    }
    void fullyDefineGroups_data() {
        QTest::addColumn<QString>("kind");
        for (QString kind : {"Spline", "Rectangle", "Slot", "ArcSlot"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void fullyDefineGroups() {
        QFETCH(QString, kind);
        sketch::Sketch s;
        if (kind == "Spline")
            sketch::createSpline(s, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
        else if (kind == "Rectangle")
            sketch::createRectangle(s, "Corner", {{0, 0}, {.03, .02}});
        else if (kind == "Slot")
            sketch::createSlot(s, "Straight", {{0, 0}, {.03, 0}, {.015, .005}}, "Center to Center",
                               true);
        else
            sketch::createSlot(s, "Centerpoint Arc", {{0, 0}, {.03, 0}, {0, .03}, {.033, 0}},
                               "Center to Center", true);
        verifyDefinition(s);
    }
    void fullyDefineParabola() {
        sketch::Sketch s;
        sketch::createParabola(s, {.01, 0}, {0, 0}, {.01, -.02}, {.01, .02});
        verifyDefinition(s);
    }
    void fullyDefineEllipticalArc() {
        sketch::Sketch s;
        sketch::createEllipse(s, {.01, .02}, {.04, .02}, {.01, .03}, true, .3, 1.2);
        verifyDefinition(s);
    }
    void fullyDefineEllipse() {
        sketch::Sketch s;
        sketch::createEllipse(s, {.01, .02}, {.04, .02}, {.01, .03});
        verifyDefinition(s);
    }
    void fullyDefineArc() {
        sketch::Sketch s;
        s.arc(s.point({.01, .02}), .015, .3, 1.2);
        verifyDefinition(s);
    }
    void fullyDefineCircle() {
        sketch::Sketch s;
        s.circle(s.point({.01, .02}), .015);
        verifyDefinition(s);
    }
    void fullyDefineSelected() {
        sketch::Sketch s;
        auto line = s.line(s.point({.01, .02}), s.point({.05, .03}));
        auto center = s.point({.1, .2}), circle = s.circle(center, .015);
        auto input = sketch::encode({s});
        auto request = sketch::fields({{"kind", "FullyDefine"},
                                       {"sketch", s.id},
                                       {"scope", "Selected"},
                                       {"selection", QCborArray{line}}});
        QCborMap response;
        try {
            response = sketch::interact(input, request);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
        auto result = sketch::parse(response.value("sketches").toArray())[0];
        QCOMPARE(response.value("diagnostics").toArray()[0].toMap().value("freedom").toInteger(),
                 qint64(3));
        for (auto &c : result.constraints)
            QVERIFY(c.a != center && c.b != center && c.a != circle && c.b != circle);
        QCOMPARE(result.entity(circle).radius, s.entity(circle).radius);
        QCOMPARE(result.entity(center).point.x, s.entity(center).point.x);
        QCOMPARE(result.entity(center).point.y, s.entity(center).point.y);
        auto drag = sketch::fields({{"kind", "Drag"},
                                    {"sketch", s.id},
                                    {"entity", center},
                                    {"from", QCborArray{.1, .2}},
                                    {"to", QCborArray{.12, .23}}});
        auto moved = sketch::interact(response.value("sketches").toArray(), drag);
        QVERIFY(moved.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
        QVERIFY(
            sketch::length(sketch::parse(moved.value("sketches").toArray())[0].entity(center).point,
                           {.12, .23}) < 1e-8);
    }
    void fullyDefineLine() {
        sketch::Sketch s;
        auto a = s.point({.01, .02}), b = s.point({.05, .02}), line = s.line(a, b);
        sketch::Constraint horizontal, driver;
        horizontal.kind = "Horizontal";
        horizontal.a = line;
        driver.kind = "Length";
        driver.a = line;
        driver.expression = "W";
        s.names.insert("W", "40 mm");
        s.constraints = {horizontal, driver};
        auto original = sketch::encode({s});
        auto request = sketch::fields({{"kind", "FullyDefine"},
                                       {"sketch", s.id},
                                       {"scope", "All"},
                                       {"selection", QCborArray{}}});
        QCborMap response;
        try {
            response = sketch::interact(original, request);
        } catch (const std::exception &error) {
            QFAIL(error.what());
        }
        auto result = sketch::parse(response.value("sketches").toArray())[0];
        auto diagnostics = response.value("diagnostics").toArray()[0].toMap();
        QVERIFY(diagnostics.value("valid").toBool());
        QCOMPARE(diagnostics.value("freedom").toInteger(), qint64(0));
        QCOMPARE(sketch::interact(original, request), response);
        QCOMPARE(sketch::encode({s}), original);
        QCOMPARE(result.constraints[0].id, horizontal.id);
        QCOMPARE(result.constraints[1].id, driver.id);
        QCOMPARE(result.constraints[1].expression, QString("W"));
        QCOMPARE(result.names, s.names);
        for (auto &e : s.entities)
            if (e.kind == "Point")
                QVERIFY(sketch::length(result.entity(e.id).point, e.point) < 1e-9);
        for (auto &c : result.constraints)
            if (c.kind == "Fixed")
                QVERIFY(c.a != a && c.a != b && c.a != line);
        QCOMPARE(sketch::interact(response.value("sketches").toArray(), request).value("sketches"),
                 response.value("sketches"));
    }
    void inferenceCurveTangent_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("endpoint");
        for (QString kind : {"Ellipse", "EllipticalArc", "Parabola", "Spline"})
            for (bool endpoint : {false, true})
                QTest::newRow(qPrintable(kind + (endpoint ? "-endpoint" : "-middle")))
                    << kind << endpoint;
    }
    void inferenceCurveTangent() {
        QFETCH(QString, kind);
        QFETCH(bool, endpoint);
        sketch::Sketch s;
        QString curve;
        if (kind == "Ellipse" || kind == "EllipticalArc")
            curve = sketch::createEllipse(s, {0, 0}, {.03, 0}, {0, .01}, kind == "EllipticalArc",
                                          .2, 1.2);
        else if (kind == "Parabola")
            curve = sketch::createParabola(s, {.01, 0}, {0, 0}, {.01, -.02}, {.01, .02});
        else
            curve = sketch::createSpline(s, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
        auto range = sketch::curveRange(s.entity(curve));
        double parameter = range.first + .37 * (range.second - range.first);
        auto point = sketch::curveValue(s, s.entity(curve), parameter),
             tangent = sketch::curveDerivative(s, s.entity(curve), parameter);
        double size = std::hypot(tangent.x, tangent.y);
        sketch::Vec2 direction{tangent.x / size, tangent.y / size};
        point = {point.x - .00001 * direction.y, point.y + .00001 * direction.x};
        double low = endpoint ? 0 : -.02, high = endpoint ? .04 : .02;
        auto line = s.line(s.point({point.x + low * direction.x, point.y + low * direction.y}),
                           s.point({point.x + high * direction.x, point.y + high * direction.y}));
        auto request = sketch::fields({{"kind", "Infer"},
                                       {"sketch", s.id},
                                       {"selection", QCborArray{line}},
                                       {"tolerance", .0002}});
        auto response = sketch::interact(sketch::encode({s}), request);
        auto proposed = sketch::parse(response.value("sketches").toArray())[0];
        if (!std::any_of(proposed.constraints.begin(), proposed.constraints.end(), [&](auto &c) {
                return c.kind == "CurveTangent" && c.a == line && c.b == curve;
            })) {
            auto probe = s;
            sketch::Constraint contact;
            contact.kind = "CurveTangent";
            contact.a = line;
            contact.b = curve;
            contact.value = endpoint ? 1e-6 : .5;
            contact.second = parameter;
            probe.constraints.push_back(contact);
            auto result = sketch::solve(probe);
            qInfo() << "Direct tangent feasibility" << kind << result.valid << result.message
                    << result.freedom;
            for (auto &c : proposed.constraints)
                qInfo() << "Alternative proposal" << c.kind;
        }
        QVERIFY2(std::any_of(proposed.constraints.begin(), proposed.constraints.end(),
                             [&](auto &c) {
                                 return c.kind == "CurveTangent" && c.a == line && c.b == curve;
                             }),
                 qPrintable(kind));
        QVERIFY(response.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
    }
    void inferenceCurvePoint_data() {
        QTest::addColumn<QString>("kind");
        for (QString kind : {"Ellipse", "EllipticalArc", "Parabola", "Spline"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void inferenceCurvePoint() {
        QFETCH(QString, kind);
        sketch::Sketch s;
        QString curve;
        if (kind == "Ellipse" || kind == "EllipticalArc")
            curve = sketch::createEllipse(s, {0, 0}, {.03, 0}, {0, .01}, kind == "EllipticalArc",
                                          .2, 1.2);
        else if (kind == "Parabola")
            curve = sketch::createParabola(s, {.01, 0}, {0, 0}, {.01, -.02}, {.01, .02});
        else
            curve = sketch::createSpline(s, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
        auto range = sketch::curveRange(s.entity(curve));
        // Use an interior contact distinct from the parabola's explicit vertex;
        // coinciding with that existing point is a different valid proposal.
        double parameter = range.first + .37 * (range.second - range.first);
        auto at = sketch::curveValue(s, s.entity(curve), parameter),
             derivative = sketch::curveDerivative(s, s.entity(curve), parameter);
        double speed = std::hypot(derivative.x, derivative.y);
        auto point =
            s.point({at.x - .00001 * derivative.y / speed, at.y + .00001 * derivative.x / speed});
        auto request = sketch::fields({{"kind", "Infer"},
                                       {"sketch", s.id},
                                       {"selection", QCborArray{point}},
                                       {"tolerance", .0002}});
        auto response = sketch::interact(sketch::encode({s}), request);
        auto proposed = sketch::parse(response.value("sketches").toArray())[0];
        QVERIFY2(std::any_of(proposed.constraints.begin(), proposed.constraints.end(),
                             [&](auto &c) {
                                 return c.kind == "Coincident" && c.a == point && c.b == curve;
                             }),
                 qPrintable(kind));
        QVERIFY(response.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
    }
    void inferenceFamilies_data() {
        QTest::addColumn<QString>("kind");
        for (QString kind : {"Horizontal", "Vertical", "Coincident", "Midpoint", "Intersection",
                             "Perpendicular", "Tangent"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void inferenceFamilies() {
        QFETCH(QString, kind);
        sketch::Sketch s;
        QString selected;
        if (kind == "Horizontal" || kind == "Vertical")
            selected =
                s.line(s.point({0, 0}), s.point(kind == "Horizontal" ? sketch::Vec2{.03, .00005}
                                                                     : sketch::Vec2{.00005, .03}));
        else if (kind == "Coincident") {
            selected = s.point({.00005, 0});
            s.point({0, 0});
        } else if (kind == "Midpoint") {
            selected = s.point({.015, .00005});
            s.line(s.point({0, 0}), s.point({.03, 0}));
        } else if (kind == "Intersection") {
            selected = s.point({.00005, .00005});
            s.line(s.point({-.03, 0}), s.point({.03, 0}));
            s.line(s.point({0, -.03}), s.point({0, .03}));
        } else if (kind == "Perpendicular") {
            selected = s.line(s.point({0, 0}), s.point({.03, 0}));
            s.line(s.point({.04, 0}), s.point({.04005, .03}));
        } else {
            selected = s.line(s.point({-.03, .01005}), s.point({.03, .01005}));
            s.circle(s.point({0, 0}), .01);
        }
        auto original = s.map();
        auto request = sketch::fields({{"kind", "Infer"},
                                       {"sketch", s.id},
                                       {"selection", QCborArray{selected}},
                                       {"tolerance", .0005}});
        auto response = sketch::interact(sketch::encode({s}), request);
        auto proposed = sketch::parse(response.value("sketches").toArray())[0];
        QVERIFY2(std::any_of(proposed.constraints.begin(), proposed.constraints.end(),
                             [&](auto &c) { return c.kind == kind; }),
                 qPrintable("Missing " + kind + " proposal"));
        QCOMPARE(s.map(), original);
        QVERIFY(response.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
    }
    void inference() {
        sketch::Sketch s;
        auto line = s.line(s.point({0, 0}), s.point({.03, .00005}));
        auto original = s.map();
        auto request = sketch::fields({{"kind", "Infer"},
                                       {"sketch", s.id},
                                       {"selection", QCborArray{line}},
                                       {"tolerance", .0005}});
        QCborMap response;
        try {
            response = sketch::interact(sketch::encode({s}), request);
        } catch (const std::exception &error) {
            QFAIL(qPrintable(QString("Inference proposal must be supported: ") + error.what()));
        }
        auto proposed = sketch::parse(response.value("sketches").toArray())[0];
        QVERIFY(std::any_of(proposed.constraints.begin(), proposed.constraints.end(),
                            [&](auto &c) { return c.kind == "Horizontal" && c.a == line; }));
        QCOMPARE(s.map(), original);
        auto repeat = sketch::interact(sketch::encode({s}), request);
        QCOMPARE(repeat.value("sketches"), response.value("sketches"));
        QVERIFY(response.value("diagnostics").toArray()[0].toMap().value("valid").toBool());
    }
    void closedPath() {
        sketch::Sketch s;
        auto center = s.point({0, 0});
        auto first = s.arc(center, .01, 0, std::numbers::pi);
        auto second = s.arc(center, .01, std::numbers::pi, std::numbers::pi);
        for (auto id : {first, second}) {
            sketch::Constraint radius;
            radius.kind = "Radius";
            radius.a = id;
            radius.value = .01;
            s.constraints.push_back(radius);
        }
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = first;
        path.members = {first, second};
        path.directions = {false, false};
        path.value = .05;
        s.constraints.push_back(path);
        auto map = s.map();
        auto records = map.value("constraints").toArray();
        auto record = records.last().toMap();
        record.insert(QStringLiteral("closed"), true);
        records[records.size() - 1] = record;
        map.insert(QStringLiteral("constraints"), records);
        sketch::Sketch parsed;
        try {
            parsed = sketch::Sketch::parse(map);
        } catch (const std::exception &error) {
            QFAIL(qPrintable(QString("Closed path association must be retained: ") + error.what()));
        }
        // A closed pair of positive circular arcs with fixed equal radii cannot
        // shorten its full circumference by quietly opening the last join.
        QVERIFY(!sketch::solve(parsed).valid);
        QCOMPARE(parsed.map(), map);
    }
    void pathCurves_data() {
        QTest::addColumn<QString>("kind");
        for (QString kind : {"Circle", "Ellipse", "EllipticalArc", "Parabola", "Spline"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void pathCurves() {
        QFETCH(QString, kind);
        sketch::Sketch s;
        QString id;
        if (kind == "Circle")
            id = s.circle(s.point({0, 0}), .01);
        else if (kind == "Ellipse" || kind == "EllipticalArc")
            id = sketch::createEllipse(s, {0, 0}, {.03, 0}, {0, .01}, kind == "EllipticalArc", .2,
                                       1.2);
        else if (kind == "Parabola")
            id = sketch::createParabola(s, {.01, 0}, {0, 0}, {.01, -.02}, {.01, .02});
        else
            id = sketch::createSpline(s, {{0, 0}, {.01, .03}, {.03, -.01}, {.05, .01}});
        auto controls = sketch::controlPoints(s.entity(id));
        if (kind == "Spline")
            controls = {controls[0]};
        for (auto &point : controls) {
            sketch::Constraint fixed;
            fixed.kind = "Fixed";
            fixed.a = point;
            fixed.value = s.entity(point).point.x;
            fixed.second = s.entity(point).point.y;
            s.constraints.push_back(fixed);
        }
        auto length = [&](const sketch::Sketch &sketch) {
            auto exact = geometry::sketchCurve(sketch, sketch.entity(id));
            GeomAdaptor_Curve adaptor(exact.curve, exact.first, exact.last);
            return GCPnts_AbscissaPoint::Length(adaptor, exact.first, exact.last, 1e-9) / 1000;
        };
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = id;
        path.members = {id};
        path.directions = {false};
        path.value = length(s) * 1.1;
        s.constraints.push_back(path);
        auto result = sketch::solve(s);
        QVERIFY2(result.valid, qPrintable(result.message));
        QVERIFY2(std::abs(length(result.sketch) - path.value) < 1e-9, qPrintable(kind));
        QCOMPARE(sketch::Sketch::parse(result.sketch.map()).map(), result.sketch.map());
    }
    void pathArc() {
        sketch::Sketch s;
        auto origin = s.point({0, 0}), corner = s.point({.03, 0}), center = s.point({.03, .01});
        auto line = s.line(origin, corner);
        auto arc = s.arc(center, .01, -std::numbers::pi / 2, std::numbers::pi / 2);
        auto add = [&](QString kind, QString id, double value = 0, double second = 0) {
            sketch::Constraint c;
            c.kind = kind;
            c.a = id;
            c.value = value;
            c.second = second;
            s.constraints.push_back(c);
        };
        add("Fixed", origin);
        add("Fixed", center, .03, .01);
        add("Horizontal", line);
        add("Length", line, .03);
        add("Radius", arc, .01);
        sketch::Constraint path;
        path.kind = "PathLength";
        path.a = line;
        path.members = {line, arc};
        path.directions = {false, false};
        path.value = .03 + .01 * std::numbers::pi;
        s.constraints.push_back(path);
        auto result = sketch::solve(s);
        QVERIFY2(result.valid, qPrintable(result.message));
        QVERIFY(std::abs(result.sketch.entity(arc).sweep - std::numbers::pi) < 1e-8);
        double independent = 0;
        for (auto &id : path.members) {
            auto exact = geometry::sketchCurve(result.sketch, result.sketch.entity(id));
            GeomAdaptor_Curve adaptor(exact.curve, exact.first, exact.last);
            independent +=
                GCPnts_AbscissaPoint::Length(adaptor, exact.first, exact.last, 1e-8) / 1000;
        }
        QVERIFY(std::abs(independent - path.value) < 1e-10);
        QVERIFY(result.maximumResidual <= 1e-7);
    }
    void pathLength() {
        sketch::Sketch s;
        auto a = s.point({0, 0}), b = s.point({.03, 0}), c = s.point({.03, .04});
        auto first = s.line(a, b), second = s.line(b, c);
        auto add = [&](QString kind, QString operand, double value = 0) {
            sketch::Constraint constraint;
            constraint.kind = kind;
            constraint.a = operand;
            constraint.value = value;
            constraint.placement =
                sketch::dimensional(kind) ? std::optional(sketch::Vec2{.01, .01}) : std::nullopt;
            s.constraints.push_back(constraint);
        };
        add("Fixed", a);
        add("Horizontal", first);
        add("Vertical", second);
        add("Length", first, .03);
        add("PathLength", first, .1);
        auto identity = s.constraints.back().id;
        auto map = s.map();
        map.insert(QStringLiteral("version"), 8);
        auto records = map.value("constraints").toArray();
        auto record = records.last().toMap();
        record.insert(QStringLiteral("members"), QCborArray{first, second});
        record.insert(QStringLiteral("directions"), QCborArray{false, false});
        record.insert(QStringLiteral("driving"), true);
        records[records.size() - 1] = record;
        map.insert(QStringLiteral("constraints"), records);
        sketch::Sketch parsed;
        try {
            parsed = sketch::Sketch::parse(map);
        } catch (const std::exception &error) {
            QFAIL(qPrintable(QString("Connected driving path must be accepted: ") + error.what()));
        }
        auto result = sketch::solve(parsed);
        QVERIFY2(result.valid, qPrintable(result.message));
        auto p = result.sketch.entity(a).point, q = result.sketch.entity(b).point,
             r = result.sketch.entity(c).point;
        QVERIFY(std::abs(sketch::length(p, q) + sketch::length(q, r) - .1) < 1e-10);
        QVERIFY(std::abs(r.y - .07) < 1e-10);
        QCOMPARE(result.sketch.constraints.back().id, identity);
        auto roundtrip = sketch::Sketch::parse(result.sketch.map());
        QCOMPARE(roundtrip.map(), result.sketch.map());
        QVERIFY(sketch::solve(roundtrip).valid);
    }
    void dimensionMetadata() {
        sketch::Sketch s;
        auto line = s.line(s.point({0, 0}), s.point({.03, .04}));
        sketch::Constraint dimension;
        dimension.kind = "Length";
        dimension.a = line;
        dimension.value = .05;
        dimension.expression = "50 mm";
        s.constraints.push_back(dimension);
        auto map = s.map();
        map.insert(QStringLiteral("version"), 8);
        map.insert(QStringLiteral("groups"), QCborArray{});
        map.insert(QStringLiteral("edits"), QCborArray{});
        auto dimensions = map.value("constraints").toArray();
        auto record = dimensions[0].toMap();
        record.insert(QStringLiteral("driving"), true);
        record.insert(QStringLiteral("increment"), .001);
        record.insert(QStringLiteral("drawing"), true);
        dimensions[0] = record;
        map.insert(QStringLiteral("constraints"), dimensions);
        auto parsed = sketch::Sketch::parse(map);
        QCOMPARE(parsed.map(), map);
        auto result = sketch::solve(parsed);
        QVERIFY2(result.valid, qPrintable(result.message));
        QCOMPARE(result.sketch.entity(line).a, s.entity(line).a);
        QCOMPARE(result.sketch.constraints[0].expression, QString("50 mm"));
        QTemporaryDir root(QDir::tempPath() + "/cad-module3-cm3-complete-XXXXXX");
        QTemporaryDir files(QDir::tempPath() + "/cad-module3-cm3-saved-XXXXXX");
        QVERIFY(root.isValid() && files.isValid());
        mark(root.path());
        mark(files.path());
        storage::ProjectStore store(root.path());
        auto project = newId(), document = newId();
        store.create(project);
        auto command = [&](QString kind, QCborMap payload = {}) {
            wire::Envelope e;
            e.context.project = project;
            e.context.document = document;
            e.revision = store.revision();
            e.command = kind;
            e.payload = payload;
            return e;
        };
        store.execute(command("CreateDocument", sketch::fields({{"id", document},
                                                                {"type", "Part"},
                                                                {"name", "CM3 metadata"},
                                                                {"units", "mm"},
                                                                {"properties_entity", newId()}})));
        store.execute(
            command("SetSketches", sketch::fields({{"sketches", sketch::encode({parsed})}})));
        auto accepted = store.sketches(document);
        auto file = files.filePath("dimension-metadata.cadproj");
        store.save(command("SaveProject"), file);
        store.close();
        store.open(file, "Saved");
        QCOMPARE(store.sketches(document), accepted);
        QCOMPARE(accepted[0]
                     .toMap()
                     .value("constraints")
                     .toArray()[0]
                     .toMap()
                     .value("increment")
                     .toDouble(),
                 .001);
        QVERIFY(accepted[0]
                    .toMap()
                    .value("constraints")
                    .toArray()[0]
                    .toMap()
                    .value("drawing")
                    .toBool());
        store.execute(command("Undo"));
        store.execute(command("Redo"));
        QCOMPARE(store.sketches(document), accepted);
    }
};
QTEST_GUILESS_MAIN(Completion)
#include "completion.moc"

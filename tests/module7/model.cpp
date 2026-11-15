#include "assembly/Model.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <numbers>
using namespace cad;
using namespace cad::assembly;
Reference ref(QString id, QString token = "Origin") {
    Reference r;
    r.occurrence = id;
    r.token = token;
    return r;
}
Frame frame(const Reference &r) {
    Frame f;
    if (r.token != "Origin") {
        f.kind = r.token.endsWith("Axis") ? "Axis" : "Plane";
        if (r.token == "X Axis")
            f.direction = {1, 0, 0};
        else if (r.token == "Y Axis")
            f.direction = {0, 1, 0};
    }
    return f;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        Model m;
        Occurrence a, b, c;
        a.definition = b.definition = c.definition = newId();
        a.grounded = true;
        b.pose.translation = {.02, .03, .005};
        c.pose.translation = {.04, .05, .009};
        m.occurrences = {a, b, c};
        auto mate = [&](QString kind, QString first, QString second, QString token,
                        double value = 0) {
            Mate q;
            q.kind = kind;
            q.first = ref(first, token);
            q.second = ref(second, token);
            q.value = value;
            return q;
        };
        m.mates = {mate("Coincident", a.id, b.id, "XY Plane"),
                   mate("Distance", b.id, c.id, "XY Plane", .01),
                   mate("Parallel", b.id, c.id, "XY Plane")};
        auto plane = solve(m, frame);
        CHECK(plane.valid);
        CHECK(plane.freedom == 6);
        m.mates.push_back(mate("Concentric", a.id, b.id, "Z Axis"));
        m.mates.push_back(mate("Concentric", b.id, c.id, "Z Axis"));
        auto axial = solve(m, frame);
        CHECK(axial.valid);
        CHECK(axial.freedom == 2);
        m.mates.push_back(mate("Angle", a.id, b.id, "X Axis", 0));
        m.mates.push_back(mate("Angle", b.id, c.id, "X Axis", 0));
        auto fixed = solve(m, frame);
        CHECK(fixed.valid);
        CHECK(fixed.freedom == 0);
        near(fixed.model.occurrences[1].pose.translation[2], 0, 1e-9);
        near(fixed.model.occurrences[2].pose.translation[2], .01, 1e-9);
        auto contradiction = mate("Distance", a.id, c.id, "XY Plane", .011);
        m.mates.push_back(contradiction);
        auto bad = solve(m, frame);
        CHECK(!bad.valid);
        CHECK(bad.model.map() == m.map());
        CHECK(!bad.diagnostics.empty());
        m.mates.pop_back();
        auto repeated = solve(m, frame);
        CHECK(repeated.valid && repeated.model.map() == fixed.model.map());
        m.occurrences[1].pose.rotation = {0, 1, 0, 0};
        CHECK(solve(m, frame).valid);
        Model perpendicular;
        perpendicular.occurrences = {a, b};
        perpendicular.mates = {mate("Perpendicular", a.id, b.id, "Z Axis")};
        auto ninety = solve(perpendicular, frame);
        CHECK(ninety.valid);
        CHECK(ninety.freedom == 5);
        Model distance;
        distance.occurrences = {a, a};
        distance.occurrences[1].id = newId();
        distance.occurrences[1].grounded = false;
        distance.mates = {mate("Distance", a.id, distance.occurrences[1].id, "Origin", .1)};
        auto separated = solve(distance, frame);
        CHECK(separated.valid);
        CHECK(separated.freedom == 5);
        distance.mates[0].second.occurrence = newId();
        CHECK(!solve(distance, frame).valid);
        auto round = Model::parse(m.map());
        CHECK(round.map() == m.map());
        auto invalid = b.pose.map();
        invalid.insert(QStringLiteral("rotation_wxyz"), QCborArray{2., 0., 0., 0.});
        rejects([&] { Pose::parse(invalid); });
        Definitions defs;
        auto root = newId(), sub = newId();
        Model nested;
        Occurrence child;
        child.definition = sub;
        nested.occurrences.push_back(child);
        defs[root] = {"Assembly", nested};
        defs[sub] = {"Assembly", {}};
        validateGraph(defs);
        child.definition = root;
        defs[sub].second.occurrences.push_back(child);
        rejects([&] { validateGraph(defs); });
        Definitions large;
        auto leaf = newId(), branch = newId(), parent = newId();
        large[leaf] = {"Part", {}};
        Model hundred;
        for (int i = 0; i < 100; ++i) {
            Occurrence o;
            o.definition = leaf;
            hundred.occurrences.push_back(o);
        }
        large[branch] = {"Assembly", hundred};
        Model duplicate;
        for (int i = 0; i < 2; ++i) {
            Occurrence o;
            o.definition = branch;
            duplicate.occurrences.push_back(o);
        }
        large[parent] = {"Assembly", duplicate};
        rejects([&] { validateGraph(large); });
        std::cout << "Rigid assembly planar/axial/angle 6/2/0 DOF; perpendicular/distance; "
                     "conflict, broken occurrence, deterministic accepted transforms, antiparallel "
                     "and cycle corpus passed\n";
    });
}

#include "sketch/Sketch.hpp"
#include <QCoreApplication>
#include <cmath>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    cad::sketch::Sketch s;
    auto centre = s.point({0, 0});
    QStringList members;
    double tau = 2 * std::acos(-1.);
    for (int i = 0; i < 32; ++i) members << s.arc(centre, .01, tau * i / 32, tau / 32);
    for (int i = 0; i < 16; ++i) {
        cad::sketch::Constraint path;
        path.kind = "PathLength"; path.a = members[0]; path.members = members;
        path.directions.assign(32, false); path.closed = true; path.value = tau * .01;
        s.constraints.push_back(path);
        if (i == 14) { s.validate(); std::cout << "15 paths: 975 expanded equations admitted\n"; }
    }
    try { s.validate(); }
    catch (const cad::domain::Error &error) {
        if (error.code == "ResourceLimit") { std::cout << "PASS: expanded equation budget rejected\n"; return 0; }
        std::cerr << error.what() << '\n'; return 2;
    }
    std::cerr << "FAIL: 16 paths (1040 expanded equations) admitted without a solver-object budget\n";
    return 1;
}

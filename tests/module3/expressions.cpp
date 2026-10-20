#include "domain/Expressions.hpp"
#include "support/check.hpp"
using namespace cad::expression;
int main() {
    return run([] {
        Environment e;
        auto q = e.evaluate("1 in + 25.4 mm");
        CHECK(q.dimension == length);
        near(q.value, .0508, 1e-14);
        near(e.evaluate("180 deg").value, e.evaluate("pi * rad").value, 1e-14);
        near(e.evaluate("sin(90 deg)").value, 1, 1e-14);
        near(e.evaluate("asin(1)").value, 1.5707963267948966, 1e-14);
        near(e.evaluate("sqrt((3 mm)^2 + (4 mm)^2)").value, .005, 1e-14);
        near(e.evaluate("2*kg + 500 g").value, 2.5, 1e-14);
        CHECK(format({.0254, length}, "in") == "1 in");
        CHECK(format({3.141592653589793, angle}, "deg") == "180 deg");
        Environment named({{"Width", "2 * Base"}, {"Base", "40 mm"}, {"Ratio", "Width / Base"}});
        named.validateAll();
        near(named.name("Width").value, .08, 1e-14);
        near(named.name("Ratio").value, 2, 1e-14);
        for (QString text : {"1 mm + 2 deg", "1 / 0", "sqrt(-1)", "sqrt(1 mm)", "sin(1 mm)",
                             "asin(2)", "tan(90 deg)", "1e309 mm", "NaN", "inf", "x.__class__",
                             "system(1)", "1 mm; quit()", "(1+2", "pi rad", "2^0.5", "0^-1"})
            rejects([&] { e.evaluate(text); });
        rejects([&] {
            Environment cycle({{"A", "B+1"}, {"B", "A+1"}});
            cycle.validateAll();
        });
        rejects([&] {
            Environment reserved({{"mm", "2"}});
            reserved.validateAll();
        });
        rejects([&] { e.evaluate(QString(4097, '1')); });
        rejects([&] { e.evaluate(QString(33, '(') + "1" + QString(33, ')')); });
        QString ast = "1";
        for (int i = 0; i < 256; ++i)
            ast += "+1";
        rejects([&] { e.evaluate(ast); });
        near(e.evaluate("-2^2").value, -4, 1e-14);
        near(e.evaluate("max(1 in, 20 mm)").value, .0254, 1e-14);
        std::cout << "3.3a: SI/display conversions, typed arithmetic/functions/names, cycles, "
                     "domain/nonfinite and 4 KiB/256-node/32-depth limits\n";
    });
}

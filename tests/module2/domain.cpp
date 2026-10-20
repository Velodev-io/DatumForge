#include "domain/Documents.hpp"
#include "support/check.hpp"
int main() {
    return run([] {
        using namespace cad;
        using namespace domain;
        CHECK(std::abs(toSI(1, "length", "in") - .0254) < 1e-15);
        CHECK(std::abs(toSI(180, "angle", "deg") - 3.141592653589793) < 1e-14);
        for (auto k : {"Part", "Assembly", "Drawing"})
            CHECK(kind(k) == k);
        QCborMap p;
        p.insert(QStringLiteral("name"), "Thickness");
        p.insert(QStringLiteral("dimension"), "length");
        p.insert(QStringLiteral("value_si"), .005);
        CHECK(Parameter::parse(p).value == .005);
        int refused = 0;
        for (int fault = 0; fault < 6; ++fault)
            try {
                if (fault == 0)
                    toSI(1, "length", "deg");
                if (fault == 1)
                    toSI(NAN, "length", "m");
                if (fault == 2)
                    kind("Sketch");
                if (fault == 3)
                    label("");
                if (fault == 4) {
                    auto bad = p;
                    bad.insert(QStringLiteral("name"), "arbitrary();");
                    Parameter::parse(bad);
                }
                if (fault == 5)
                    units("feet");
            } catch (const std::exception &) {
                ++refused;
            }
        CHECK(refused == 6);
        wire::Envelope e;
        e.context.project = newId();
        e.command = "SetParameter";
        e.payload = p;
        auto hash = requestHash(e);
        e.epoch = newId();
        e.request = newId();
        e.context.session = newId();
        e.generation++;
        CHECK(hash == requestHash(e));
        e.revision++;
        CHECK(hash != requestHash(e));
    });
}

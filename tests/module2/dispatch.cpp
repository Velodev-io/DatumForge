#include "Projects.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
using namespace cad;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir directory(QDir::tempPath() + "/cad-module2-dispatch-XXXXXX");
        CHECK(directory.isValid());
        QFile marker(directory.filePath(".cad-test-root"));
        CHECK(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
        marker.close();
        worker::Projects session(directory.path());
        wire::Envelope e;
        e.epoch = newId();
        e.command = "NewProject";
        e.payload.insert(QStringLiteral("id"), e.context.document);
        e.payload.insert(QStringLiteral("properties_entity"), newId());
        e.payload.insert(QStringLiteral("type"), "Part");
        e.payload.insert(QStringLiteral("name"), "Dispatch fixture");
        e.payload.insert(QStringLiteral("units"), "mm");
        e.payload.insert(QStringLiteral("replace"), false);
        auto first = session.run(e);
        CHECK(first.value("project_state").toMap().value("revision").toInteger() == 1);
        e.epoch = newId();
        e.context.session = newId();
        e.request = newId();
        auto replay = session.run(e);
        CHECK(replay.value("project_state").toMap().value("revision").toInteger() == 1);
        int rejected = 0;
        try {
            auto changed = e;
            changed.payload.insert(QStringLiteral("name"), "Other content");
            session.run(changed);
        } catch (const domain::Error &) {
            ++rejected;
        }
        try {
            auto replacing = e;
            replacing.context = Context();
            replacing.operation = newId();
            replacing.payload.insert(QStringLiteral("id"), replacing.context.document);
            session.run(replacing);
        } catch (const domain::Error &) {
            ++rejected;
        }
        CHECK(rejected == 2);
    });
}

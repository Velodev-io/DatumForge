#include "diagnostics/Journal.hpp"
#include "domain/Expressions.hpp"
#include "drawing/Model.hpp"
#include "drawing/Paths.hpp"
#include "support/check.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
using namespace cad;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return run([] {
        QTemporaryDir root(QDir::tempPath() + "/cad-module9-privacy-XXXXXX");
        CHECK(root.isValid());
        QFile marker(root.filePath(".cad-test-root"));
        CHECK(marker.open(QIODevice::WriteOnly));
        marker.write("disposable\n");
        diagnostics::Journal log(root.path());
        for (int i = 0; i < 5000; ++i)
            log.record("/private/secret/model text", "password=secret", 1, 0);
        qint64 sum = 0;
        for (auto path : QDir(root.path()).entryList({"jobs-*.jsonl"}, QDir::Files)) {
            auto size = QFileInfo(root.filePath(path)).size();
            CHECK(size <= diagnostics::Journal::fileCap);
            sum += size;
        }
        CHECK(sum <= diagnostics::Journal::files * diagnostics::Journal::fileCap);
        auto report = QJsonDocument(log.report()).toJson();
        CHECK(!report.contains("/private") && !report.contains("secret") &&
              !report.contains("password"));
        QFile poisoned(root.filePath("jobs-0.jsonl"));
        CHECK(poisoned.open(QIODevice::WriteOnly | QIODevice::Append));
        poisoned.write("{\"command\":\"SaveProject\",\"status\":\"Succeeded\",\"raw_model\":"
                       "\"SECRET_MODEL_TEXT\"}\n");
        poisoned.close();
        CHECK(!QJsonDocument(log.report()).toJson().contains("SECRET_MODEL_TEXT"));
        log.exportTo(root.filePath("diagnostics.json"));
        QByteArray bad;
        drawing::appendPoint(bad, std::numeric_limits<double>::quiet_NaN(), 0);
        drawing::appendPoint(bad, 0, 0);
        rejects([&] { drawing::points(bad); });
        rejects([&] { drawing::points(QByteArray(7, 'x')); });
        drawing::Model m;
        drawing::Sheet s;
        drawing::Annotation a;
        a.text = QString(513, 'x');
        s.annotations.push_back(a);
        m.sheets.push_back(s);
        rejects([&] { drawing::Model::parse(m.map()); });
        std::cout << "Rotating 256 KiB journal, default export redaction including poisoned extra "
                     "fields, oversized text and malformed vector rejection passed\n";
    });
}

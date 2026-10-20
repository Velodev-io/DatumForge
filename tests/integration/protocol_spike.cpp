#include "support/admission.hpp"
#include "support/check.hpp"
#include "support/protocol.hpp"
int main() {
    return run([] {
        auto good = wire::request("epoch");
        auto framed = wire::frame(good);
        wire::Frames parser;
        CHECK(parser.feed(framed.left(2)).empty());
        auto decoded = parser.feed(framed.mid(2));
        CHECK(decoded.size() == 1);
        wire::validate(decoded[0], "epoch");
        rejects([&] { wire::validate(good, "old"); });
        auto bad = good;
        bad.insert(QStringLiteral("protocol_version"), 2);
        rejects([&] { wire::validate(bad, "epoch"); });
        bad = good;
        bad.insert(QStringLiteral("command_type"), "execute-shell");
        rejects([&] { wire::validate(bad, "epoch"); });
        bad = good;
        bad.insert(QStringLiteral("preview_generation"), -1);
        rejects([&] { wire::validate(bad, "epoch"); });
        rejects([] { wire::Frames().feed(QByteArray::fromHex("ffffffff")); });
        rejects([] { wire::decode(QByteArray::fromHex("a16178")); });
        rejects([] { wire::decode(QByteArray::fromHex("a2617801617802")); });
        rejects([] { wire::decode(QByteArray::fromHex("9a000186a1")); });
        rejects([] { wire::decode(QByteArray::fromHex("a16178fb7ff0000000000000")); });
        rejects([] { wire::decode(QByteArray(34, char(0x81)) + char(0)); });
        rejects([] { wire::decode(QByteArray::fromHex("a161787a00004001")); });
        wire::Session session{"new", 5, 9};
        CHECK(!session.accept("old", 9, 10));
        CHECK(!session.accept("new", 4, 10));
        CHECK(!session.accept("new", 6, 8));
        CHECK(session.accept("new", 6, 10));
        wire::Buffers buffers;
        buffers.add("opaque", QByteArray(32, 'x'));
        CHECK(buffers.bytes() == 32);
        rejects([&] { buffers.add("/tmp/arbitrary", QByteArray()); });
        CHECK(!wire::Buffers::range(UINT64_MAX, 10, 128));
        CHECK(!wire::Buffers::range(100, UINT64_MAX, 128));
        buffers.restart();
        CHECK(buffers.bytes() == 0);
        wire::Chunks chunks;
        QByteArray bulk(2 * 1024 * 1024, 'x');
        chunks.begin("epoch", bulk.size(),
                     QCryptographicHash::hash(bulk, QCryptographicHash::Sha256));
        rejects([&] { chunks.append("old", 0, bulk.left(1)); });
        rejects([&] { chunks.append("epoch", 1, bulk.left(1)); });
        rejects([&] { chunks.finish(); });
        for (qsizetype offset = 0; offset < bulk.size(); offset += 8192)
            chunks.append("epoch", offset, bulk.mid(offset, 8192));
        CHECK(chunks.complete());
        CHECK(chunks.finish() == bulk);
        chunks.begin("old", 32, QByteArray(32, 'x'));
        chunks.append("old", 0, QByteArray(8, 'x'));
        chunks.abandon();
        rejects([&] { chunks.append("old", 8, QByteArray(8, 'x')); });
        wire::Mesh mesh;
        float xyz[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        quint32 indices[] = {0, 1, 2};
        mesh.vertices = QByteArray(reinterpret_cast<char *>(xyz), sizeof xyz);
        mesh.indices = QByteArray(reinterpret_cast<char *>(indices), sizeof indices);
        mesh.checksum =
            QCryptographicHash::hash(mesh.vertices + mesh.indices, QCryptographicHash::Sha256);
        mesh.check();
        indices[2] = 999;
        mesh.indices = QByteArray(reinterpret_cast<char *>(indices), sizeof indices);
        mesh.checksum =
            QCryptographicHash::hash(mesh.vertices + mesh.indices, QCryptographicHash::Sha256);
        rejects([&] { mesh.check(); });
        float badEdge[6]{0, 0, 0, 1, 1, INFINITY};
        rejects([&] {
            wire::validateEdges(QByteArray(reinterpret_cast<char *>(badEdge), sizeof badEdge));
        });
        CHECK(admission::decide(1, 1024U << 20, 0, admission::Pressure::Normal) ==
              admission::Response::Run);
        CHECK(admission::decide(1, 1, 0, admission::Pressure::Normal) ==
              admission::Response::PostponeHeavyJob);
        CHECK(admission::decide(1, 1024U << 20, 1, admission::Pressure::Normal) ==
              admission::Response::PostponeHeavyJob);
        CHECK(admission::decide(1, 1024U << 20, 0, admission::Pressure::Elevated) ==
              admission::Response::StopPreviewsAndEvictDerived);
        CHECK(admission::decide(1, 1024U << 20, 0, admission::Pressure::Critical) ==
              admission::Response::PostponeHeavyJob);
        rejects([] { admission::decide(SIZE_MAX, SIZE_MAX, 0, admission::Pressure::Normal); });
        std::array<admission::CacheEntry, 3> cache{
            {{true, false, 100}, {false, true, 50}, {false, false, 25}}};
        CHECK(admission::trim(cache) == 25);
        CHECK(cache[0].bytes == 100 && cache[1].bytes == 50);
        std::cout << "bounded CBOR/framing, stale epoch/revision/generation, buffer cleanup, "
                     "range/checksum/index corpus\n";
    });
}

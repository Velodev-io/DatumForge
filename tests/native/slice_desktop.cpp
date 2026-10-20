#include "support/check.hpp"
#include "support/protocol.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMainWindow>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStatusBar>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>
#include <memory>
#ifdef Q_OS_MACOS
#include <libproc.h>
#include <sys/resource.h>
#endif
void memorySample(const char *stage, qint64 worker = 0) {
#ifdef Q_OS_MACOS
    for (auto pid : {QCoreApplication::applicationPid(), worker})
        if (pid) {
            rusage_info_v2 usage{};
            if (proc_pid_rusage(int(pid), RUSAGE_INFO_V2,
                                reinterpret_cast<rusage_info_t *>(&usage)) == 0)
                std::cout << "memory stage=" << stage << " pid=" << pid
                          << " footprint_bytes=" << usage.ri_phys_footprint
                          << " resident_bytes=" << usage.ri_resident_size << std::endl;
            else
                std::cout << "memory stage=" << stage << " pid=" << pid
                          << " Blocked=proc_pid_rusage" << std::endl;
        }
#else
    (void)stage;
    (void)worker;
#endif
}
class Transport {
    QTemporaryDir root;
    QLocalServer server;
    QProcess process;
    QLocalSocket *socket = nullptr;
    wire::Frames parser;
    QString epoch;
    QByteArray secret;
    bool authenticated = false;
    bool stopping = false;
    wire::Chunks chunks;
    qsizetype vertexBytes = 0;

  public:
    wire::Mesh mesh;
    QList<int> faces;
    QByteArray edges;
    QString error;
    double volume = 0;
    std::function<void()> updated;
    Transport() {
        CHECK(root.isValid());
        QObject::connect(&process, &QProcess::finished, [this](int, QProcess::ExitStatus) {
            if (!stopping) {
                error = "Worker unavailable; displayed result is stale. Restart worker to rebuild "
                        "the box.";
                if (updated)
                    updated();
            }
        });
        QObject::connect(&server, &QLocalServer::newConnection, [this] {
            socket = server.nextPendingConnection();
            socket->setReadBufferSize(limits::control + 4);
            QObject::connect(socket, &QLocalSocket::readyRead, [this] {
                try {
                    while (socket->bytesAvailable())
                        for (const auto &reply : parser.feed(socket->read(16384))) {
                            if (!authenticated) {
                                wire::require(reply.value("capability").toByteArray() == secret &&
                                                  reply.value("epoch").toString() == epoch &&
                                                  reply.value("version").toInteger() == 1,
                                              "handshake");
                                authenticated = true;
                                socket->write(wire::frame(wire::request(epoch)));
                                continue;
                            }
                            wire::require(reply.value("worker_epoch").toString() == epoch,
                                          "stale worker");
                            if (reply.contains(QStringLiteral("chunk"))) {
                                chunks.append(epoch,
                                              quint64(reply.value("chunk_offset").toInteger()),
                                              reply.value("chunk").toByteArray());
                                if (chunks.complete()) {
                                    auto bulk = chunks.finish();
                                    mesh.vertices = bulk.left(vertexBytes);
                                    mesh.indices = bulk.mid(vertexBytes);
                                    mesh.check();
                                    wire::require(faces.size() == mesh.indices.size() / 12,
                                                  "face ID mapping");
                                    wire::validateEdges(edges);
                                    if (updated)
                                        updated();
                                }
                                continue;
                            }
                            wire::require(reply.value("preview_generation").toInteger() == 1 &&
                                              reply.value("revision").toInteger() == 1,
                                          "stale generation/revision");
                            volume = reply.value("volume_mm3").toDouble();
                            vertexBytes = reply.value("vertices_bytes").toInteger();
                            auto bulkBytes = reply.value("bulk_bytes").toInteger();
                            wire::require(vertexBytes >= 0 && vertexBytes <= bulkBytes,
                                          "vertex payload range");
                            chunks.begin(epoch, quint64(bulkBytes),
                                         reply.value("transfer_checksum").toByteArray());
                            edges = reply.value("edges").toByteArray();
                            mesh.checksum = reply.value("checksum").toByteArray();
                            mesh.faces = int(reply.value("faces").toInteger());
                            wire::require(mesh.faces > 0 && mesh.faces <= int(limits::collection),
                                          "face count");
                            faces.clear();
                            for (const auto &face : reply.value("face_ids").toArray()) {
                                wire::require(face.isInteger() && face.toInteger() > 0 &&
                                                  face.toInteger() <= mesh.faces,
                                              "face ID range");
                                faces.append(int(face.toInteger()));
                            }
                        }
                } catch (const std::exception &e) {
                    error = e.what();
                    if (updated)
                        updated();
                }
            });
        });
    }
    ~Transport() {
        updated = {};
        stop();
    }
    void stop() {
        stopping = true;
        process.kill();
        process.waitForFinished(2000);
        if (socket) {
            socket->disconnect();
            socket->deleteLater();
            socket = nullptr;
        }
        server.close();
        authenticated = false;
        parser = wire::Frames();
        chunks.abandon();
        mesh = {};
        faces.clear();
        edges.clear();
        stopping = false;
    }
    void start() {
        stop();
        error.clear();
        epoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
        secret = QByteArray::number(QRandomGenerator::system()->generate64(), 16) +
                 QByteArray::number(QRandomGenerator::system()->generate64(), 16);
        auto path = root.path() + "/worker.sock";
        QLocalServer::removeServer(path);
        server.setSocketOptions(QLocalServer::UserAccessOption);
        CHECK(server.listen(path));
        auto executable = QCoreApplication::applicationDirPath() + "/slice_worker";
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        process.start(executable, {});
        CHECK(process.waitForStarted(3000));
        process.write(path.toUtf8().toBase64() + ' ' + secret + ' ' + epoch.toUtf8() + '\n');
        process.closeWriteChannel();
    }
    QString workerEpoch() const {
        return epoch;
    }
    qint64 workerPid() const {
        return process.processId();
    }
};
class View : public QOpenGLWidget, protected QOpenGLFunctions {
    QLabel overlay{this};
    QOpenGLShaderProgram shader;
    QOpenGLBuffer buffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QByteArray vertices;
    QByteArray edges;
    QList<int> faceIds;
    QPoint previous;
    bool dirty = true;
    float yaw = 35, pitch = -25, distance = 90;
    int count = 1, selected = 0, selectedEdge = 0;
    QElapsedTimer clock;
    QList<double> frames;
    QList<double> presentation, acknowledgment;
    qint64 lastSwap = 0, pendingInput = 0;
    QMatrix4x4 matrix(int instance = 0) const {
        QMatrix4x4 m;
        m.perspective(40, float(width()) / qMax(1, height()), .1f, 3000.f);
        m.translate(0, 0, -distance);
        m.rotate(pitch, 1, 0, 0);
        m.rotate(yaw, 0, 1, 0);
        if (count > 1)
            m.translate((instance % 10 - 4.5f) * 14, (instance / 10 - 4.5f) * 24, 0);
        m.translate(-5, -10, -15);
        return m;
    }

  public:
    std::function<void(int)> picked;
    explicit View(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
        setAccessibleName("Module 0 box viewport");
        setFocusPolicy(Qt::StrongFocus);
        overlay.setAttribute(Qt::WA_TransparentForMouseEvents);
        overlay.setStyleSheet("color: white; background: rgba(20,24,32,190); padding: 6px;");
        overlay.move(16, 12);
        overlay.show();
        clock.start();
        QObject::connect(this, &QOpenGLWidget::frameSwapped, [this] {
            auto now = clock.nsecsElapsed();
            if (pendingInput) {
                if (lastSwap)
                    presentation.append((now - lastSwap) / 1e6);
                acknowledgment.append((now - pendingInput) / 1e6);
                lastSwap = now;
                pendingInput = 0;
            }
        });
    }
    ~View() {
        makeCurrent();
        buffer.destroy();
        vao.destroy();
        doneCurrent();
    }
    void setMesh(const wire::Mesh &mesh, const QList<int> &faces, const QByteArray &edgeData) {
        mesh.check();
        vertices = mesh.vertices;
        faceIds = faces;
        edges = edgeData;
        dirty = true;
        update();
    }
    void instances(int value) {
        count = value;
        distance = value > 1 ? 500 : 90;
        yaw = 25;
        pitch = -20;
        lastSwap = 0;
        update();
    }
    void initializeGL() override {
        initializeOpenGLFunctions();
        CHECK(shader.addShaderFromSourceCode(
            QOpenGLShader::Vertex, "#version 330 core\nlayout(location=0) in vec3 position;uniform "
                                   "mat4 mvp;void main(){gl_Position=mvp*vec4(position,1.0);}"));
        CHECK(shader.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                             "#version 330 core\nuniform vec4 color;out vec4 "
                                             "outputColor;void main(){outputColor=color;}"));
        CHECK(shader.link());
        vao.create();
        buffer.create();
        std::cout << "GPU=" << reinterpret_cast<const char *>(glGetString(GL_RENDERER))
                  << " GL=" << reinterpret_cast<const char *>(glGetString(GL_VERSION)) << std::endl;
    }
    void paintGL() override {
        auto start = clock.nsecsElapsed();
        glClearColor(.08f, .10f, .13f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        if (!vertices.isEmpty()) {
            QOpenGLVertexArrayObject::Binder binding(&vao);
            buffer.bind();
            shader.bind();
            if (dirty) {
                auto all = vertices + edges;
                buffer.allocate(all.constData(), int(all.size()));
                dirty = false;
            }
            shader.enableAttributeArray(0);
            shader.setAttributeBuffer(0, GL_FLOAT, 0, 3, 12);
            for (int i = 0; i < count; ++i) {
                shader.setUniformValue("mvp", matrix(i));
                for (int triangle = 0; triangle < faceIds.size(); ++triangle) {
                    auto face = faceIds[triangle];
                    shader.setUniformValue(
                        "color", face == selected ? QVector4D(1, .65f, .1f, 1)
                                                  : QVector4D(.25f + .07f * face, .5f, .7f, 1));
                    glDrawArrays(GL_TRIANGLES, triangle * 3, 3);
                }
                glDisable(GL_DEPTH_TEST);
                for (int edge = 0; edge < edges.size() / 24; ++edge) {
                    shader.setUniformValue("color", edge + 1 == selectedEdge
                                                        ? QVector4D(1, .9f, .1f, 1)
                                                        : QVector4D(.05f, .08f, .1f, 1));
                    glDrawArrays(GL_LINES, int(vertices.size() / 12) + edge * 2, 2);
                }
                glEnable(GL_DEPTH_TEST);
            }
            shader.release();
            buffer.release();
        }
        overlay.setText(QString("Module 0 feasibility • mesh only\nDrag to orbit / wheel to "
                                "zoom\nFace %1 • %2 shared-mesh instances")
                            .arg(selected)
                            .arg(count));
        overlay.adjustSize();
        frames.append((clock.nsecsElapsed() - start) / 1e6);
    }
    int pick(const QPointF &position) {
        // CPU ray candidate from the same mesh/revision. No BRep in UI.
        auto inv = matrix().inverted();
        float x = 2 * position.x() / width() - 1, y = 1 - 2 * position.y() / height();
        auto near = inv.map(QVector3D(x, y, -1)), far = inv.map(QVector3D(x, y, 1)),
             direction = (far - near).normalized();
        float closest = INFINITY;
        int hit = 0;
        for (int t = 0; t < faceIds.size(); ++t) {
            QVector3D p[3];
            for (int j = 0; j < 3; ++j) {
                float v[3];
                std::memcpy(v, vertices.constData() + (t * 3 + j) * 12, 12);
                p[j] = QVector3D(v[0], v[1], v[2]);
            }
            auto e1 = p[1] - p[0], e2 = p[2] - p[0], h = QVector3D::crossProduct(direction, e2);
            auto determinant = QVector3D::dotProduct(e1, h);
            if (std::abs(determinant) < 1e-8)
                continue;
            auto s = near - p[0];
            auto u = QVector3D::dotProduct(s, h) / determinant;
            auto q = QVector3D::crossProduct(s, e1);
            auto v = QVector3D::dotProduct(direction, q) / determinant;
            auto distanceToHit = QVector3D::dotProduct(e2, q) / determinant;
            if (u >= 0 && v >= 0 && u + v <= 1 && distanceToHit > 0 && distanceToHit < closest) {
                closest = distanceToHit;
                hit = faceIds[t];
            }
        }
        selected = hit;
        update();
        if (picked)
            picked(hit);
        return hit;
    }
    QPointF edgePoint(int edge, int endpoint) const {
        float v[3];
        std::memcpy(v, edges.constData() + edge * 24 + endpoint * 12, 12);
        auto p = matrix().map(QVector3D(v[0], v[1], v[2]));
        return QPointF((p.x() + 1) * width() / 2, (1 - p.y()) * height() / 2);
    }
    int pickEdge(const QPointF &position) {
        selectedEdge = 0;
        double closest = 6;
        for (int edge = 0; edge < edges.size() / 24; ++edge) {
            auto a = edgePoint(edge, 0), b = edgePoint(edge, 1), d = b - a;
            double length = d.x() * d.x() + d.y() * d.y();
            if (length < 1e-8)
                continue;
            double t =
                qBound(0., ((position - a).x() * d.x() + (position - a).y() * d.y()) / length, 1.);
            double error = QLineF(position, a + t * d).length();
            if (error < closest) {
                closest = error;
                selectedEdge = edge + 1;
            }
        }
        update();
        return selectedEdge;
    }
    void mousePressEvent(QMouseEvent *e) override {
        previous = e->position().toPoint();
        if (!pickEdge(e->position()))
            pick(e->position());
        else if (picked)
            picked(-selectedEdge);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        auto d = e->position().toPoint() - previous;
        previous = e->position().toPoint();
        yaw += d.x();
        pitch += d.y();
        update();
    }
    void wheelEvent(QWheelEvent *e) override {
        distance = qBound(30.f, distance - e->angleDelta().y() / 30.f, 1000.f);
        update();
    }
    void rotateSample() {
        pendingInput = clock.nsecsElapsed();
        yaw += 1;
        repaint();
    }
    QList<double> samples() const {
        return frames;
    }
    QList<double> presentationSamples() const {
        return presentation;
    }
    QList<double> acknowledgmentSamples() const {
        return acknowledgment;
    }
};
int main(int argc, char **argv) {
    bool transportOnly = argc > 1 && QByteArray(argv[1]) == "--transport-test";
    bool profiling = argc > 1 && QByteArray(argv[1]) == "--profile";
    bool automated = profiling || (argc > 1 && QByteArray(argv[1]) == "--native-test");
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setApplicationName("Solidworks Clone Module 0");
    app.setOrganizationName("local.solidworks-clone.test");
    return run([&] {
        Transport transport;
        if (transportOnly) {
            transport.start();
            CHECK(QTest::qWaitFor(
                [&] { return !transport.mesh.vertices.isEmpty() || !transport.error.isEmpty(); },
                10000));
            CHECK(transport.error.isEmpty());
            near(transport.volume, 6000);
            CHECK(transport.mesh.faces == 6);
            CHECK(transport.faces.size() == 12);
            CHECK(transport.edges.size() == 12 * 24);
            auto old = transport.workerEpoch();
            transport.start();
            CHECK(old != transport.workerEpoch());
            CHECK(QTest::qWaitFor(
                [&] { return !transport.mesh.vertices.isEmpty() || !transport.error.isEmpty(); },
                10000));
            CHECK(transport.error.isEmpty());
            near(transport.volume, 6000);
            std::cout << "real QProcess/QLocalSocket handshake, inherited capability, worker mesh, "
                         "exact volume, restart epoch\n";
            return;
        }
        QMainWindow window;
        window.setWindowTitle("Solidworks Clone — Module 0 feasibility");
        window.resize(1280, 800);
        View view;
        window.setCentralWidget(&view);
        auto *dock = new QDockWidget("Feasibility controls", &window);
        auto *panel = new QWidget;
        auto *layout = new QVBoxLayout(panel);
        auto *label = new QLabel("Known box: 10 × 20 × 30 mm\nWorker measurement pending");
        layout->addWidget(label);
        auto *restart = new QPushButton("Restart worker");
        layout->addWidget(restart);
        auto *hundred = new QPushButton("Display 100 instances");
        layout->addWidget(hundred);
        layout->addStretch();
        dock->setWidget(panel);
        window.addDockWidget(Qt::LeftDockWidgetArea, dock);
        QObject::connect(restart, &QPushButton::clicked, [&] { transport.start(); });
        QObject::connect(hundred, &QPushButton::clicked, [&] { view.instances(100); });
        transport.updated = [&] {
            if (!transport.error.isEmpty()) {
                label->setText(transport.error);
                return;
            }
            view.setMesh(transport.mesh, transport.faces, transport.edges);
            label->setText(QString("Worker exact volume: %1 mm³\nFaces: %2 / triangles: %3")
                               .arg(transport.volume)
                               .arg(transport.mesh.faces)
                               .arg(transport.faces.size()));
        };
        view.picked = [&](int face) {
            window.statusBar()->showMessage(
                QString("Revision 1 • candidate face ID %1 • exact geometry remains in worker")
                    .arg(face));
        };
        window.show();
        window.raise();
        window.activateWindow();
        if (profiling) {
            QTest::qWait(30000);
            memorySample("idle");
        }
        transport.start();
        if (automated) {
            CHECK(QTest::qWaitFor(
                [&] { return !transport.mesh.vertices.isEmpty() || !transport.error.isEmpty(); },
                15000));
            CHECK(transport.error.isEmpty());
            CHECK(QTest::qWaitFor([&] { return view.isValid(); }, 10000));
            QTest::qWait(1000);
            CHECK(view.pick(QPointF(view.width() / 2., view.height() / 2.)) > 0);
            CHECK(view.pickEdge((view.edgePoint(0, 0) + view.edgePoint(0, 1)) / 2) > 0);
            if (profiling) {
                QTest::qWait(30000);
                for (int i = 0; i < 30; ++i) {
                    memorySample("box", transport.workerPid());
                    QTest::qWait(250);
                }
            }
            window.resize(1100, 750);
            QTest::qWait(200);
            dock->setFloating(true);
            QTest::qWait(200);
            dock->setFloating(false);
            QTest::qWait(200);
            for (int i = 0; i < 30; ++i) {
                view.rotateSample();
                QTest::qWait(20);
            }
            view.instances(100);
            QTest::qWait(100);
            for (int i = 0; i < 30; ++i) {
                view.rotateSample();
                QTest::qWait(20);
            }
            if (profiling) {
                QTest::qWait(30000);
                for (int i = 0; i < 30; ++i) {
                    memorySample("100instances", transport.workerPid());
                    QTest::qWait(250);
                }
            }
            auto screenshot = QCoreApplication::arguments().value(2);
            if (!screenshot.isEmpty())
                window.grab().save(screenshot);
            std::cout << "ui_pid=" << QCoreApplication::applicationPid()
                      << " worker_pid=" << transport.workerPid() << " frame_submission_ms=";
            for (auto value : view.samples())
                std::cout << value << ',';
            std::cout << std::endl;
            std::cout << "presentation_callback_intervals_ms=";
            for (auto value : view.presentationSamples())
                std::cout << value << ',';
            std::cout << std::endl;
            std::cout << "scripted_input_to_swap_ms=";
            for (auto value : view.acknowledgmentSamples())
                std::cout << value << ',';
            std::cout << std::endl;
            std::cout << "NavigationMetrics="
                      << (view.presentationSamples().size() >= 58 &&
                                  view.acknowledgmentSamples().size() >= 60
                              ? "Pass: box/100-instance samples collected"
                              : "Blocked: insufficient native swap callbacks; no FPS/input claim")
                      << std::endl;
            transport.start();
            CHECK(QTest::qWaitFor([&] { return !transport.mesh.vertices.isEmpty(); }, 10000));
            CHECK(transport.error.isEmpty());
            QTest::qWait(500);
            return;
        }
        app.exec();
        window.takeCentralWidget(); // stack-owned view
    });
}

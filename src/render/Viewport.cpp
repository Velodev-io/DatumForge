#include "Viewport.hpp"
#include <QLineF>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <cmath>
#include <limits>
namespace cad {
Viewport::Viewport(QWidget *parent) : QOpenGLWidget(parent) {
    setObjectName("viewport");
    setAccessibleName("Reference geometry viewport");
    setFocusPolicy(Qt::StrongFocus);
    overlay.setObjectName("viewportOverlay");
    overlay.setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay.setStyleSheet("color:white;background:rgba(20,24,32,190);padding:6px;");
    overlay.move(12, 12);
    setStale(false);
}
Viewport::~Viewport() {
    if (context())
        disconnect(context(), nullptr, this, nullptr);
    cleanup();
}
void Viewport::cleanup() {
    if (initialized && context()) {
        makeCurrent();
        buffer.destroy();
        vao.destroy();
        shader.removeAllShaders();
        doneCurrent();
        initialized = false;
    }
}
void Viewport::initializeGL() {
    initializeOpenGLFunctions();
    graphicsDetails =
        QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_VERSION))) + "; " +
        QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_RENDERER))) + "; " +
        QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &Viewport::cleanup,
            Qt::DirectConnection);
    bool okay = shader.addShaderFromSourceCode(
        QOpenGLShader::Vertex, "#version 330 core\nlayout(location=0) in vec3 position; uniform "
                               "mat4 matrix; void main(){gl_Position=matrix*vec4(position,1.0);}");
    okay = okay &&
           shader.addShaderFromSourceCode(
               QOpenGLShader::Fragment,
               "#version 330 core\nout vec4 pixel; uniform vec4 color; void main(){pixel=color;}");
    okay = okay && shader.link() && vao.create() && buffer.create();
    if (!okay) {
        failed = true;
        overlay.setText("Graphics initialization failed: " + shader.log());
        overlay.adjustSize();
        emit graphicsFailed(overlay.text());
        return;
    }
    initialized = true;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    frameClock.start();
}
QMatrix4x4 Viewport::matrix() const {
    QMatrix4x4 m;
    float aspect = float(qMax(width(), 1)) / qMax(height(), 1);
    if (orthographic)
        m.ortho(-span * aspect, span * aspect, -span, span, .1f, 100000.f);
    else
        m.perspective(40, aspect, .1f, 100000.f);
    m.translate(pan.x(), pan.y(), -distance);
    if (customOrientation)
        m.rotate(orientation);
    else {
        m.rotate(pitch, 1, 0, 0);
        m.rotate(yaw, 0, 1, 0);
    }
    m.translate(-center);
    return m;
}
void Viewport::draw(const QVector<QVector3D> &vertices, GLenum primitive, const QVector4D &color,
                    const Instance &instance) {
    if (vertices.empty())
        return;
    auto m = matrix();
    m.translate(instance.translation);
    m.rotate(instance.rotation);
    shader.setUniformValue("matrix", m);
    shader.setUniformValue("color", color);
    buffer.bind();
    buffer.allocate(vertices.constData(), int(vertices.size() * sizeof(QVector3D)));
    shader.enableAttributeArray(0);
    shader.setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(QVector3D));
    glDrawArrays(primitive, 0, vertices.size());
}
void Viewport::paintGL() {
    glClearColor(.09f, .12f, .16f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!initialized || failed || (!mesh && datums.empty()))
        return;
    auto began = frameClock.nsecsElapsed();
    QOpenGLVertexArrayObject::Binder bound(&vao);
    shader.bind();
    if (mesh) {
        QVector<QVector3D> triangles;
        triangles.reserve(mesh->indices.size());
        for (auto i : mesh->indices)
            triangles.append(mesh->vertices[i]);
        const QVector4D base = stale ? QVector4D(.42, .46, .5, 1) : QVector4D(.3, .64, .83, 1);
        for (const auto &instance : mesh->instances) {
            auto first = instance.firstTriangle;
            auto count =
                instance.triangleCount < 0 ? mesh->triangleRoles.size() : instance.triangleCount;
            auto firstEdge = instance.firstEdge;
            auto edgeCount = instance.edgeCount < 0 ? mesh->edgeIds.size() : instance.edgeCount;
            auto localTriangles = triangles.mid(first * 3, count * 3),
                 localEdges = mesh->edges.mid(firstEdge * 2, edgeCount * 2);
            if (mode != "Wireframe") {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1, 1);
                draw(localTriangles, GL_TRIANGLES,
                     mode == "Hidden edges" ? QVector4D(.09, .12, .16, 1) : base, instance);
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
            if (mode != "Shaded")
                draw(localEdges, GL_LINES, QVector4D(.85, .9, .95, 1), instance);
            if (selection.occurrence != instance.id)
                continue;
            QVector<QVector3D> highlight;
            if (selection.kind == "Face") {
                int role = mesh->faceIds.indexOf(selection.entity);
                for (int t = first; t < first + count; ++t)
                    if (mesh->triangleRoles[t] == quint32(role))
                        for (int j = 0; j < 3; ++j)
                            highlight.append(mesh->vertices[mesh->indices[t * 3 + j]]);
                draw(highlight, GL_TRIANGLES, QVector4D(1, .65, .15, 1), instance);
            } else if (selection.kind == "Edge") {
                int role = mesh->edgeIds.indexOf(selection.entity);
                if (role >= 0) {
                    highlight = {mesh->edges[role * 2], mesh->edges[role * 2 + 1]};
                    draw(highlight, GL_LINES, QVector4D(1, .65, .15, 1), instance);
                }
            } else
                draw(localEdges, GL_LINES, QVector4D(1, .65, .15, 1), instance);
        }
    }
    for (auto &d : datums)
        if (d.visible)
            draw(datumSegments(d.geometry.id), GL_LINES,
                 selection.entity == d.geometry.id ? QVector4D(1, .65, .15, 1)
                 : stale                           ? QVector4D(.42, .46, .5, 1)
                                                   : QVector4D(.5, .85, .55, 1),
                 Instance{});
    shader.release();
    buffer.release();
    frameTimes.append(double(frameClock.nsecsElapsed() - began) / 1e6);
    if (frameTimes.size() > 1000)
        frameTimes.removeFirst();
}
void Viewport::resizeGL(int, int) {
    update();
}
void Viewport::setMesh(MeshPtr value) {
    bool first = !mesh;
    mesh = std::move(value);
    if (first && mesh)
        fit();
    update();
}
void Viewport::setSelection(Selection value) {
    selection = std::move(value);
    update();
}
void Viewport::setStale(bool value) {
    stale = value;
    if (failed) {
        update();
        return;
    }
    overlay.setText((stale ? "STALE · " : "") + sceneLabel + " · " + mode +
                    "\nLeft: select · Right: orbit · Middle: pan · Wheel: zoom");
    overlay.adjustSize();
    update();
}
void Viewport::setMode(QString value) {
    mode = std::move(value);
    setStale(stale);
}
void Viewport::fit() {
    if (!mesh && datums.empty())
        return;
    QVector3D low, high;
    bool first = true;
    if (mesh)
        for (auto &instance : mesh->instances)
            for (int t = instance.firstTriangle;
                 t < (instance.triangleCount < 0 ? mesh->triangleRoles.size()
                                                 : instance.firstTriangle + instance.triangleCount);
                 ++t)
                for (int k = 0; k < 3; ++k) {
                    auto v = instance.map(mesh->vertices[mesh->indices[t * 3 + k]]);
                    if (first) {
                        low = high = v;
                        first = false;
                    } else
                        for (int j = 0; j < 3; ++j) {
                            low[j] = qMin(low[j], v[j]);
                            high[j] = qMax(high[j], v[j]);
                        }
                }
    for (auto &d : datums)
        if (d.visible)
            for (auto v : datumSegments(d.geometry.id)) {
                if (first) {
                    low = high = v;
                    first = false;
                } else
                    for (int j = 0; j < 3; ++j) {
                        low[j] = qMin(low[j], v[j]);
                        high[j] = qMax(high[j], v[j]);
                    }
            }
    if (first)
        return;
    center = (low + high) / 2;
    float radius = qMax((high - low).length() / 2, 1.f);
    float aspect = float(qMax(width(), 1)) / qMax(height(), 1);
    span = radius * 1.25f / qMin(1.f, aspect);
    distance = span / std::tan(qDegreesToRadians(20.f)) + radius;
    pan = {};
    update();
}
void Viewport::setView(QString view) {
    customOrientation = false;
    orthographic = view != "Isometric";
    yaw = view == "Right" ? 90 : view == "Isometric" ? 35 : 0;
    pitch = view == "Top" ? 90 : view == "Isometric" ? -25 : 0;
    fit();
}
void Viewport::setDatums(std::vector<DisplayDatum> values) {
    datums = std::move(values);
    update();
}
bool Viewport::normalTo(const sketch::Frame &frame) {
    try {
        frame.validate();
    } catch (const std::exception &) {
        return false;
    }
    auto vector = [](sketch::Vec3 p) { return QVector3D(p[0], p[1], p[2]); };
    auto u = vector(frame.u), v = vector(frame.v), n = QVector3D::crossProduct(u, v);
    QMatrix3x3 rotation;
    for (int j = 0; j < 3; ++j) {
        rotation(0, j) = u[j];
        rotation(1, j) = v[j];
        rotation(2, j) = n[j];
    }
    orientation = QQuaternion::fromRotationMatrix(rotation);
    customOrientation = true;
    orthographic = true;
    center = vector(frame.origin) * 1000;
    pan = {};
    update();
    return true;
}
QVector<QVector3D> Viewport::datumSegments(QString id) const {
    auto vector = [](sketch::Vec3 p) { return QVector3D(p[0], p[1], p[2]); };
    for (auto &display : datums) {
        auto &d = display.geometry;
        if (d.id != id)
            continue;
        auto o = vector(d.frame.origin) * 1000;
        float size = 10;
        if (d.kind == "DatumAxis") {
            auto direction = vector(d.direction) * size;
            return {o - direction, o + direction};
        }
        auto u = vector(d.frame.u) * size, v = vector(d.frame.v) * size;
        return {o - u - v, o + u - v, o + u - v,    o + u + v,    o + u + v,    o - u + v,
                o - u + v, o - u - v, o - u * .15f, o + u * .15f, o - v * .15f, o + v * .15f};
    }
    return {};
}
QPointF Viewport::project(QVector3D point) const {
    auto p = matrix() * QVector4D(point, 1);
    if (p.w() <= 0)
        return {-1e9, -1e9};
    return {(p.x() / p.w() + 1) * width() / 2, (1 - p.y() / p.w()) * height() / 2};
}
Selection Viewport::pick(QPointF pos) const {
    if (stale || failed)
        return {};
    if (filter == "Datum") {
        Selection hit;
        double best = 9, depth = std::numeric_limits<double>::max();
        for (auto &d : datums) {
            if (!d.visible)
                continue;
            auto vertices = datumSegments(d.geometry.id);
            for (int i = 0; i + 1 < vertices.size(); i += 2) {
                auto a = project(vertices[i]), b = project(vertices[i + 1]), ab = b - a;
                if (std::abs(a.x()) >= 1e8 || std::abs(b.x()) >= 1e8)
                    continue;
                auto length2 = QPointF::dotProduct(ab, ab);
                double t = length2 > 1e-12
                               ? qBound(0., QPointF::dotProduct(pos - a, ab) / length2, 1.)
                               : 0.;
                auto pixels = QLineF(pos, a + ab * t).length();
                auto p = matrix() * QVector4D(vertices[i] * (1 - t) + vertices[i + 1] * t, 1);
                double z = p.z() / p.w();
                if (p.w() <= 0 || z < -1 || z > 1)
                    continue;
                if (pixels < best - .01 || (std::abs(pixels - best) < .01 && z < depth)) {
                    best = pixels;
                    depth = z;
                    hit = {d.geometry.kind, d.geometry.id, {}};
                }
            }
        }
        return hit;
    }
    if (!mesh)
        return {};
    bool invertible = false;
    auto inverse = matrix().inverted(&invertible);
    if (!invertible)
        return {};
    QVector3D screen(float(pos.x() * 2 / width() - 1), float(1 - pos.y() * 2 / height()), -1);
    auto near = inverse.map(screen);
    screen.setZ(1);
    auto direction = (inverse.map(screen) - near).normalized();
    float closest = std::numeric_limits<float>::max();
    Selection hit;
    auto intersect = [&](QVector3D a, QVector3D b, QVector3D c) {
        auto e1 = b - a, e2 = c - a, p = QVector3D::crossProduct(direction, e2);
        float det = QVector3D::dotProduct(e1, p);
        if (std::abs(det) < 1e-7)
            return -1.f;
        float inv = 1 / det;
        auto s = near - a;
        float u = QVector3D::dotProduct(s, p) * inv;
        if (u < 0 || u > 1)
            return -1.f;
        auto q = QVector3D::crossProduct(s, e1);
        float v = QVector3D::dotProduct(direction, q) * inv;
        if (v < 0 || u + v > 1)
            return -1.f;
        return QVector3D::dotProduct(e2, q) * inv;
    };
    for (const auto &instance : mesh->instances)
        for (int t = instance.firstTriangle;
             t < (instance.triangleCount < 0 ? mesh->triangleRoles.size()
                                             : instance.firstTriangle + instance.triangleCount);
             ++t) {
            auto a = instance.map(mesh->vertices[mesh->indices[t * 3]]),
                 b = instance.map(mesh->vertices[mesh->indices[t * 3 + 1]]),
                 c = instance.map(mesh->vertices[mesh->indices[t * 3 + 2]]);
            float d = intersect(a, b, c);
            if (d >= 0 && d < closest) {
                closest = d;
                hit = {"Face", mesh->faceIds[mesh->triangleRoles[t]], instance.id};
            }
        }
    if (filter == "Body" || filter == "Component") {
        if (!hit.empty())
            return {filter,
                    wire::uuid(hit.entity.left(36)) ? hit.entity.left(36)
                                                    : QString("body/reference-box"),
                    hit.occurrence};
        return {};
    }
    if (filter != "Edge")
        return hit;
    Selection edgeHit;
    double best = 8;
    float depth = std::numeric_limits<float>::max();
    for (const auto &instance : mesh->instances)
        for (int e = instance.firstEdge;
             e < (instance.edgeCount < 0 ? mesh->edgeIds.size()
                                         : instance.firstEdge + instance.edgeCount);
             ++e) {
            auto a = instance.map(mesh->edges[e * 2]), b = instance.map(mesh->edges[e * 2 + 1]);
            auto pa = project(a), pb = project(b), ab = pb - pa;
            double length = QPointF::dotProduct(ab, ab);
            double u = length > 0 ? qBound(0., QPointF::dotProduct(pos - pa, ab) / length, 1.) : 0;
            double pixels = QLineF(pos, pa + ab * u).length();
            auto point = a + (b - a) * u;
            float d = QVector3D::dotProduct(point - near, direction);
            if (pixels <= best && d >= 0 &&
                (closest == std::numeric_limits<float>::max() ||
                 d <= closest + qMax(.1f, span * .015f)) &&
                (pixels < best - .2 || d < depth)) {
                best = pixels;
                depth = d;
                edgeHit = {"Edge", mesh->edgeIds[e], instance.id};
            }
        }
    return edgeHit;
}
void Viewport::mousePressEvent(QMouseEvent *e) {
    previous = press = e->position().toPoint();
    draggedOccurrence =
        componentDrag && e->button() == Qt::LeftButton ? pick(e->position()).occurrence : QString();
    setFocus();
}
void Viewport::mouseMoveEvent(QMouseEvent *e) {
    auto delta = e->position().toPoint() - previous;
    previous = e->position().toPoint();
    if (e->buttons() & Qt::RightButton) {
        if (customOrientation)
            orientation = QQuaternion::fromAxisAndAngle(1, 0, 0, delta.y() * .5f) *
                          QQuaternion::fromAxisAndAngle(0, 1, 0, delta.x() * .5f) * orientation;
        else {
            yaw += delta.x() * .5f;
            pitch = qBound(-89.f, pitch + delta.y() * .5f, 89.f);
        }
    }
    if (e->buttons() & Qt::MiddleButton)
        pan += QVector2D(delta.x(), -delta.y()) * (2 * span / qMax(height(), 1));
    update();
}
void Viewport::mouseReleaseEvent(QMouseEvent *e) {
    if (componentDrag && e->button() == Qt::LeftButton && !draggedOccurrence.isEmpty() &&
        (e->position().toPoint() - press).manhattanLength() >= 4) {
        auto delta = e->position() - press;
        QMatrix4x4 rotation;
        if (customOrientation)
            rotation.rotate(orientation);
        else {
            rotation.rotate(pitch, 1, 0, 0);
            rotation.rotate(yaw, 0, 1, 0);
        }
        auto world = rotation.inverted().mapVector(
            QVector3D(float(delta.x()), float(-delta.y()), 0) * (2 * span / qMax(height(), 1)));
        emit componentDragged(draggedOccurrence, world);
        draggedOccurrence.clear();
        return;
    }
    if (e->button() == Qt::LeftButton && (e->position().toPoint() - press).manhattanLength() < 4)
        emit selected(pick(e->position()));
}
void Viewport::wheelEvent(QWheelEvent *e) {
    float factor = std::exp(-e->angleDelta().y() / 120.f * .12f);
    distance = qBound(2.f, distance * factor, 50000.f);
    span = qBound(.5f, span * factor, 20000.f);
    update();
    e->accept();
}
QString Viewport::graphicsDescription() const {
    if (!context())
        return "Unavailable";
    auto f = context()->format();
    return QString("OpenGL %1.%2; DPR %3; initialized %4")
               .arg(f.majorVersion())
               .arg(f.minorVersion())
               .arg(devicePixelRatioF())
               .arg(initialized && !failed) +
           "; " + graphicsDetails;
}
} // namespace cad

#pragma once
#include "model/DatumGeometry.hpp"
#include "protocol/Client.hpp"
#include <QElapsedTimer>
#include <QLabel>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
namespace cad {
struct DisplayDatum {
    model::DatumGeometry geometry;
    bool visible = true;
};
class Viewport : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
    MeshPtr mesh;
    Selection selection;
    QOpenGLShaderProgram shader;
    QOpenGLBuffer buffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QLabel overlay{this};
    QString sceneLabel = "mm · Reference scene";
    QString mode = "Shaded with edges", filter = "Face", graphicsDetails;
    QVector3D center;
    float yaw = 35, pitch = -25, distance = 100, span = 50;
    QVector2D pan;
    std::vector<DisplayDatum> datums;
    QQuaternion orientation;
    bool customOrientation = false;
    QPoint previous, press;
    bool componentDrag = false;
    QString draggedOccurrence;
    bool initialized = false, failed = false, stale = false, orthographic = false;
    QList<double> frameTimes;
    QElapsedTimer frameClock;
    QMatrix4x4 matrix() const;
    void draw(const QVector<QVector3D> &vertices, GLenum primitive, const QVector4D &color,
              const Instance &instance);
    void cleanup();

  protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int, int) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

  public:
    explicit Viewport(QWidget *parent = nullptr);
    ~Viewport() override;
    void setMesh(MeshPtr value);
    void setComponentDrag(bool enabled) {
        componentDrag = enabled;
    }
    void setSelection(Selection value);
    void setStale(bool value);
    void setContext(QString value) {
        sceneLabel = std::move(value);
        setStale(stale);
    }
    void setMode(QString value);
    void setFilter(QString value) {
        filter = value;
    }
    void fit();
    void setView(QString view);
    void setDatums(std::vector<DisplayDatum> values);
    bool normalTo(const sketch::Frame &frame);
    QVector<QVector3D> datumSegments(QString id) const;
    Selection pick(QPointF position) const;
    QPointF project(QVector3D point) const;
    QString graphicsDescription() const;
    QList<double> samples() const {
        return frameTimes;
    }
    float cameraDistance() const {
        return distance;
    }
    QVector2D cameraPan() const {
        return pan;
    }
  signals:
    void selected(cad::Selection selection);
    void componentDragged(QString occurrence, QVector3D deltaMM);
    void graphicsFailed(QString reason);
};
} // namespace cad

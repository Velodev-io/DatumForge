#pragma once
#include "sketch/ArcGeometry.hpp"
#include "sketch/Sketch.hpp"
#include <QWidget>
#include <optional>
namespace cad {
class SketchView : public QWidget {
    Q_OBJECT
    sketch::Sketch drawing;
    QMap<QString, std::vector<sketch::Vec2>> curveDisplay;
    QString selectedId, dragging;
    double scale = 6000;
    sketch::Vec2 cameraCenter;
    bool editable = false;
    bool present = false;
    QString tool = "Select / drag";
    QString rectangleMethod = "Corner";
    std::vector<sketch::Vec2> rectangleInputs;
    QString slotMethod = "Straight";
    std::vector<sketch::Vec2> slotInputs;
    std::vector<sketch::Vec2> curveInputs;
    int curveDegree = 3;
    bool curveClosed = false;
    std::optional<sketch::Vec2> circleCenter, inputAnchor;
    std::optional<sketch::Vec2> arcCenter, arcStart;
    double arcLastAngle = 0, arcSweep = 0;
    struct Endpoint {
        QString curve;
        int end = 0;
        sketch::Vec2 point, direction;
    };
    std::optional<Endpoint> tangentSeed;
    std::optional<sketch::Vec2> tangentDirection;
    bool normalIntent = false, arcPressed = false;
    int arcPressStage = 0;
    std::optional<Endpoint> continuation;
    bool continuationAway = false;
    sketch::Vec2 cursor;
    QPoint previous, pressedAt;
    bool primitivePressed = false, secondPress = false, spaceDown = false;
    void zoomAt(QPointF at, double factor);
    void finishCircle(QPointF at);
    void moveArc(sketch::Vec2 at);
    QString arcPrompt() const;
    bool arcTool() const;
    std::optional<sketch::ArcConstruction> arcPreview() const;
    std::optional<Endpoint> endpointAt(QPointF position) const;
    void finishArc();

  protected:
    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  public:
    explicit SketchView(QWidget *parent = nullptr);
    void showSketch(sketch::Sketch sketch, QString selected, bool editing);
    void clearSketch();
    void setTool(QString name);
    void setInputAnchor(std::optional<sketch::Vec2> anchor);
    void setRectangleInput(QString method, std::vector<sketch::Vec2> inputs);
    void setSlotInput(QString method, std::vector<sketch::Vec2> inputs);
    void setCurveInput(QString kind, std::vector<sketch::Vec2> inputs, int degree, bool closed);
    bool cancelPointerInput();
    bool sizingCircle() const {
        return circleCenter.has_value();
    }
    bool sizingArc() const {
        return arcCenter.has_value();
    }
    void beginTangent(QString curve, int end);
    void continueFrom(QString curve, int end);
    QPointF screen(sketch::Vec2 p) const;
    sketch::Vec2 local(QPointF p) const;
    QString pick(QPointF p) const;
    void fit();
    sketch::Vec2 snapPoint(sketch::Vec2 point) const;
  signals:
    void clicked(double x, double y);
    void completePrimitive();
    void selected(QString id);
    void dragged(QString point, double x, double y);
    void circleCreated(double x, double y, double radius);
    void arcCreated(double x, double y, double radius, double start, double sweep);
    void arcJoined(double x, double y, double radius, double start, double sweep, QString source,
                   int ownEnd, int sourceEnd, bool normal);
    void tangentTransition(QString source, int end);
    void dragStarted(QString entity, double x, double y);
    void dragFinished();
    void dragCanceled();
    void editRequested(QString entity);
    void deleteRequested();
    void canceled();
};
} // namespace cad

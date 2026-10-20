#pragma once
#include "sketch/Sketch.hpp"
#include <QWidget>
#include <optional>
namespace cad {
class SketchView : public QWidget {
    Q_OBJECT
    sketch::Sketch drawing;
    QString selectedId, dragging;
    double scale = 6000;
    sketch::Vec2 cameraCenter;
    bool editable = false;
    bool present = false;
    QString tool = "Point";
    std::optional<sketch::Vec2> circleCenter, inputAnchor;
    sketch::Vec2 cursor;
    QPoint previous, pressedAt;
    bool primitivePressed = false, secondPress = false, spaceDown = false;
    void zoomAt(QPointF at, double factor);
    void finishCircle(QPointF at);

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
    bool cancelPointerInput();
    bool sizingCircle() const {
        return circleCenter.has_value();
    }
    QPointF screen(sketch::Vec2 p) const;
    sketch::Vec2 local(QPointF p) const;
    QString pick(QPointF p) const;
    void fit();
    sketch::Vec2 snapPoint(sketch::Vec2 point) const;
  signals:
    void clicked(double x, double y);
    void selected(QString id);
    void dragged(QString point, double x, double y);
    void circleCreated(double x, double y, double radius);
    void dragStarted(QString entity, double x, double y);
    void dragFinished();
    void dragCanceled();
    void editRequested(QString entity);
    void deleteRequested();
    void canceled();
};
} // namespace cad

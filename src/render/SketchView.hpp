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
    QPointF offset{0, 0};
    bool editable = false;
    bool present = false;
    QString tool = "Point";
    std::optional<sketch::Vec2> circleCenter, inputAnchor;
    sketch::Vec2 cursor;
    QPoint previous;

  protected:
    void paintEvent(QPaintEvent *) override;
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
  signals:
    void clicked(double x, double y);
    void selected(QString id);
    void dragged(QString point, double x, double y);
    void circleCreated(double x, double y, double radius);
    void canceled();
};
} // namespace cad

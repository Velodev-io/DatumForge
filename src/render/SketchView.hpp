#pragma once
#include "sketch/Sketch.hpp"
#include <QWidget>
namespace cad {
class SketchView : public QWidget {
    Q_OBJECT
    sketch::Sketch drawing;
    QString selectedId, dragging;
    double scale = 6000;
    QPointF offset{0, 0};
    bool editable = false;
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
    QPointF screen(sketch::Vec2 p) const;
    sketch::Vec2 local(QPointF p) const;
    QString pick(QPointF p) const;
    void fit();
  signals:
    void clicked(double x, double y);
    void selected(QString id);
    void dragged(QString point, double x, double y);
    void canceled();
};
} // namespace cad

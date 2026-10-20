#include "render/SketchView.hpp"
#include <QApplication>
#include <QNativeGestureEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>
using namespace cad;
class Input : public QObject {
    Q_OBJECT
  private slots:
    void stableResize() {
        SketchView view;
        view.resize(600, 600);
        view.show();
        QTest::qWait(20);
        auto before = view.local(QPointF(view.width() / 2., view.height() / 2.));
        view.resize(900, 800);
        QTest::qWait(20);
        auto after = view.local(QPointF(view.width() / 2., view.height() / 2.));
        QVERIFY(std::abs(before.x - after.x) < 1e-12);
        QVERIFY(std::abs(before.y - after.y) < 1e-12);
    }
    void trackpadPan() {
        SketchView view;
        view.resize(800, 600);
        view.show();
        QTest::qWait(20);
        QPointF at(400, 300), delta(37, -21);
        auto before = view.screen({.01, .01});
        QWheelEvent wheel(at, view.mapToGlobal(at), delta.toPoint(), {}, Qt::NoButton,
                          Qt::NoModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(&view, &wheel);
        auto after = view.screen({.01, .01});
        QVERIFY(QLineF(after, before + delta).length() < 1e-8);
    }
    void nativePinch() {
        SketchView view;
        view.resize(800, 600);
        view.show();
        QTest::qWait(20);
        QPointF at(320, 240);
        auto anchor = view.local(at);
        double before = QLineF(view.screen({0, 0}), view.screen({.01, 0})).length();
        QNativeGestureEvent pinch(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(),
                                  2, at, at, view.mapToGlobal(at), .25, {});
        QApplication::sendEvent(&view, &pinch);
        auto after = view.local(at);
        QVERIFY(std::abs(anchor.x - after.x) < 1e-12 && std::abs(anchor.y - after.y) < 1e-12);
        QVERIFY(std::abs(QLineF(view.screen({0, 0}), view.screen({.01, 0})).length() / before -
                         1.25) < 1e-12);
    }
    void fitFullCircle() {
        SketchView view;
        view.resize(800, 600);
        sketch::Sketch s;
        auto center = s.point({1, -2});
        s.circle(center, 3);
        view.showSketch(s, {}, false);
        view.fit();
        for (auto p : std::array<sketch::Vec2, 4>{{{-2, -2}, {4, -2}, {1, 1}, {1, -5}}}) {
            auto at = view.screen(p);
            QVERIFY(at.x() > 30 && at.x() < view.width() - 30 && at.y() > 30 &&
                    at.y() < view.height() - 30);
        }
    }
    void resizeWhileDrawing() {
        SketchView view;
        view.resize(600, 600);
        view.showSketch({}, {}, true);
        view.setTool("Circle");
        view.show();
        QTest::qWait(20);
        QSignalSpy created(&view, &SketchView::circleCreated);
        auto center = view.screen({0, 0}).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center);
        view.resize(900, 700);
        QTest::qWait(20);
        auto end = view.screen({.01, 0}).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(created.size(), 1);
        QVERIFY(std::abs(created[0][0].toDouble()) < 1e-12 &&
                std::abs(created[0][1].toDouble()) < 1e-12);
        QVERIFY(std::abs(created[0][2].toDouble() - .01) < 1e-12);
    }
    void preciseCircleDrag() {
        SketchView view;
        view.resize(800, 600);
        view.showSketch({}, {}, true);
        view.setTool("Circle");
        view.show();
        QTest::qWait(20);
        auto center = view.screen({.01, .01}).toPoint();
        auto end = center + QPoint(17, 0);
        QSignalSpy created(&view, &SketchView::circleCreated);
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, center);
        QMouseEvent move(QEvent::MouseMove, end, view.mapToGlobal(end), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &move);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(created.size(), 1);
        QVERIFY(std::abs(created[0][2].toDouble() -
                         sketch::length(view.local(center), view.local(end))) < 1e-12);
    }
    void clickCircle() {
        SketchView view;
        view.resize(800, 600);
        view.showSketch({}, {}, true);
        view.setTool("Circle");
        view.show();
        QTest::qWait(20);
        auto center = view.screen({.01, .01}).toPoint(), end = center + QPoint(60, 0);
        QSignalSpy created(&view, &SketchView::circleCreated);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center);
        QTest::mouseMove(&view, end);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(created.size(), 1);
    }
};
QTEST_MAIN(Input)
#include "input.moc"

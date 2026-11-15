#include "render/SketchView.hpp"
#include <QApplication>
#include <QNativeGestureEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>
using namespace cad;
class Input : public QObject {
    Q_OBJECT
    void move(SketchView &view, QPoint at) {
        QMouseEvent event(QEvent::MouseMove, at, view.mapToGlobal(at), Qt::NoButton, Qt::NoButton,
                          Qt::NoModifier);
        QApplication::sendEvent(&view, &event);
        QTest::qWait(5);
    }
    bool previewInk(SketchView &view, sketch::Vec2 p) {
        auto image = view.grab().toImage();
        auto at = view.screen(p) * image.devicePixelRatio();
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx) {
                auto c = image.pixelColor(qRound(at.x()) + dx, qRound(at.y()) + dy);
                if (c.red() > 160 && c.green() > 90 && c.blue() < 150)
                    return true;
            }
        return false;
    }

  private slots:
    void threePointArcUsesPointerBend() {
        SketchView view;
        view.resize(800, 600);
        view.showSketch({}, {}, true);
        view.setTool("3 Point Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy created(&view, &SketchView::arcCreated);
        for (auto p : {sketch::Vec2{-.01, 0}, sketch::Vec2{.01, 0}, sketch::Vec2{0, .01}})
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen(p).toPoint());
        QCOMPARE(created.size(), 1);
        QVERIFY(std::abs(created[0][2].toDouble() - .01) < 1e-12);
        QVERIFY(std::abs(created[0][4].toDouble() - 3.14159265358979323846) < 1e-12);
    }
    void tangentAndNormalArcIntent() {
        SketchView view;
        view.resize(800, 600);
        sketch::Sketch s;
        auto line = s.line(s.point({-.02, 0}), s.point({0, 0}));
        view.showSketch(s, {}, true);
        view.setTool("Tangent Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy joined(&view, &SketchView::arcJoined);
        QSignalSpy clicks(&view, &SketchView::clicked);
        auto click = [&](sketch::Vec2 p) {
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen(p).toPoint());
        };
        click({0, 0});
        move(view, view.screen({.003, 0}).toPoint());
        move(view, view.screen({.01, .01}).toPoint());
        QVERIFY(previewInk(view, {.0070710678, .0029289322}));
        QCOMPARE(joined.size(), 0);
        click({.01, .01});
        QCOMPARE(joined.size(), 1);
        QCOMPARE(joined[0][5].toString(), line);
        QCOMPARE(joined[0][6].toInt(), 0);
        QCOMPARE(joined[0][7].toInt(), 1);
        QVERIFY(!joined[0][8].toBool());
        QVERIFY(std::abs(joined[0][2].toDouble() - .01) < 1e-12);
        click({0, 0});
        move(view, view.screen({.003, 0}).toPoint());
        move(view, view.screen({0, 0}).toPoint()); // Return resets the intent zone.
        move(view, view.screen({0, .003}).toPoint());
        move(view, view.screen({-.01, .01}).toPoint());
        click({-.01, .01});
        QCOMPARE(joined.size(), 2);
        QVERIFY(joined[1][8].toBool());
        QCOMPARE(clicks.size(), 0);
        click({.04, .03});
        QVERIFY(!view.sizingArc());
        auto other = s.line(s.entity(line).b, s.point({0, -.02}));
        view.showSketch(s, {}, true);
        click({0, 0});
        QVERIFY(!view.sizingArc());
        view.showSketch(s, other, true);
        click({0, 0});
        QVERIFY(view.sizingArc());
        QVERIFY(view.cancelPointerInput());
    }
    void tangentArcEightIntentResults() {
        SketchView view;
        view.resize(800, 600);
        sketch::Sketch s;
        auto line = s.line(s.point({-.02, 0}), s.point({0, 0}));
        view.showSketch(s, {}, true);
        view.setTool("Tangent Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy joined(&view, &SketchView::arcJoined);
        for (auto direction : {sketch::Vec2{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
            for (double side : {-1., 1.}) {
                int previous = joined.size();
                QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier,
                                  view.screen({0, 0}).toPoint());
                move(view, view.screen({direction.x * .003, direction.y * .003}).toPoint());
                sketch::Vec2 end{.01 * (direction.x - side * direction.y),
                                 .01 * (direction.y + side * direction.x)};
                move(view, view.screen(end).toPoint());
                QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier,
                                  view.screen(end).toPoint());
                QCOMPARE(joined.size(), previous + 1);
                QCOMPARE(joined.last()[5].toString(), line);
                QCOMPARE(joined.last()[8].toBool(), direction.y != 0);
                auto &v = joined.last();
                auto first = sketch::arcPoint({v[0].toDouble(), v[1].toDouble()}, v[2].toDouble(),
                                              v[3].toDouble() + v[6].toInt() * v[4].toDouble());
                QVERIFY(sketch::length(first, {0, 0}) < 1e-12);
                QVERIFY(std::abs(v[2].toDouble() - .01) < 1e-12);
            }
        QCOMPARE(joined.size(), 8);
    }
    void dragToSizeArcAndThreePointDegeneracy() {
        SketchView view;
        view.resize(800, 600);
        view.showSketch({}, {}, true);
        view.setTool("Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy made(&view, &SketchView::arcCreated);
        auto center = view.screen({0, 0}).toPoint(), start = view.screen({.01, 0}).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, center);
        QMouseEvent held(QEvent::MouseMove, start, view.mapToGlobal(start), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &held);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, start);
        QCOMPARE(made.size(), 0);
        move(view, view.screen({0, .01}).toPoint());
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen({0, .01}).toPoint());
        QCOMPARE(made.size(), 1);
        QVERIFY(std::abs(made[0][2].toDouble() - .01) < 1e-12);
        view.setTool("3 Point Arc");
        auto click = [&](sketch::Vec2 p) {
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen(p).toPoint());
        };
        click({0, 0});
        click({.01, 0});
        click({.005, 0});
        QCOMPARE(made.size(), 1);
        QVERIFY(view.sizingArc());
        click({.005, .005});
        QCOMPARE(made.size(), 2);
        view.setTool("3 Point Arc");
        click({-.01, 0});
        click({.01, 0});
        click({0, -.02});
        QCOMPARE(made.size(), 3);
        QVERIFY(made[2][4].toDouble() > sketch::arcPi);
    }
    void centerpointArcIsTransient() {
        SketchView view;
        view.resize(800, 600);
        view.showSketch({}, {}, true);
        view.setTool("Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy clicks(&view, &SketchView::clicked);
        auto center = view.screen({0, 0}).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(clicks.size(), 0); // Center alone must not create a fixed 2 mm quarter arc.
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center + QPoint(60, 0));
        QCOMPARE(clicks.size(), 0); // Radius/start alone is still a transient construction.
        QVERIFY(view.cancelPointerInput());
    }
    void centerpointArcPreviewAndDirections() {
        SketchView view;
        view.resize(800, 600);
        view.showSketch({}, {}, true);
        view.setTool("Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy created(&view, &SketchView::arcCreated);
        auto click = [&](sketch::Vec2 at) {
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen(at).toPoint());
        };
        click({0, 0});
        move(view, view.screen({.005, 0}).toPoint());
        QVERIFY(previewInk(view, {0, .005}));
        move(view, view.screen({.01, 0}).toPoint());
        QVERIFY(previewInk(view, {0, .01}));
        QVERIFY(!previewInk(view, {0, .005}));
        QCOMPARE(created.size(), 0);
        click({.01, 0});
        move(view, view.screen({.007, .007}).toPoint());
        move(view, view.screen({0, .01}).toPoint());
        QCOMPARE(created.size(), 0);
        // The live quarter arc passes through 45 degrees, away from both radial guides.
        QVERIFY2(previewInk(view, {.0070710678, .0070710678}),
                 "The chosen radius and sweep must be visible before the final click");
        click({0, .01});
        QCOMPARE(created.size(), 1);
        QVERIFY(std::abs(created[0][2].toDouble() - .01) < 1e-12);
        QVERIFY(std::abs(created[0][3].toDouble()) < 1e-12);
        QVERIFY(std::abs(created[0][4].toDouble() - 3.14159265358979323846 / 2) < 1e-12);
        QVERIFY(!view.sizingArc());
        click({0, 0});
        click({.01, 0});
        move(view, view.screen({0, -.01}).toPoint());
        click({0, -.01});
        QCOMPARE(created.size(), 2);
        QVERIFY(std::abs(created[1][3].toDouble() + 3.14159265358979323846 / 2) < 1e-12);
        QVERIFY(std::abs(created[1][4].toDouble() - 3.14159265358979323846 / 2) < 1e-12);
        click({0, 0});
        click({.01, 0});
        for (auto at : {sketch::Vec2{0, .01}, sketch::Vec2{-.01, 0}, sketch::Vec2{0, -.01}})
            move(view, view.screen(at).toPoint());
        click({0, -.01});
        QCOMPARE(created.size(), 3);
        QVERIFY(std::abs(created[2][4].toDouble() - 3 * 3.14159265358979323846 / 2) < 1e-12);
    }
    void arcResizeInvalidAndCancel() {
        SketchView view;
        view.resize(600, 600);
        view.showSketch({}, {}, true);
        view.setTool("Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy created(&view, &SketchView::arcCreated);
        auto click = [&](sketch::Vec2 at) {
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen(at).toPoint());
        };
        click({0, 0});
        click({0, 0}); // A zero radius must leave the construction pending.
        click({.01, 0});
        click({.01, 0}); // A zero sweep is not a completed arc.
        click({0, 0});   // The center cannot be an end point.
        QCOMPARE(created.size(), 0);
        view.resize(900, 700);
        QTest::qWait(20);
        for (auto at : {sketch::Vec2{0, .01}, sketch::Vec2{-.01, 0}, sketch::Vec2{0, -.01},
                        sketch::Vec2{.01, 0}})
            move(view, view.screen(at).toPoint());
        click({.01, 0}); // A full circle is not an arc.
        QCOMPARE(created.size(), 0);
        QVERIFY(view.cancelPointerInput());
        QVERIFY(!view.sizingArc());
        click({.01, .01});
        view.setTool("Point");
        QVERIFY(!view.sizingArc());
        QCOMPARE(created.size(), 0);
        view.setTool("Arc");
        click({.01, .01});
        click({.02, .01});
        move(view, view.screen({.01, .02}).toPoint());
        click({.01, .02});
        QCOMPARE(created.size(), 1);
        QVERIFY(std::abs(created[0][0].toDouble() - .01) < 1e-12);
        QVERIFY(std::abs(created[0][1].toDouble() - .01) < 1e-12);
    }
    void pendingArcKeepsWorldAnchors() {
        SketchView view;
        view.resize(600, 600);
        view.showSketch({}, {}, true);
        view.setTool("Arc");
        view.show();
        QTest::qWait(20);
        QSignalSpy created(&view, &SketchView::arcCreated);
        auto click = [&](sketch::Vec2 p) {
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screen(p).toPoint());
        };
        click({0, 0});
        click({.01, 0});
        view.resize(900, 700);
        QTest::qWait(20);
        QPointF at = view.screen({0, 0});
        QNativeGestureEvent pinch(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(),
                                  2, at, at, view.mapToGlobal(at), .25, {});
        QApplication::sendEvent(&view, &pinch);
        move(view, view.screen({0, .01}).toPoint());
        click({0, .01});
        QCOMPARE(created.size(), 1);
        QVERIFY(std::abs(created[0][0].toDouble()) < 1e-12);
        QVERIFY(std::abs(created[0][1].toDouble()) < 1e-12);
        QVERIFY(std::abs(created[0][2].toDouble() - .01) < 1e-12);
        QVERIFY(std::abs(created[0][4].toDouble() - 3.14159265358979323846 / 2) < 1e-12);
    }
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

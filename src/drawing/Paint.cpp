#include "Paint.hpp"
#include "Paths.hpp"
#include "domain/Documents.hpp"
#include <QFont>
#include <QPageLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
namespace cad::drawing {
void paint(QPainter &p, QCborMap sheet) {
    double w = sheet.value("width_mm").toDouble(), h = sheet.value("height_mm").toDouble();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(QRectF(0, 0, w, h), Qt::white);
    p.setPen(QPen(Qt::black, .25));
    p.drawRect(QRectF(5, 5, w - 10, h - 10));
    QFont f("Helvetica");
    f.setPixelSize(3);
    p.setFont(f);
    auto text = [&](double x, double y, QString s) {
        p.save();
        p.translate(x, y);
        p.scale(.1, .1);
        QFont font("Helvetica");
        font.setPixelSize(28);
        p.setFont(font);
        p.drawText(QPointF(0, 0), s);
        p.restore();
    };
    p.setClipRect(QRectF(6, 6, w - 12, h - 18));
    for (auto value : sheet.value("views").toArray()) {
        auto v = value.toMap();
        for (auto key : {"visible", "hidden_paths"}) {
            p.setPen(QPen(key == QString("visible") ? Qt::black : Qt::darkGray,
                          key == QString("visible") ? .25 : .15,
                          key == QString("visible") ? Qt::SolidLine : Qt::DashLine));
            for (auto path : v.value(QString::fromLatin1(key)).toArray()) {
                QPainterPath shape;
                bool first = true;
                for (auto at : points(path)) {
                    if (first)
                        shape.moveTo(at);
                    else
                        shape.lineTo(at);
                    first = false;
                }
                p.drawPath(shape);
            }
        }
        p.setPen(Qt::black);
        if (v.value("status") != "Current")
            text(v.value("x_mm").toDouble(), v.value("y_mm").toDouble() + 8,
                 v.value("status").toString());
    }
    p.setPen(QPen(Qt::black, .18));
    for (auto value : sheet.value("annotations").toArray()) {
        auto a = value.toMap();
        double x = a.value("x_mm").toDouble(), y = a.value("y_mm").toDouble();
        auto anchor = a.value("anchor_mm").toArray();
        auto ends = a.value("dimension_points_mm").toArray();
        if (a.value("kind") == "Linear" && ends.size() == 2) {
            auto first = ends[0].toArray(), last = ends[1].toArray();
            QPointF u(first[0].toDouble(), first[1].toDouble()),
                v(last[0].toDouble(), last[1].toDouble()), d = v - u;
            double len = std::hypot(d.x(), d.y());
            if (len > 1e-8) {
                d /= len;
                QPointF n(-d.y(), d.x()), mid = (u + v) / 2;
                double offset = QPointF::dotProduct(QPointF(x, y) - mid, n);
                QPointF q = u + n * offset, r = v + n * offset;
                p.drawLine(u, q + n * 2);
                p.drawLine(v, r + n * 2);
                p.drawLine(q, r);
                for (auto at : {q, r})
                    p.drawLine(at - d - n, at + d + n);
                x = (q.x() + r.x()) / 2 - a.value("label").toString().size() * .65;
                y = (q.y() + r.y()) / 2 + 3;
            }
        } else if (anchor.size() == 2) {
            QPointF q(anchor[0].toDouble(), anchor[1].toDouble());
            if (a.value("kind") == "Center") {
                p.drawLine(q + QPointF(-3, 0), q + QPointF(3, 0));
                p.drawLine(q + QPointF(0, -3), q + QPointF(0, 3));
            } else {
                p.drawLine(q, {x, y - 1});
                p.drawEllipse(q, .4, .4);
            }
        }
        text(x, y, a.value("label").toString());
    }
    if (sheet.contains(QStringLiteral("bom"))) {
        double y = 25;
        text(w - 115, 18, "BOM - Part ID / Quantity");
        for (auto value : sheet.value("bom").toArray()) {
            auto row = value.toMap();
            text(w - 115, y,
                 row.value("definition").toString() + " / " +
                     QString::number(row.value("quantity").toInteger()));
            y += 5;
        }
    }
    if (sheet.contains(QStringLiteral("bom_error")))
        text(w - 115, 18, "BOM unresolved");
    p.setClipping(false);
    p.drawLine(QPointF(5, h - 15), QPointF(w - 5, h - 15));
    text(9, h - 9,
         sheet.value("name").toString() + " | " + sheet.value("projection").toString() +
             " | Scale " + QString::number(sheet.value("scale").toDouble(), 'g', 6) + " | " +
             sheet.value("units").toString());
    p.restore();
}
void pdf(QCborMap drawing, QIODevice &output) {
    domain::check(drawing.value("current").toBool() && !drawing.value("sheets").toArray().empty(),
                  "StaleResult",
                  "Update all views and repair dangling annotations before PDF export");
    QPdfWriter writer(&output);
    writer.setResolution(2540);
    writer.setCreator("DatumForge");
    writer.setTitle("Engineering drawing");
    QPainter p;
    bool first = true;
    for (auto value : drawing.value("sheets").toArray()) {
        auto sheet = value.toMap();
        QPageSize page(
            QSizeF(sheet.value("width_mm").toDouble(), sheet.value("height_mm").toDouble()),
            QPageSize::Millimeter, "DatumForge sheet", QPageSize::ExactMatch);
        domain::check(
            writer.setPageLayout(QPageLayout(page, QPageLayout::Portrait, QMarginsF(0, 0, 0, 0),
                                             QPageLayout::Millimeter)),
            "StorageError", "PDF page layout failed");
        if (first) {
            domain::check(p.begin(&writer), "StorageError", "Cannot begin vector PDF");
            first = false;
        } else
            domain::check(writer.newPage(), "StorageError", "Cannot add PDF sheet");
        p.save();
        p.scale(100, 100);
        paint(p, sheet);
        p.restore();
    }
    domain::check(p.end(), "StorageError", "Cannot finish vector PDF");
}
} // namespace cad::drawing

#pragma once
#include <QAction>
#include <QHash>
#include <QIcon>
#include <QPainter>
namespace cad {
class CommandRegistry {
    QHash<QString, QAction *> actions;

  public:
    QAction *add(QObject *owner, QString id, QString label, QString shortcut = {},
                 QString unavailable = {}) {
        QPixmap pixmap(48, 48);
        pixmap.setDevicePixelRatio(2);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#34739c"), 1.6));
        // Original vector glyphs, shared by menus, search and command tabs.
        if (id.endsWith("Rectangle"))
            p.drawRect(QRectF(4, 6, 16, 12));
        else if (id.endsWith("Circle"))
            p.drawEllipse(QPointF(12, 12), 8, 8);
        else if (id.endsWith("Arc"))
            p.drawArc(QRectF(4, 4, 16, 16), 20 * 16, 260 * 16);
        else if (id.endsWith("Polyline"))
            p.drawPolyline(QPolygonF{{3, 18}, {8, 5}, {15, 15}, {21, 7}});
        else if (id.endsWith("Line")) {
            p.drawLine(4, 19, 20, 5);
            p.drawEllipse(QPointF(4, 19), 2, 2);
            p.drawEllipse(QPointF(20, 5), 2, 2);
        } else if (id.endsWith("Point")) {
            p.drawLine(12, 4, 12, 20);
            p.drawLine(4, 12, 20, 12);
            p.drawEllipse(QPointF(12, 12), 3, 3);
        } else if (id.contains("Select"))
            p.drawPolygon(QPolygonF{{5, 3}, {19, 13}, {12, 14}, {9, 21}});
        else if (id.contains("finish")) {
            p.drawLine(4, 12, 10, 18);
            p.drawLine(10, 18, 21, 5);
        } else if (id.contains("cancel") || id.contains("delete")) {
            p.drawLine(6, 6, 18, 18);
            p.drawLine(6, 18, 18, 6);
        } else if (id.contains("dimension") || id == "measure") {
            p.drawLine(3, 12, 21, 12);
            p.drawLine(5, 7, 5, 17);
            p.drawLine(19, 7, 19, 17);
            p.drawLine(5, 12, 9, 9);
            p.drawLine(19, 12, 15, 15);
        } else if (id == "undo" || id == "redo") {
            if (id == "redo") {
                p.translate(24, 0);
                p.scale(-1, 1);
            }
            p.drawArc(QRectF(5, 7, 15, 13), 0, 210 * 16);
            p.drawPolyline(QPolygonF{{4, 5}, {4, 12}, {11, 12}});
        } else if (id.startsWith("save")) {
            p.drawRect(4, 4, 16, 16);
            p.drawRect(8, 4, 8, 6);
            p.drawRect(8, 14, 8, 6);
        } else if (id == "open")
            p.drawPolygon(QPolygonF{{3, 7}, {10, 7}, {12, 10}, {21, 10}, {18, 19}, {3, 19}});
        else if (id == "new" || id == "document.new") {
            p.drawRect(5, 3, 13, 18);
            p.drawLine(8, 12, 16, 12);
            p.drawLine(12, 8, 12, 16);
        } else if (id == "fit") {
            p.drawRect(7, 7, 10, 10);
            for (int n = 0; n < 4; ++n) {
                p.save();
                p.translate(12, 12);
                p.rotate(90 * n);
                p.drawPolyline(QPolygonF{{-9, -4}, {-9, -9}, {-4, -9}});
                p.restore();
            }
        } else if (id == "sketch.grid") {
            for (int x = 5; x <= 20; x += 5)
                for (int y = 5; y <= 20; y += 5)
                    p.drawPoint(x, y);
        } else if (id.startsWith("sketch")) {
            p.drawRect(3, 15, 17, 6);
            p.drawPolygon(QPolygonF{{7, 15}, {15, 3}, {19, 6}, {11, 18}, {6, 20}});
        } else {
            p.drawPolygon(QPolygonF{{4, 8}, {12, 4}, {20, 8}, {20, 17}, {12, 21}, {4, 17}});
            p.drawLine(4, 8, 12, 12);
            p.drawLine(12, 12, 20, 8);
            p.drawLine(12, 12, 12, 21);
        }
        p.end();
        QIcon icon(pixmap);
        auto checked = pixmap;
        QPainter tint(&checked);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(checked.rect(), Qt::white);
        tint.end();
        icon.addPixmap(checked, QIcon::Normal, QIcon::On);
        icon.addPixmap(checked, QIcon::Active, QIcon::On);
        auto *a = new QAction(icon, label, owner);
        a->setObjectName(id);
        a->setShortcut(QKeySequence(shortcut));
        a->setToolTip(unavailable.isEmpty() ? label : label + " — " + unavailable);
        a->setStatusTip(a->toolTip());
        a->setEnabled(unavailable.isEmpty());
        a->setProperty("unavailableReason", unavailable);
        actions.insert(id, a);
        return a;
    }
    bool matches(QAction *action, QString query) const {
        QString aliases = action->objectName() + " " + action->property("aliases").toString();
        if (action->objectName() == "saveAs")
            aliases += " duplicate copy";
        if (action->objectName() == "extrude")
            aliases += " boss pad protrusion";
        if (action->objectName() == "assembly")
            aliases += " component mate constraint";
        if (action->objectName() == "drawing.pdf")
            aliases += " print drawing export";
        return (action->text() + " " + aliases).contains(query.trimmed(), Qt::CaseInsensitive);
    }
    QAction *get(const QString &id) const {
        return actions.value(id);
    }
    QList<QAction *> all() const {
        return actions.values();
    }
};
} // namespace cad

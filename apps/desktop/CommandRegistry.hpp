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
        QPixmap pixmap(24, 24);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#34739c"), 1.6));
        // Original geometric glyphs; no reference artwork is bundled.
        p.drawPolygon(QPolygonF({{4, 8}, {12, 4}, {20, 8}, {20, 17}, {12, 21}, {4, 17}}));
        p.drawLine(4, 8, 12, 12);
        p.drawLine(12, 12, 20, 8);
        p.drawLine(12, 12, 12, 21);
        p.end();
        auto *a = new QAction(QIcon(pixmap), label, owner);
        a->setObjectName(id);
        a->setShortcut(QKeySequence(shortcut));
        a->setToolTip(unavailable.isEmpty() ? label : label + " — " + unavailable);
        a->setStatusTip(a->toolTip());
        a->setEnabled(unavailable.isEmpty());
        a->setProperty("unavailableReason", unavailable);
        actions.insert(id, a);
        return a;
    }
    QAction *get(const QString &id) const {
        return actions.value(id);
    }
    QList<QAction *> all() const {
        return actions.values();
    }
};
} // namespace cad

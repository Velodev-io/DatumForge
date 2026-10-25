#pragma once
#include <QCborMap>
class QPainter;
class QIODevice;
namespace cad::drawing {
void paint(QPainter &painter, QCborMap sheet);
void pdf(QCborMap drawing, QIODevice &output);
} // namespace cad::drawing

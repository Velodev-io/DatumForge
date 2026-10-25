#pragma once
#include "features/Model.hpp"
namespace cad::drawing {
struct View {
    QString id = newId(), source, orientation = "Front", parent;
    double x = 30, y = 40;
    qint64 sourceRevision = 0;
    bool hidden = true;
    QCborMap map() const;
    static View parse(QCborMap);
};
struct Annotation {
    QString id = newId(), view, kind = "Text", body, token, secondToken, text, overrideText;
    double x = 30, y = 30;
    QCborMap map() const;
    static Annotation parse(QCborMap);
};
struct Sheet {
    QString id = newId(), name = "Sheet", units = "mm", projection = "third-angle", bomSource;
    qint64 bomRevision = 0;
    double width = 297, height = 210, scale = .5;
    std::vector<View> views;
    std::vector<Annotation> annotations;
    QCborMap map() const;
    static Sheet parse(QCborMap);
};
struct Model {
    std::vector<Sheet> sheets;
    QCborMap map() const;
    static Model parse(QCborMap);
};
} // namespace cad::drawing

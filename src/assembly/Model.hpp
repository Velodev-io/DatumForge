#pragma once
#include "features/Model.hpp"
namespace cad::assembly {
using Vec = sketch::Vec3;
struct Pose {
    Vec translation{};
    std::array<double, 4> rotation{1, 0, 0,
                                   0}; // normalized w,x,y,z; local -> parent; SI translation
    QCborMap map() const;
    static Pose parse(QCborMap map);
};
struct Occurrence {
    QString id = newId(), definition, name = "Component";
    qint64 sourceRevision = 0;
    Pose pose;
    bool grounded = false, hidden = false, suppressed = false, unloaded = false;
    QCborMap map() const;
    static Occurrence parse(QCborMap map);
};
struct Reference {
    QString occurrence, body, token = "Origin";
    QStringList path;
    QCborMap map() const;
    static Reference parse(QCborMap map);
};
struct Mate {
    QString id = newId(), name = "Mate", kind = "Coincident", alignment = "Aligned";
    Reference first, second;
    double value = 0;
    bool suppressed = false;
    QCborMap map() const;
    static Mate parse(QCborMap map);
};
struct Model {
    std::vector<Occurrence> occurrences;
    std::vector<Mate> mates;
    QCborMap map() const;
    static Model parse(QCborMap map);
};
struct Frame {
    QString kind = "Point";
    Vec origin{}, direction{0, 0, 1};
};
using FrameResolver = std::function<Frame(const Reference &)>;
struct Solution {
    Model model;
    bool valid = false;
    int freedom = 0, rank = 0;
    double residual = 0;
    QCborArray diagnostics;
    QCborMap map() const;
};
Solution solve(Model model, const FrameResolver &frames);
using Definitions = QMap<QString, QPair<QString, Model>>;
void validateGraph(const Definitions &definitions);
Pose compose(const Pose &parent, const Pose &child);
Vec transform(const Pose &pose, Vec point, bool direction = false);
} // namespace cad::assembly

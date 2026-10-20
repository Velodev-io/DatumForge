#pragma once
#include "features/Model.hpp"
#include "protocol/Mesh.hpp"
#include <TopoDS_Shape.hxx>
namespace cad::kernel {
struct Body {
    QString id;
    TopoDS_Shape shape;
};
class ResultCache;
struct Part {
    std::vector<Body> bodies;
    QCborArray diagnostics, history;
    bool stale=false;
    QCborArray errors;
    QMap<QString, TopoDS_Shape> stages, tools;
    static Part build(const QCborArray &sketches, const QCborArray &features, ResultCache *cache=nullptr, bool allowFailure=false);
    QCborMap measure(qint64 revision, QString body = {}, QString token = {}, double density = 0,
                     QString secondToken = {}) const;
    DisplayMesh mesh(double deflectionMM = .2) const;
    QCborArray topology(QString body = {}) const;
    QCborMap reference(QString source,QString token,const Part &former)const;
};
QString topologyToken(const TopoDS_Shape &shape);
QCborMap evolution(QString token,QCborArray history,QStringList currentTokens);
struct Effect { std::vector<Body> bodies; TopoDS_Shape stage,tool; QCborArray history; qint64 bytes=0; };
class ResultCache {
    QMap<QByteArray,Effect> entries;QList<QByteArray> order;qint64 used=0,budget;
  public:
    quint64 hits=0,computations=0;
    explicit ResultCache(qint64 bytes=256LL<<20):budget(bytes){domain::check(bytes>=0&&bytes<=256LL<<20,"ResourceLimit","Cache budget exceeds 256 MiB");}
    bool get(QByteArray key,Effect &effect);
    void put(QByteArray key,Effect effect);
    void trim(){entries.clear();order.clear();used=0;}
    QCborMap statistics()const;
};
} // namespace cad::kernel

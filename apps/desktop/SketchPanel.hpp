#pragma once
#include "protocol/Client.hpp"
#include "render/SketchView.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
namespace cad {
class SketchPanel : public QWidget {
    Q_OBJECT
    Client *client;
    SketchView *view;
    QComboBox *sketches, *plane, *tool, *relation;
    QLineEdit *x, *y, *radius, *start, *sweep, *value, *nameExpression;
    QCheckBox *construction;
    QListWidget *entities, *constraints;
    QLabel *context;
    QPushButton *preview, *accept, *cancel, *edit, *create, *add, *addConstraint, *removeConstraint,
        *setDimension, *setName;
    std::vector<sketch::Sketch> accepted, draft;
    QString document, selectedId;
    qint64 revision = -1;
    bool editing = false, syncing = false, committing = false;
    std::vector<sketch::Vec2> clicks;
    sketch::Sketch &current();
    void rebuild();
    void addAt(sketch::Vec2 p, bool snapInput = false);
    void previewDraft();
    void guard(std::function<void()> fn);
    void addRelation();
    void updateRelations();

  public:
    explicit SketchPanel(Client *client, QWidget *parent = nullptr);
    void install(QCborMap document, bool idle);
    bool active() const {
        return editing;
    }
    void begin();
    void discard();
  signals:
    void editingChanged();
};
} // namespace cad

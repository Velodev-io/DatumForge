#pragma once
#include "assembly/Model.hpp"
#include "protocol/Client.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>
namespace cad {
class AssemblyPanel : public QWidget {
    Q_OBJECT
    Client *client;
    QCborMap installed;
    assembly::Model accepted, draft;
    bool editing = false, sync = false;
    QString pendingReference;
    QComboBox *definition, *occurrence, *mateList, *mateKind, *alignment, *firstOccurrence,
        *secondOccurrence, *firstReference, *secondReference;
    QLineEdit *name;
    QDoubleSpinBox *position[3], *angles[3], *mateValue;
    QCheckBox *grounded, *hidden, *suppressed, *unloaded;
    QLabel *diagnostics, *states;
    QPushButton *acceptButton, *previewButton, *cancelButton;
    void begin();
    void chooseOccurrence();
    void chooseMate();
    void updateLists();
    void preview();
    void readReferences();

  public:
    explicit AssemblyPanel(Client *client);
    void install(QCborMap document, QCborArray documents, bool idle);
    bool active() const {
        return editing;
    }
    void discard();
    void drag(QString occurrence, QVector3D deltaMM);
  signals:
    void editingChanged();
};
} // namespace cad

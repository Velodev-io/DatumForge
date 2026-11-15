#pragma once
#include "protocol/Client.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
namespace cad {
class HistoryPanel : public QWidget {
    Q_OBJECT
    Client *client;
    QCborMap document, resolution;
    QCborArray candidate;
    QComboBox *feature, *action, *parameter, *candidateList;
    QLineEdit *name, *expression;
    QCheckBox *suppress, *cascade, *acknowledge;
    QSpinBox *position;
    QLabel *diagnostic;
    QPushButton *accept, *repair, *previewButton, *cancelButton;
    QWidget *inputs;
    bool editing = false, pending = false, previewReady = false, proposalCurrent = false;
    QCborArray profiles() const;
    void invalidate();

  public:
    explicit HistoryPanel(Client *client);
    bool active() const {
        return editing;
    }
    void begin(QCborMap document, QString feature = {});
    void preview();
    void finish();
    void discard();
    void inspectReference();
    void replaceReference();
  signals:
    void editingChanged();
};
} // namespace cad

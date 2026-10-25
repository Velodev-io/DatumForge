#pragma once
#include "drawing/Model.hpp"
#include "protocol/Client.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
namespace cad {
class DrawingCanvas : public QWidget {
    Q_OBJECT
    QCborMap sheet;

  public:
    explicit DrawingCanvas(QWidget *parent = nullptr);
    void install(QCborMap value);

  protected:
    void paintEvent(QPaintEvent *) override;
};
class DrawingPanel : public QWidget {
    Q_OBJECT
    Client *client;
    drawing::Model draft;
    QString doc;
    bool editing = false, syncing = false, submitting = false;
    qint64 submittedRevision = 0;
    QComboBox *sheets, *source, *orientation, *projection, *units, *views, *kind, *anchors, *second,
        *annotationRows;
    QCheckBox *hiddenLines, *projected;
    QLineEdit *name, *text, *overrideText;
    QDoubleSpinBox *width, *height, *scale, *x, *y;
    QLabel *message;
    QPushButton *beginButton, *applyButton, *cancelButton, *previewButton;
    DrawingCanvas *canvas;
    QCborMap latest;
    void populate();
    void saveSheet();
    drawing::Sheet *sheet();

  public:
    DrawingPanel(Client *, DrawingCanvas *);
    void install(QCborMap active, QCborArray documents, bool idle);
    bool active() const {
        return editing;
    }
    void begin();
    void discard();
  signals:
    void editingChanged();
};
} // namespace cad

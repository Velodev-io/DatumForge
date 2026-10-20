#pragma once
#include "features/Model.hpp"
#include "protocol/Client.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>
namespace cad {
class FeaturePanel : public QWidget {
    Q_OBJECT
    Client *client;
    QCborMap document, result;
    features::Feature draft;
    QCborArray candidate;
    bool editing = false, validPreview = false, installing = false, commitPending = false;
    QFormLayout *form;
    QWidget *inputs, *evaluation;
    QLineEdit *name;
    QComboBox *profile, *operation, *target, *source, *extent, *distribution;
    QDoubleSpinBox *size, *depth, *angle, *spacing, *density;
    QSpinBox *count;
    QCheckBox *through;
    std::array<QDoubleSpinBox *, 3> origin, axis;
    QLabel *title, *diagnostic, *measurement;
    QPushButton *accept, *previewButton, *cancelButton;
    void fieldsForKind();
    QCborArray definitions() const;
    double unit() const;
    void invalidate();

  public:
    explicit FeaturePanel(Client *client);
    void install(QCborMap active, bool idle);
    void begin(QString kind, QString editId = {});
    void discard();
    bool active() const {
        return editing;
    }
    bool hasSolid() const {
        return !document.value("features").toArray().empty();
    }
    void preview();
    void finish();
    QCborMap latestResult() const {
        return result;
    }
  signals:
    void editingChanged();
};
} // namespace cad

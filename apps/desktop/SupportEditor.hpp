#pragma once
#include "model/SupportEditing.hpp"
#include "protocol/Client.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
namespace cad {
class SupportEditor : public QDialog {
    Q_OBJECT
    Client *client;
    QCborMap document;
    model::SupportDefinition draft;
    QString epoch, project, documentId, recordId, newSketchId;
    qint64 revision;
    bool loading = false, submitting = false, retired = false, cancelPending = false,
         removalStaged = false;
    QString pending;
    QCborMap submitted, previewed, previewResult, inspection, inspectionModel;
    bool repairSelected = false;
    int inspectedSlot = -1;
    QString inspectedOwner;
    QWidget *inputs = nullptr;
    QFormLayout *form;
    QComboBox *kind, *item, *mode, *principal, *target, *bisector;
    std::array<QComboBox *, 3> references, endpoints;
    std::array<QDoubleSpinBox *, 3> parameters;
    std::array<QWidget *, 3> referenceRows;
    std::array<QPushButton *, 3> inspectButtons;
    QDoubleSpinBox *distance, *angle, *scale, *centerX, *centerY, *translateX, *translateY, *u, *v;
    QSpinBox *count;
    QCheckBox *reverse, *construction;
    QListWidget *sources;
    QPushButton *addSource, *removeSource, *stage, *preview, *apply = nullptr, *cancel, *remove;
    QComboBox *replacements = nullptr;
    QLabel *status;
    QPlainTextEdit *summary;
    double unit() const;
    bool current() const;
    void invalidate();
    void refreshState();
    void records(QString selected = {});
    void load();
    void updateFields();
    QStringList roles(int slot) const;
    domain::GeometryReference reference(int slot) const;
    void referenceChoices(int slot, std::optional<domain::GeometryReference> selected = {});
    model::SupportDefinition candidate() const;
    void inspect(int slot);
    void requestPreview();
    void commit();
    void stageChange();

  public:
    SupportEditor(Client *client, QCborMap document, QWidget *parent = nullptr,
                  QString selected = {}, std::optional<model::SupportDefinition> initialDraft = {});
    void reject() override;
};
} // namespace cad

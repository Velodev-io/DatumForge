#pragma once
#include "protocol/Client.hpp"
#include "render/SketchView.hpp"
#include "sketch/Editing.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
namespace cad {
class SketchPanel : public QWidget {
    Q_OBJECT
    Client *client;
    QWidget *propertyPane;
    QFormLayout *rootForm;
    SketchView *view;
    QComboBox *sketches, *plane, *tool, *relation;
    QComboBox *arcMethod;
    QComboBox *rectangleMethod;
    QComboBox *slotMethod, *slotLengthMode;
    QCheckBox *slotAddDimensions;
    QSpinBox *splineDegree, *selectedCurveDegree;
    QCheckBox *splineClosed;
    QPushButton *finishSpline;
    QLineEdit *selectedWeights, *selectedKnots, *selectedMultiplicities;
    QLineEdit *contactParameterA, *contactParameterB;
    QComboBox *pierceSource, *pierceBranch;
    QLineEdit *pierceLower, *pierceUpper;
    QPushButton *repairPierce;
    void refreshPierceSources();
    sketch::CurveReference pierceReference() const;
    QPlainTextEdit *selectedControls;
    QLineEdit *x, *y, *radius, *start, *sweep, *value, *nameExpression;
    QCheckBox *construction;
    QListWidget *entities, *constraints;
    QLabel *context, *selectionTitle;
    QGroupBox *selectionGroup;
    QLineEdit *selectedX, *selectedY, *selectedRadius, *selectedWidth, *selectedHeight,
        *selectedStart, *selectedSweep;
    QLineEdit *selectedRotation, *selectedSideAngle;
    QPushButton *applySelection, *removeEntity;
    QGroupBox *lineEditGroup;
    QComboBox *lineEditMode, *lineEditSource;
    QListWidget *lineEditBoundaries, *lineEditDisclosure;
    QLineEdit *lineEditX, *lineEditY;
    QCheckBox *lineEditConsent;
    QPushButton *lineEditPreview, *lineEditApply, *lineEditCancel;
    QCborArray lineEditBefore, lineEditCandidate;
    void clearLineEdit();
    void requestLineEdit();
    void refreshLineEdit();
    std::optional<sketch::Sketch> gestureBefore;
    sketch::Vec2 gestureOrigin;
    QString gestureError;
    std::vector<sketch::Sketch> draftUndo, draftRedo;
    QPushButton *preview, *accept, *cancel, *edit, *create, *add, *addConstraint, *removeConstraint,
        *setDimension, *setName;
    std::vector<sketch::Sketch> accepted, draft;
    QString document, selectedId;
    QStringList relationSelectionOrder;
    QString resumeLineTool;
    std::optional<std::pair<QString, int>> pendingArcJoin;
    bool lineContinuationAnchor = false;
    qint64 revision = -1;
    bool editing = false, syncing = false, committing = false;
    bool dimensionEditing = false;
    void openDimensionEditor(QString constraint = {});
    std::vector<sketch::Vec2> clicks;
    sketch::Sketch &current();
    void rebuild();
    void addAt(sketch::Vec2 p, bool snapInput = false);
    void previewDraft();
    void finishSplineInput();
    void guard(std::function<void()> fn);
    void addRelation();
    void updateRelations();
    void cancelInput();
    void updateToolFields();
    void updateSelection();
    void selectEntity(QString id);
    void applySelected();
    void deleteSelected();
    void remember(sketch::Sketch before);

  public:
    explicit SketchPanel(Client *client, QWidget *parent = nullptr);
    void install(QCborMap document, bool idle);
    bool active() const {
        return editing;
    }
    void begin();
    void discard();
    void editSelected(QString entity = {});
    void setTool(QString name);
    void undoDraftEdit();
    void redoDraftEdit();
    bool canUndoDraft() const {
        return !draftUndo.empty();
    }
    bool canRedoDraft() const {
        return !draftRedo.empty();
    }
    SketchView *canvas() const {
        return view;
    }
    QWidget *propertyManager() const {
        return propertyPane;
    }
    bool dimensionAvailable() const;
    QString selectedEntity() const {
        return selectedId;
    }
    QString sketchId() const {
        return sketches->currentData().toString();
    }
    QString sketchName() const {
        return sketches->currentText();
    }
    void chooseSketch(QString id);
    void choosePlane(QString name);
    void dimensionSelection();
    void removeSelection() {
        guard([&] { deleteSelected(); });
    }
    void finish() {
        accept->click();
    }
    QString toolName() const {
        return tool->currentText();
    }
  signals:
    void editingChanged();
    void toolChanged(QString tool);
    void selectionChanged();
};
} // namespace cad

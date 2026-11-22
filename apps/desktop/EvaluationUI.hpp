#pragma once

#include "MainWindow.hpp"
#include "features/Catalogs.hpp"
#include "protocol/Client.hpp"
#include "render/Viewport.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <vector>

namespace cad {

class MaterialDialog : public QDialog {
    Q_OBJECT
    MainWindow *window = nullptr;
    Client *client = nullptr;
    QLineEdit *materialSearch = nullptr;
    QTreeWidget *materialTree = nullptr;
    QComboBox *materialScope = nullptr;
    QLabel *densityLabel = nullptr;
    QLabel *modulusLabel = nullptr;
    QLabel *poissonLabel = nullptr;
    QLabel *shearLabel = nullptr;
    QLabel *thermalLabel = nullptr;
    QLabel *specificHeatLabel = nullptr;
    QLabel *yieldLabel = nullptr;
    QLabel *tensileLabel = nullptr;
    QPushButton *favoriteButton = nullptr;
    QPushButton *applyButton = nullptr;
    QPushButton *customButton = nullptr;
    QSet<QString> favorites;
    std::vector<features::Material> customMaterials;
    std::optional<features::Material> currentMaterial;

  public:
    explicit MaterialDialog(MainWindow *win, Client *cli, QWidget *parent = nullptr);
    void populateTree(const QString &filter = {});
    void selectMaterial(const QString &id);
    void applyMaterial();
    void toggleFavorite();
    void addCustomMaterial();
};

class MassPropertiesDialog : public QDialog {
    Q_OBJECT
    MainWindow *window = nullptr;
    Client *client = nullptr;
    QComboBox *scopeCombo = nullptr;
    QComboBox *frameCombo = nullptr;
    QPushButton *recalculateButton = nullptr;
    QLabel *massValueLabel = nullptr;
    QLabel *volumeValueLabel = nullptr;
    QLabel *surfaceAreaValueLabel = nullptr;
    QLabel *centroidValueLabel = nullptr;
    QLabel *momentsValueLabel = nullptr;
    QLabel *principalMomentsValueLabel = nullptr;
    QLabel *statusLabel = nullptr;

  public:
    explicit MassPropertiesDialog(MainWindow *win, Client *cli, QWidget *parent = nullptr);
    void calculate();
    void refreshScopes();
};

class SectionPropertiesDialog : public QDialog {
    Q_OBJECT
    MainWindow *window = nullptr;
    Client *client = nullptr;
    QLabel *faceLabel = nullptr;
    QPushButton *recalculateButton = nullptr;
    QLabel *areaValueLabel = nullptr;
    QLabel *centroidValueLabel = nullptr;
    QLabel *momentsValueLabel = nullptr;
    QLabel *polarMomentValueLabel = nullptr;
    QLabel *statusLabel = nullptr;

  public:
    explicit SectionPropertiesDialog(MainWindow *win, Client *cli, QWidget *parent = nullptr);
    void calculate();
};

class SectionViewDialog : public QDialog {
    Q_OBJECT
    MainWindow *window = nullptr;
    Client *client = nullptr;
    Viewport *viewport = nullptr;
    QListWidget *sectionList = nullptr;
    QLineEdit *nameEdit = nullptr;
    QCheckBox *activeCheck = nullptr;
    QComboBox *supportCombo = nullptr;
    QDoubleSpinBox *offsetSpin = nullptr;
    QCheckBox *flipCheck = nullptr;
    QPushButton *applyButton = nullptr;
    QPushButton *newButton = nullptr;
    QPushButton *deleteButton = nullptr;
    bool initialClippingActive = false;

  public:
    explicit SectionViewDialog(MainWindow *win, Client *cli, Viewport *vp,
                               QWidget *parent = nullptr);
    void populateSections();
    void updatePreview();
    void apply();
    void createNew();
    void removeCurrent();

  protected:
    void reject() override;
};

} // namespace cad

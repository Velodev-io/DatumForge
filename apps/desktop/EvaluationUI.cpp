#include "EvaluationUI.hpp"
#include "features/Catalogs.hpp"
#include "model/Supports.hpp"
#include "render/Viewport.hpp"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>
#include <numbers>

namespace cad {
using sketch::fields;

// ============================================================================
// MaterialDialog
// ============================================================================

MaterialDialog::MaterialDialog(MainWindow *win, Client *cli, QWidget *parent)
    : QDialog(parent ? parent : win), window(win), client(cli) {
    setObjectName("materialDialog");
    setWindowTitle("Material Library");
    resize(700, 500);

    auto *mainLayout = new QVBoxLayout(this);

    // Search bar
    materialSearch = new QLineEdit(this);
    materialSearch->setObjectName("materialSearch");
    materialSearch->setPlaceholderText("Search materials (e.g. Steel, Aluminum, 1060)...");
    mainLayout->addWidget(materialSearch);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Material Tree
    materialTree = new QTreeWidget(this);
    materialTree->setObjectName("materialTree");
    materialTree->setHeaderLabel("Materials");
    splitter->addWidget(materialTree);

    // Right: Properties Panel
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);

    auto *scopeLayout = new QHBoxLayout;
    scopeLayout->addWidget(new QLabel("Apply to:", rightWidget));
    materialScope = new QComboBox(rightWidget);
    materialScope->setObjectName("materialScope");
    materialScope->addItem("Entire Part");
    scopeLayout->addWidget(materialScope, 1);
    rightLayout->addLayout(scopeLayout);

    auto *formGroup = new QGroupBox("Physical Properties", rightWidget);
    auto *form = new QFormLayout(formGroup);

    densityLabel = new QLabel("—", formGroup);
    densityLabel->setObjectName("materialDensityLabel");
    form->addRow("Density:", densityLabel);

    modulusLabel = new QLabel("—", formGroup);
    modulusLabel->setObjectName("materialModulusLabel");
    form->addRow("Elastic Modulus:", modulusLabel);

    poissonLabel = new QLabel("—", formGroup);
    poissonLabel->setObjectName("materialPoissonLabel");
    form->addRow("Poisson's Ratio:", poissonLabel);

    shearLabel = new QLabel("—", formGroup);
    shearLabel->setObjectName("materialShearLabel");
    form->addRow("Shear Modulus:", shearLabel);

    thermalLabel = new QLabel("—", formGroup);
    thermalLabel->setObjectName("materialThermalLabel");
    form->addRow("Thermal Conductivity:", thermalLabel);

    specificHeatLabel = new QLabel("—", formGroup);
    specificHeatLabel->setObjectName("materialSpecificHeatLabel");
    form->addRow("Specific Heat:", specificHeatLabel);

    yieldLabel = new QLabel("—", formGroup);
    yieldLabel->setObjectName("materialYieldLabel");
    form->addRow("Yield Strength:", yieldLabel);

    tensileLabel = new QLabel("—", formGroup);
    tensileLabel->setObjectName("materialTensileLabel");
    form->addRow("Tensile Strength:", tensileLabel);

    rightLayout->addWidget(formGroup);

    auto *btnRow = new QHBoxLayout;
    favoriteButton = new QPushButton("Add to Favorites", rightWidget);
    favoriteButton->setObjectName("materialFavoriteButton");
    favoriteButton->setEnabled(false);
    btnRow->addWidget(favoriteButton);

    customButton = new QPushButton("Add Custom Material...", rightWidget);
    customButton->setObjectName("addCustomMaterialButton");
    btnRow->addWidget(customButton);

    rightLayout->addLayout(btnRow);
    rightLayout->addStretch(1);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);

    // Bottom action buttons
    auto *actions = new QHBoxLayout;
    applyButton = new QPushButton("Apply", this);
    applyButton->setObjectName("materialApplyButton");
    applyButton->setEnabled(false);
    actions->addWidget(applyButton);

    auto *closeBtn = new QPushButton("Close", this);
    closeBtn->setObjectName("materialCloseButton");
    actions->addWidget(closeBtn);

    mainLayout->addLayout(actions);

    connect(materialSearch, &QLineEdit::textChanged, this, &MaterialDialog::populateTree);
    connect(materialTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current) {
        if (!current || current->data(0, Qt::UserRole).isNull()) {
            densityLabel->setText("—");
            modulusLabel->setText("—");
            poissonLabel->setText("—");
            shearLabel->setText("—");
            thermalLabel->setText("—");
            specificHeatLabel->setText("—");
            yieldLabel->setText("—");
            tensileLabel->setText("—");
            favoriteButton->setEnabled(false);
            applyButton->setEnabled(false);
            currentMaterial.reset();
            return;
        }
        selectMaterial(current->data(0, Qt::UserRole).toString());
    });
    connect(favoriteButton, &QPushButton::clicked, this, &MaterialDialog::toggleFavorite);
    connect(customButton, &QPushButton::clicked, this, &MaterialDialog::addCustomMaterial);
    connect(applyButton, &QPushButton::clicked, this, &MaterialDialog::applyMaterial);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    favorites.insert("steel_1018");
    favorites.insert("aluminum_6061_t6");
    populateTree();
}

void MaterialDialog::populateTree(const QString &filter) {
    QSignalBlocker b(materialTree);
    materialTree->clear();

    auto *favGroup = new QTreeWidgetItem(materialTree, QStringList{"Favorites"});
    favGroup->setExpanded(true);

    QMap<QString, QTreeWidgetItem *> families;
    for (const auto &fam : features::availableMaterialFamilies()) {
        auto *item = new QTreeWidgetItem(materialTree, QStringList{fam});
        item->setExpanded(!filter.isEmpty());
        families[fam] = item;
    }

    auto addMatItem = [&](const features::Material &mat, bool isFav) {
        if (!filter.isEmpty() && !mat.name.contains(filter, Qt::CaseInsensitive) &&
            !mat.family.contains(filter, Qt::CaseInsensitive))
            return;

        auto *item = new QTreeWidgetItem(QStringList{mat.name});
        item->setData(0, Qt::UserRole, mat.id);

        if (isFav) {
            favGroup->addChild(item);
        } else if (families.contains(mat.family)) {
            families[mat.family]->addChild(item);
        }
    };

    // Standard materials
    for (const auto &mat : features::standardMaterials()) {
        if (favorites.contains(mat.id))
            addMatItem(mat, true);
        addMatItem(mat, false);
    }

    // Custom materials
    for (const auto &mat : customMaterials) {
        if (favorites.contains(mat.id))
            addMatItem(mat, true);
        addMatItem(mat, false);
    }

    // Remove empty groups if filtering
    if (!filter.isEmpty()) {
        if (favGroup->childCount() == 0)
            delete favGroup;
        for (auto *grp : families.values())
            if (grp->childCount() == 0)
                delete grp;
    }
}

void MaterialDialog::selectMaterial(const QString &id) {
    currentMaterial = features::findMaterial(id);
    if (!currentMaterial) {
        for (const auto &m : customMaterials) {
            if (m.id == id) {
                currentMaterial = m;
                break;
            }
        }
    }
    if (!currentMaterial)
        return;

    const auto &p = currentMaterial->properties;
    densityLabel->setText(QString::number(p.density_kg_m3, 'f', 1) + " kg/m³");
    modulusLabel->setText(p.elastic_modulus_pa.has_value()
                              ? QString::number(*p.elastic_modulus_pa / 1e9, 'f', 1) + " GPa"
                              : "—");
    poissonLabel->setText(p.poissons_ratio.has_value()
                              ? QString::number(*p.poissons_ratio, 'f', 2)
                              : "—");
    shearLabel->setText(p.shear_modulus_pa.has_value()
                            ? QString::number(*p.shear_modulus_pa / 1e9, 'f', 1) + " GPa"
                            : "—");
    thermalLabel->setText(p.thermal_conductivity_w_mk.has_value()
                              ? QString::number(*p.thermal_conductivity_w_mk, 'f', 1) + " W/(m·K)"
                              : "—");
    specificHeatLabel->setText(p.specific_heat_j_kgk.has_value()
                                   ? QString::number(*p.specific_heat_j_kgk, 'f', 0) + " J/(kg·K)"
                                   : "—");
    yieldLabel->setText(p.yield_strength_pa.has_value()
                            ? QString::number(*p.yield_strength_pa / 1e6, 'f', 1) + " MPa"
                            : "—");
    tensileLabel->setText(p.tensile_strength_pa.has_value()
                              ? QString::number(*p.tensile_strength_pa / 1e6, 'f', 1) + " MPa"
                              : "—");

    favoriteButton->setEnabled(true);
    favoriteButton->setText(favorites.contains(id) ? "Remove from Favorites" : "Add to Favorites");
    applyButton->setEnabled(true);
}

void MaterialDialog::toggleFavorite() {
    if (!currentMaterial)
        return;
    auto matId = currentMaterial->id;
    if (favorites.contains(matId))
        favorites.remove(matId);
    else
        favorites.insert(matId);
    populateTree(materialSearch->text());
    selectMaterial(matId);
    favoriteButton->setText(favorites.contains(matId) ? "Remove from Favorites" : "Add to Favorites");
}

void MaterialDialog::addCustomMaterial() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Custom Material", "Material Name:", QLineEdit::Normal, "Custom Alloy", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    double density = QInputDialog::getDouble(this, "Custom Material Density", "Density (kg/m³):", 7800.0, 1.0, 50000.0, 1, &ok);
    if (!ok)
        return;

    features::Material m;
    m.id = "custom_" + QString::number(customMaterials.size() + 1);
    m.name = name.trimmed();
    m.family = "Custom";
    m.properties.density_kg_m3 = density;
    customMaterials.push_back(m);
    populateTree(materialSearch->text());
}

void MaterialDialog::applyMaterial() {
    if (!currentMaterial || !client || !window)
        return;

    auto activeDoc = window->state().activeDocument;
    if (activeDoc.isEmpty())
        return;

    // Update document model support with material definition
    QCborMap activeRecord;
    for (auto val : window->state().documentRecords) {
        auto d = val.toMap();
        if (d.value("id").toString() == activeDoc)
            activeRecord = d;
    }

    auto supMap = activeRecord.contains(QStringLiteral("model_supports"))
                      ? activeRecord.value("model_supports").toMap()
                      : activeRecord.value("supports").toMap();
    auto model = model::SupportDefinition::parse(supMap);
    model.document = activeDoc;
    model.material = currentMaterial->map();

    client->projectCommand("SetModelSupports", fields({{"model", model.map()}}));
    accept();
}

// ============================================================================
// MassPropertiesDialog
// ============================================================================

MassPropertiesDialog::MassPropertiesDialog(MainWindow *win, Client *cli, QWidget *parent)
    : QDialog(parent ? parent : win), window(win), client(cli) {
    setObjectName("massPropertiesDialog");
    setWindowTitle("Mass Properties");
    resize(550, 420);

    auto *mainLayout = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel("Scope:", this));
    scopeCombo = new QComboBox(this);
    scopeCombo->setObjectName("massScopeCombo");
    scopeCombo->addItem("Entire Part");
    topRow->addWidget(scopeCombo, 1);

    topRow->addWidget(new QLabel("Reference Frame:", this));
    frameCombo = new QComboBox(this);
    frameCombo->setObjectName("massFrameCombo");
    frameCombo->addItems({"Document coordinate system", "Center of mass"});
    topRow->addWidget(frameCombo, 1);

    recalculateButton = new QPushButton("Recalculate", this);
    recalculateButton->setObjectName("massRecalculateButton");
    topRow->addWidget(recalculateButton);

    mainLayout->addLayout(topRow);

    auto *resultsGroup = new QGroupBox("Computed Properties", this);
    auto *form = new QFormLayout(resultsGroup);

    massValueLabel = new QLabel("—", resultsGroup);
    massValueLabel->setObjectName("massValueLabel");
    form->addRow("Mass:", massValueLabel);

    volumeValueLabel = new QLabel("—", resultsGroup);
    volumeValueLabel->setObjectName("volumeValueLabel");
    form->addRow("Volume:", volumeValueLabel);

    surfaceAreaValueLabel = new QLabel("—", resultsGroup);
    surfaceAreaValueLabel->setObjectName("surfaceAreaValueLabel");
    form->addRow("Surface Area:", surfaceAreaValueLabel);

    centroidValueLabel = new QLabel("—", resultsGroup);
    centroidValueLabel->setObjectName("centroidValueLabel");
    form->addRow("Center of Mass:", centroidValueLabel);

    momentsValueLabel = new QLabel("—", resultsGroup);
    momentsValueLabel->setObjectName("momentsValueLabel");
    form->addRow("Moments of Inertia:", momentsValueLabel);

    principalMomentsValueLabel = new QLabel("—", resultsGroup);
    principalMomentsValueLabel->setObjectName("principalMomentsValueLabel");
    form->addRow("Principal Moments:", principalMomentsValueLabel);

    mainLayout->addWidget(resultsGroup);

    statusLabel = new QLabel("Ready", this);
    statusLabel->setObjectName("massStatusLabel");
    mainLayout->addWidget(statusLabel);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(recalculateButton, &QPushButton::clicked, this, &MassPropertiesDialog::calculate);

    connect(client, &Client::partResult, this, [this](QCborMap data) {
        if (!isVisible())
            return;
        if (data.contains(QStringLiteral("mass_properties"))) {
            auto res = features::MassPropertiesResult::parse(data.value("mass_properties").toMap());
            if (!res.valid) {
                statusLabel->setText("Evaluation failed: " + res.reason);
                return;
            }
            massValueLabel->setText(res.mass_kg.has_value()
                                        ? QString::number(*res.mass_kg, 'g', 6) + " kg"
                                        : "— (density missing)");
            volumeValueLabel->setText(QString::number(res.volume_m3 * 1e9, 'g', 6) + " mm³ (" +
                                      QString::number(res.volume_m3, 'g', 6) + " m³)");
            surfaceAreaValueLabel->setText(QString::number(res.surface_area_m2 * 1e6, 'g', 6) + " mm² (" +
                                           QString::number(res.surface_area_m2, 'g', 6) + " m²)");
            centroidValueLabel->setText(QString("X = %1 mm, Y = %2 mm, Z = %3 mm")
                                            .arg(res.center_of_mass_m[0] * 1000.0, 0, 'f', 3)
                                            .arg(res.center_of_mass_m[1] * 1000.0, 0, 'f', 3)
                                            .arg(res.center_of_mass_m[2] * 1000.0, 0, 'f', 3));
            momentsValueLabel->setText(QString("Ixx = %1, Iyy = %2, Izz = %3\nIxy = %4, Ixz = %5, Iyz = %6 kg·m²")
                                           .arg(res.inertia_tensor_kg_m2[0], 0, 'e', 4)
                                           .arg(res.inertia_tensor_kg_m2[1], 0, 'e', 4)
                                           .arg(res.inertia_tensor_kg_m2[2], 0, 'e', 4)
                                           .arg(res.inertia_tensor_kg_m2[3], 0, 'e', 4)
                                           .arg(res.inertia_tensor_kg_m2[4], 0, 'e', 4)
                                           .arg(res.inertia_tensor_kg_m2[5], 0, 'e', 4));
            principalMomentsValueLabel->setText(QString("Ix = %1, Iy = %2, Iz = %3 kg·m²")
                                                    .arg(res.principal_moments_kg_m2[0], 0, 'e', 4)
                                                    .arg(res.principal_moments_kg_m2[1], 0, 'e', 4)
                                                    .arg(res.principal_moments_kg_m2[2], 0, 'e', 4));
            statusLabel->setText(QString("Evaluated %1 solid bodies successfully").arg(res.solids_count));
        }
    });
}

void MassPropertiesDialog::refreshScopes() {
    scopeCombo->clear();
    scopeCombo->addItem("Entire Part");
    if (client && client->mesh()) {
        for (const auto &inst : client->mesh()->instances) {
            scopeCombo->addItem("Body " + inst.id, inst.id);
        }
    }
}

void MassPropertiesDialog::calculate() {
    if (!client || !window)
        return;
    auto doc = window->state().activeDocument;
    if (doc.isEmpty()) {
        statusLabel->setText("No active document");
        return;
    }

    features::MassPropertiesRequest req;
    req.revision = window->state().revision;
    req.scopeType = scopeCombo->currentIndex() == 0 ? "Part" : "Body";
    req.targetId = scopeCombo->currentData().toString();
    req.frame = frameCombo->currentIndex() == 0 ? "Part" : "CenterOfMass";

    client->projectCommand("EvaluateMassProperties", req.map());
    statusLabel->setText("Computing exact mass properties in worker...");
}

// ============================================================================
// SectionPropertiesDialog
// ============================================================================

SectionPropertiesDialog::SectionPropertiesDialog(MainWindow *win, Client *cli, QWidget *parent)
    : QDialog(parent ? parent : win), window(win), client(cli) {
    setObjectName("sectionPropertiesDialog");
    setWindowTitle("Section Properties");
    resize(500, 380);

    auto *mainLayout = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout;
    faceLabel = new QLabel("Selected Face: (None)", this);
    faceLabel->setObjectName("sectionFaceLabel");
    topRow->addWidget(faceLabel, 1);

    recalculateButton = new QPushButton("Evaluate Selection", this);
    recalculateButton->setObjectName("sectionRecalculateButton");
    topRow->addWidget(recalculateButton);

    mainLayout->addLayout(topRow);

    auto *resultsGroup = new QGroupBox("Planar Cross-Section Properties", this);
    auto *form = new QFormLayout(resultsGroup);

    areaValueLabel = new QLabel("—", resultsGroup);
    areaValueLabel->setObjectName("sectionAreaValueLabel");
    form->addRow("Area:", areaValueLabel);

    centroidValueLabel = new QLabel("—", resultsGroup);
    centroidValueLabel->setObjectName("sectionCentroidValueLabel");
    form->addRow("Centroid:", centroidValueLabel);

    momentsValueLabel = new QLabel("—", resultsGroup);
    momentsValueLabel->setObjectName("sectionMomentsValueLabel");
    form->addRow("Moments of Inertia:", momentsValueLabel);

    polarMomentValueLabel = new QLabel("—", resultsGroup);
    polarMomentValueLabel->setObjectName("sectionPolarMomentValueLabel");
    form->addRow("Polar Moment (J):", polarMomentValueLabel);

    mainLayout->addWidget(resultsGroup);

    statusLabel = new QLabel("Select a planar face in the 3D model, then Evaluate.", this);
    statusLabel->setObjectName("sectionStatusLabel");
    mainLayout->addWidget(statusLabel);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(recalculateButton, &QPushButton::clicked, this, &SectionPropertiesDialog::calculate);

    connect(client, &Client::partResult, this, [this](QCborMap data) {
        if (!isVisible())
            return;
        if (data.contains(QStringLiteral("section_properties"))) {
            auto res = features::SectionPropertiesResult::parse(data.value("section_properties").toMap());
            if (!res.valid) {
                statusLabel->setText("Evaluation failed: " + res.reason);
                return;
            }
            areaValueLabel->setText(QString::number(res.area_m2 * 1e6, 'g', 6) + " mm² (" +
                                    QString::number(res.area_m2, 'g', 6) + " m²)");
            centroidValueLabel->setText(QString("Local: u = %1 mm, v = %2 mm\nPart: X = %3 mm, Y = %4 mm, Z = %5 mm")
                                            .arg(res.centroid_local_m[0] * 1000.0, 0, 'f', 3)
                                            .arg(res.centroid_local_m[1] * 1000.0, 0, 'f', 3)
                                            .arg(res.centroid_part_m[0] * 1000.0, 0, 'f', 3)
                                            .arg(res.centroid_part_m[1] * 1000.0, 0, 'f', 3)
                                            .arg(res.centroid_part_m[2] * 1000.0, 0, 'f', 3));
            momentsValueLabel->setText(QString("Iuu = %1, Ivv = %2, Iuv = %3 mm⁴")
                                           .arg(res.second_moments_m4[0] * 1e12, 0, 'e', 4)
                                           .arg(res.second_moments_m4[1] * 1e12, 0, 'e', 4)
                                           .arg(res.second_moments_m4[2] * 1e12, 0, 'e', 4));
            polarMomentValueLabel->setText(QString::number(res.polar_moment_m4 * 1e12, 'e', 4) + " mm⁴");
            statusLabel->setText(QString("Section evaluated successfully; principal angle = %1°")
                                     .arg(res.principal_angle_rad * 180.0 / std::numbers::pi, 0, 'f', 2));
        }
    });
}

void SectionPropertiesDialog::calculate() {
    if (!client || !window)
        return;
    auto sel = window->state().selection;
    if (sel.kind != "Face" || sel.entity.isEmpty()) {
        statusLabel->setText("Please select a planar face in the Viewport first.");
        return;
    }

    faceLabel->setText("Selected Face: " + sel.entity);

    domain::GeometryReference ref;
    ref.role = "Face";
    ref.object = sel.occurrence.isEmpty() ? sel.entity : sel.occurrence;
    ref.topology = sel.entity;

    features::SectionPropertiesRequest req;
    req.revision = window->state().revision;
    req.targetType = "Face";
    req.faceReference = ref;

    client->projectCommand("EvaluateSectionProperties", req.map());
    statusLabel->setText("Computing exact planar section properties...");
}

// ============================================================================
// SectionViewDialog
// ============================================================================

SectionViewDialog::SectionViewDialog(MainWindow *win, Client *cli, Viewport *vp, QWidget *parent)
    : QDialog(parent ? parent : win), window(win), client(cli), viewport(vp) {
    setObjectName("sectionViewDialog");
    setWindowTitle("Named Section Views");
    resize(500, 360);

    initialClippingActive = viewport ? viewport->sectionClippingActive() : false;

    auto *mainLayout = new QHBoxLayout(this);

    // Left: Section View List
    auto *leftCol = new QVBoxLayout;
    leftCol->addWidget(new QLabel("Section Views:", this));
    sectionList = new QListWidget(this);
    sectionList->setObjectName("sectionViewList");
    leftCol->addWidget(sectionList, 1);

    auto *listBtns = new QHBoxLayout;
    newButton = new QPushButton("New", this);
    newButton->setObjectName("sectionViewNewButton");
    listBtns->addWidget(newButton);

    deleteButton = new QPushButton("Delete", this);
    deleteButton->setObjectName("sectionViewDeleteButton");
    listBtns->addWidget(deleteButton);
    leftCol->addLayout(listBtns);

    mainLayout->addLayout(leftCol, 1);

    // Right: Controls
    auto *rightCol = new QVBoxLayout;
    auto *form = new QFormLayout;

    nameEdit = new QLineEdit("Section 1", this);
    nameEdit->setObjectName("sectionViewName");
    form->addRow("Name:", nameEdit);

    activeCheck = new QCheckBox("Active clipping", this);
    activeCheck->setObjectName("sectionViewActive");
    activeCheck->setChecked(true);
    form->addRow("Active:", activeCheck);

    supportCombo = new QComboBox(this);
    supportCombo->setObjectName("sectionViewSupport");
    supportCombo->addItem("Front Plane", "datum/front");
    supportCombo->addItem("Top Plane", "datum/top");
    supportCombo->addItem("Right Plane", "datum/right");
    form->addRow("Support Plane:", supportCombo);

    offsetSpin = new QDoubleSpinBox(this);
    offsetSpin->setObjectName("sectionViewOffset");
    offsetSpin->setRange(-10000.0, 10000.0);
    offsetSpin->setSuffix(" mm");
    offsetSpin->setValue(0.0);
    form->addRow("Offset:", offsetSpin);

    flipCheck = new QCheckBox("Flip normal", this);
    flipCheck->setObjectName("sectionViewFlip");
    form->addRow("Direction:", flipCheck);

    rightCol->addLayout(form);
    rightCol->addStretch(1);

    auto *actionBtns = new QHBoxLayout;
    applyButton = new QPushButton("Apply", this);
    applyButton->setObjectName("sectionViewApplyButton");
    actionBtns->addWidget(applyButton);

    auto *closeBtn = new QPushButton("Close", this);
    closeBtn->setObjectName("sectionViewCloseButton");
    actionBtns->addWidget(closeBtn);
    rightCol->addLayout(actionBtns);

    mainLayout->addLayout(rightCol, 1);

    connect(newButton, &QPushButton::clicked, this, &SectionViewDialog::createNew);
    connect(deleteButton, &QPushButton::clicked, this, &SectionViewDialog::removeCurrent);
    connect(activeCheck, &QCheckBox::toggled, this, &SectionViewDialog::updatePreview);
    connect(offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionViewDialog::updatePreview);
    connect(flipCheck, &QCheckBox::toggled, this, &SectionViewDialog::updatePreview);
    connect(supportCombo, &QComboBox::currentIndexChanged, this, &SectionViewDialog::updatePreview);
    connect(applyButton, &QPushButton::clicked, this, &SectionViewDialog::apply);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    populateSections();
}

void SectionViewDialog::populateSections() {
    sectionList->clear();
    if (!window)
        return;

    auto activeDoc = window->state().activeDocument;
    QCborMap activeRecord;
    for (auto val : window->state().documentRecords) {
        auto d = val.toMap();
        if (d.value("id").toString() == activeDoc)
            activeRecord = d;
    }

    auto supMap = activeRecord.contains(QStringLiteral("model_supports"))
                      ? activeRecord.value("model_supports").toMap()
                      : activeRecord.value("supports").toMap();

    for (auto val : supMap.value("section_views").toArray()) {
        auto sv = features::SectionView::parse(val.toMap());
        auto *item = new QListWidgetItem(sv.name, sectionList);
        item->setData(Qt::UserRole, sv.id);
    }
    if (sectionList->count() == 0)
        createNew();
    else
        sectionList->setCurrentRow(0);
}

void SectionViewDialog::updatePreview() {
    if (!viewport)
        return;
    bool active = activeCheck->isChecked();
    double offsetMM = offsetSpin->value();
    bool flip = flipCheck->isChecked();
    auto sup = supportCombo->currentText();
    QVector3D norm(0, 0, 1);
    if (sup.contains("Top"))
        norm = QVector3D(0, 1, 0);
    else if (sup.contains("Right"))
        norm = QVector3D(1, 0, 0);

    viewport->setSectionClipping(active, QVector3D(0, 0, 0), norm, offsetMM, flip);
}

void SectionViewDialog::createNew() {
    int count = sectionList->count() + 1;
    QString name = QString("Section %1").arg(count);
    auto *item = new QListWidgetItem(name, sectionList);
    features::SectionView sv;
    sv.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sv.name = name;
    sv.active = true;
    item->setData(Qt::UserRole, sv.id);
    sectionList->setCurrentItem(item);
    nameEdit->setText(name);
    activeCheck->setChecked(true);
    offsetSpin->setValue(0.0);
    flipCheck->setChecked(false);
    updatePreview();
}

void SectionViewDialog::removeCurrent() {
    auto *item = sectionList->currentItem();
    if (!item)
        return;
    delete item;
    if (sectionList->count() == 0) {
        if (viewport)
            viewport->setSectionClipping(false);
    } else {
        updatePreview();
    }
}

void SectionViewDialog::apply() {
    if (!client || !window)
        return;

    auto activeDoc = window->state().activeDocument;
    if (activeDoc.isEmpty())
        return;

    QCborMap activeRecord;
    for (auto val : window->state().documentRecords) {
        auto d = val.toMap();
        if (d.value("id").toString() == activeDoc)
            activeRecord = d;
    }

    auto supMap = activeRecord.contains(QStringLiteral("model_supports"))
                      ? activeRecord.value("model_supports").toMap()
                      : activeRecord.value("supports").toMap();
    auto model = model::SupportDefinition::parse(supMap);
    model.document = activeDoc;

    features::SectionView sv;
    auto currentItem = sectionList->currentItem();
    if (currentItem && !currentItem->data(Qt::UserRole).toString().isEmpty() &&
        wire::uuid(currentItem->data(Qt::UserRole).toString())) {
        sv.id = currentItem->data(Qt::UserRole).toString();
    } else {
        sv.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    sv.name = nameEdit->text();
    sv.active = activeCheck->isChecked();
    sv.offset = offsetSpin->value() / 1000.0; // convert mm to meters SI
    sv.flip = flipCheck->isChecked();

    bool resolvedSupport = false;
    auto sel = window->state().selection;
    if (supportCombo->currentText().contains("Selected Face") && sel.kind == "Face" &&
        !sel.entity.isEmpty() && wire::uuid(sel.occurrence)) {
        domain::GeometryReference ref;
        ref.role = "Face";
        ref.object = sel.occurrence;
        ref.topology = sel.entity;
        sv.support = ref;
        resolvedSupport = true;
    }
    if (!resolvedSupport) {
        QString planeName = supportCombo->currentText();
        QString paramPlane = planeName.contains("Top") ? "XZ" : (planeName.contains("Right") ? "YZ" : "XY");
        QString datumId;
        for (const auto &d : model.datums) {
            if (d.kind == "DatumPlane" && d.mode == "Principal" &&
                d.parameters.value("plane").toString() == paramPlane) {
                datumId = d.id;
                break;
            }
        }
        if (datumId.isEmpty()) {
            model::Datum pl;
            pl.kind = "DatumPlane";
            pl.mode = "Principal";
            pl.parameters = fields({{"plane", paramPlane}});
            model.datums.push_back(pl);
            model.sequence();
            datumId = pl.id;
        }
        domain::GeometryReference ref;
        ref.role = "Plane";
        ref.object = datumId;
        sv.support = ref;
    }

    QCborArray views;
    views.append(sv.map());
    model.section_views = views;

    client->projectCommand("SetModelSupports", fields({{"model", model.map()}}));
    initialClippingActive = sv.active;
    updatePreview();
    accept();
}

void SectionViewDialog::reject() {
    if (viewport)
        viewport->setSectionClipping(initialClippingActive);
    QDialog::reject();
}

// ============================================================================
// MainWindow Integration
// ============================================================================

void MainWindow::buildEvaluationUI() {
    auto *matDlg = new MaterialDialog(this, client, this);
    auto *massDlg = new MassPropertiesDialog(this, client, this);
    auto *secDlg = new SectionPropertiesDialog(this, client, this);
    auto *secViewDlg = new SectionViewDialog(this, client, viewport, this);

    connect(commands.get("material.manage"), &QAction::triggered, matDlg, [matDlg] {
        matDlg->populateTree();
        matDlg->show();
    });

    connect(commands.get("massProperties"), &QAction::triggered, massDlg, [massDlg] {
        massDlg->refreshScopes();
        massDlg->show();
        massDlg->calculate();
    });

    connect(commands.get("sectionProperties"), &QAction::triggered, secDlg, [secDlg] {
        secDlg->show();
        secDlg->calculate();
    });

    connect(commands.get("sectionView"), &QAction::triggered, secViewDlg, [secViewDlg] {
        secViewDlg->populateSections();
        secViewDlg->show();
    });

    auto *hiddenLinesAction = commands.add(this, "view.hiddenLines", "Hidden Lines Visible");
    hiddenLinesAction->setCheckable(true);
    connect(hiddenLinesAction, &QAction::toggled, this, [this](bool checked) {
        if (viewport)
            viewport->setMode(checked ? "Hidden Lines Visible" : "Shaded with edges");
    });

    auto *shadowsAction = commands.add(this, "view.shadows", "Shadows");
    shadowsAction->setCheckable(true);
    connect(shadowsAction, &QAction::toggled, this, [this](bool checked) {
        if (viewport)
            viewport->setShadows(checked);
    });
}

void MainWindow::refreshEvaluationUI() {
    bool idle = client->ready() && client->outstanding() == 0;
    bool hasPart = false;
    for (auto val : state().documentRecords) {
        if (val.toMap().value("id").toString() == state().activeDocument &&
            val.toMap().value("type").toString() == "Part") {
            hasPart = true;
            break;
        }
    }

    if (commands.get("material.manage"))
        commands.get("material.manage")->setEnabled(hasPart && idle);
    if (commands.get("massProperties"))
        commands.get("massProperties")->setEnabled(hasPart && idle);
    if (commands.get("sectionProperties"))
        commands.get("sectionProperties")->setEnabled(hasPart && idle);
    if (commands.get("sectionView"))
        commands.get("sectionView")->setEnabled(hasPart && idle);
    if (commands.get("view.hiddenLines"))
        commands.get("view.hiddenLines")->setEnabled(bool(client->mesh()) && idle);
    if (commands.get("view.shadows"))
        commands.get("view.shadows")->setEnabled(bool(client->mesh()) && idle);
}

} // namespace cad

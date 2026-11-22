#include "EvaluationUI.hpp"
#include "features/Catalogs.hpp"
#include "features/Model.hpp"
#include "model/SupportEditing.hpp"
#include "render/Viewport.hpp"
#include <QColorDialog>
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

namespace {
QCborMap activePart(const MainWindow &window) {
    for (auto val : window.state().documentRecords) {
        auto record = val.toMap();
        if (record.value("id") == window.state().activeDocument && record.value("type") == "Part")
            return record;
    }
    return {};
}

QCborMap selectedFace(const Client &client, const Selection &selection) {
    if (selection.kind != "Face")
        return {};
    for (auto value : client.acceptedPart().value("topology").toArray()) {
        auto face = value.toMap();
        if (face.value("body").toString() + "/" + face.value("token").toString() ==
            selection.entity)
            return face;
    }
    return {};
}

domain::GeometryReference faceReference(const QCborMap &face) {
    domain::GeometryReference ref;
    ref.role = "Face";
    ref.object = face.value("stage").toString();
    ref.topology = face.value("token").toString();
    return ref;
}

QVector3D displayVector(const QCborMap &record, QString key) {
    auto v = features::vector(record, key);
    return {float(v[0]), float(v[1]), float(v[2])};
}
} // namespace

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
    poissonLabel->setText(p.poissons_ratio.has_value() ? QString::number(*p.poissons_ratio, 'f', 2)
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
    favoriteButton->setText(favorites.contains(matId) ? "Remove from Favorites"
                                                      : "Add to Favorites");
}

void MaterialDialog::addCustomMaterial() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Custom Material",
                                         "Material Name:", QLineEdit::Normal, "Custom Alloy", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    double density = QInputDialog::getDouble(this, "Custom Material Density",
                                             "Density (kg/m³):", 7800.0, 1.0, 50000.0, 1, &ok);
    if (!ok)
        return;

    features::Material m;
    m.id = "custom_" + QString::number(customMaterials.size() + 1);
    m.name = name.trimmed();
    m.family = "Custom";
    m.properties.density_kg_m3 = density;
    customMaterials.push_back(m);
    populateTree(materialSearch->text());
    selectMaterial(m.id);
}

void MaterialDialog::applyMaterial() {
    if (!currentMaterial || !client || !window)
        return;

    auto activeDoc = window->state().activeDocument;
    if (activeDoc.isEmpty())
        return;

    auto model = model::editableSupports(activePart(*window));
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
            surfaceAreaValueLabel->setText(QString::number(res.surface_area_m2 * 1e6, 'g', 6) +
                                           " mm² (" + QString::number(res.surface_area_m2, 'g', 6) +
                                           " m²)");
            centroidValueLabel->setText(QString("X = %1 mm, Y = %2 mm, Z = %3 mm")
                                            .arg(res.center_of_mass_m[0] * 1000.0, 0, 'f', 3)
                                            .arg(res.center_of_mass_m[1] * 1000.0, 0, 'f', 3)
                                            .arg(res.center_of_mass_m[2] * 1000.0, 0, 'f', 3));
            momentsValueLabel->setText(
                QString("Ixx = %1, Iyy = %2, Izz = %3\nIxy = %4, Ixz = %5, Iyz = %6 kg·m²")
                    .arg(res.inertia_tensor_kg_m2[0], 0, 'e', 4)
                    .arg(res.inertia_tensor_kg_m2[1], 0, 'e', 4)
                    .arg(res.inertia_tensor_kg_m2[2], 0, 'e', 4)
                    .arg(res.inertia_tensor_kg_m2[3], 0, 'e', 4)
                    .arg(res.inertia_tensor_kg_m2[4], 0, 'e', 4)
                    .arg(res.inertia_tensor_kg_m2[5], 0, 'e', 4));
            principalMomentsValueLabel->setText(
                QString("Ix = %1, Iy = %2, Iz = %3 kg·m²")
                    .arg(res.principal_moments_kg_m2[0], 0, 'e', 4)
                    .arg(res.principal_moments_kg_m2[1], 0, 'e', 4)
                    .arg(res.principal_moments_kg_m2[2], 0, 'e', 4));
            statusLabel->setText(
                QString("Evaluated %1 solid bodies successfully").arg(res.solids_count));
        }
    });
}

void MassPropertiesDialog::refreshScopes() {
    scopeCombo->clear();
    scopeCombo->addItem("Entire Part");
    if (client)
        for (auto value : client->acceptedPart().value("bodies").toArray()) {
            const auto id = value.toString();
            scopeCombo->addItem("Body " + id.right(8), id);
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
            auto res =
                features::SectionPropertiesResult::parse(data.value("section_properties").toMap());
            if (!res.valid) {
                statusLabel->setText("Evaluation failed: " + res.reason);
                return;
            }
            areaValueLabel->setText(QString::number(res.area_m2 * 1e6, 'g', 6) + " mm² (" +
                                    QString::number(res.area_m2, 'g', 6) + " m²)");
            centroidValueLabel->setText(
                QString("Local: u = %1 mm, v = %2 mm\nPart: X = %3 mm, Y = %4 mm, Z = %5 mm")
                    .arg(res.centroid_local_m[0] * 1000.0, 0, 'f', 3)
                    .arg(res.centroid_local_m[1] * 1000.0, 0, 'f', 3)
                    .arg(res.centroid_part_m[0] * 1000.0, 0, 'f', 3)
                    .arg(res.centroid_part_m[1] * 1000.0, 0, 'f', 3)
                    .arg(res.centroid_part_m[2] * 1000.0, 0, 'f', 3));
            momentsValueLabel->setText(QString("Iuu = %1, Ivv = %2, Iuv = %3 mm⁴")
                                           .arg(res.second_moments_m4[0] * 1e12, 0, 'e', 4)
                                           .arg(res.second_moments_m4[1] * 1e12, 0, 'e', 4)
                                           .arg(res.second_moments_m4[2] * 1e12, 0, 'e', 4));
            polarMomentValueLabel->setText(QString::number(res.polar_moment_m4 * 1e12, 'e', 4) +
                                           " mm⁴");
            statusLabel->setText(
                QString("Section evaluated successfully; principal angle = %1°")
                    .arg(res.principal_angle_rad * 180.0 / std::numbers::pi, 0, 'f', 2));
        }
    });
}

void SectionPropertiesDialog::calculate() {
    if (!client || !window)
        return;
    auto sel = window->state().selection;
    if (sel.kind == "Sketch" && !sel.entity.isEmpty()) {
        faceLabel->setText("Selected Sketch: " + sel.entity);
        features::SectionPropertiesRequest req;
        req.revision = window->state().revision;
        req.targetType = "Sketch";
        req.sketchId = sel.entity;
        client->projectCommand("EvaluateSectionProperties", req.map());
        statusLabel->setText("Computing exact planar sketch section properties...");
        return;
    }
    if (sel.kind != "Face" || sel.entity.isEmpty()) {
        statusLabel->setText("Please select a planar face or closed sketch first.");
        return;
    }

    faceLabel->setText("Selected Face: " + sel.entity);

    domain::GeometryReference ref;
    ref.document = "Self";
    ref.role = "Face";
    QString top = sel.entity;
    QString obj = window->state().activeDocument;
    if (top.contains('/')) {
        int slash = top.indexOf('/');
        QString maybeUuid = top.left(slash);
        if (wire::uuid(maybeUuid))
            obj = maybeUuid;
        top = top.mid(slash + 1);
    }
    ref.object = obj;
    ref.topology = top;

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

    connect(sectionList, &QListWidget::currentItemChanged, this,
            &SectionViewDialog::onCurrentItemChanged);
    connect(newButton, &QPushButton::clicked, this, &SectionViewDialog::createNew);
    connect(deleteButton, &QPushButton::clicked, this, &SectionViewDialog::removeCurrent);
    connect(activeCheck, &QCheckBox::toggled, this, &SectionViewDialog::updatePreview);
    connect(offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &SectionViewDialog::updatePreview);
    connect(flipCheck, &QCheckBox::toggled, this, &SectionViewDialog::updatePreview);
    connect(supportCombo, &QComboBox::currentIndexChanged, this, &SectionViewDialog::updatePreview);
    connect(applyButton, &QPushButton::clicked, this, &SectionViewDialog::apply);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void SectionViewDialog::populateSections() {
    if (viewport) {
        initialClippingActive = viewport->sectionClippingActive();
        initialOrigin = viewport->sectionClippingOrigin();
        initialNormal = viewport->sectionClippingNormal();
        initialOffset = viewport->sectionClippingOffset();
        initialFlip = viewport->sectionClippingFlipped();
    }
    updatingSelection = true;
    sectionList->clear();
    loadedSections.clear();
    loadSupportChoices();
    for (auto val :
         activePart(*window).value("model_supports").toMap().value("section_views").toArray()) {
        auto sv = features::SectionView::parse(val.toMap());
        auto *item = new QListWidgetItem(sv.name, sectionList);
        item->setData(Qt::UserRole, sv.id);
        loadedSections[sv.id] = sv;
    }
    updatingSelection = false;
    if (sectionList->count() == 0)
        createNew();
    else
        sectionList->setCurrentRow(0);
}

void SectionViewDialog::loadSupportChoices() {
    supportCombo->clear();
    const auto record = activePart(*window);
    auto model = model::editableSupports(record);
    for (auto plane : {"XY", "XZ", "YZ"}) {
        const auto existing =
            std::find_if(model.datums.begin(), model.datums.end(), [&](const auto &datum) {
                return datum.mode == "Principal" && datum.kind == "DatumPlane" &&
                       datum.parameters.value("plane") == plane;
            });
        supportCombo->addItem(QString(plane) + " Plane", "principal/" + QString(plane));
        if (existing != model.datums.end())
            supportCombo->setItemData(supportCombo->count() - 1, existing->id, Qt::UserRole + 1);
    }
    for (auto it = window->datumGeometry.cbegin(); it != window->datumGeometry.cend(); ++it)
        if (it->kind == "DatumPlane" && wire::uuid(it.key()) &&
            supportCombo->findData(it.key(), Qt::UserRole + 1) < 0)
            supportCombo->addItem(window->datumLabels.value(it.key()), it.key());
    const auto face = selectedFace(*client, window->state().selection);
    if (face.contains(QStringLiteral("normal"))) {
        const auto ref = faceReference(face);
        if (wire::uuid(ref.object))
            supportCombo->addItem("Selected Face", QVariant::fromValue(ref.map()));
    }
}

void SectionViewDialog::saveCurrent(QListWidgetItem *item) {
    if (!item || !loadedSections.contains(item->data(Qt::UserRole).toString()))
        return;
    auto &view = loadedSections[item->data(Qt::UserRole).toString()];
    view.name = nameEdit->text().trimmed();
    item->setText(view.name);
    view.active = activeCheck->isChecked();
    view.offset = offsetSpin->value() / 1000.0;
    view.flip = flipCheck->isChecked();
    if (supportCombo->currentData().canConvert<QCborMap>()) {
        view.support =
            domain::GeometryReference::parse(supportCombo->currentData().value<QCborMap>());
    } else {
        view.support = {};
        view.support.role = "Plane";
        view.support.object = supportCombo->currentData().toString();
    }
}

void SectionViewDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous) {
    if (updatingSelection || !current)
        return;
    saveCurrent(previous);
    updatingSelection = true;
    const auto view = loadedSections.value(current->data(Qt::UserRole).toString());
    nameEdit->setText(view.name);
    activeCheck->setChecked(view.active);
    offsetSpin->setValue(view.offset * 1000.0);
    flipCheck->setChecked(view.flip);
    if (view.support.role == "Face") {
        const auto data = QVariant::fromValue(view.support.map());
        int index = supportCombo->findData(data);
        if (index < 0) {
            supportCombo->addItem("Saved Face", data);
            index = supportCombo->count() - 1;
        }
        supportCombo->setCurrentIndex(index);
    } else if (!view.support.object.isEmpty()) {
        int index = supportCombo->findData(view.support.object);
        if (index < 0)
            index = supportCombo->findData(view.support.object, Qt::UserRole + 1);
        if (index < 0) {
            supportCombo->addItem("Saved Plane", view.support.object);
            index = supportCombo->count() - 1;
        }
        supportCombo->setCurrentIndex(index);
    }
    updatingSelection = false;
    updatePreview();
}

void SectionViewDialog::updatePreview() {
    if (!viewport || updatingSelection)
        return;
    const auto choice = supportCombo->currentData();
    if (choice.canConvert<QCborMap>()) {
        const auto ref = domain::GeometryReference::parse(choice.value<QCborMap>());
        for (auto value : client->acceptedPart().value("topology").toArray()) {
            const auto face = value.toMap();
            if (face.value("stage") == ref.object && face.value("token") == ref.topology &&
                face.contains(QStringLiteral("normal"))) {
                viewport->setSectionClipping(
                    activeCheck->isChecked(), displayVector(face, "center_si") * 1000,
                    displayVector(face, "normal"), offsetSpin->value(), flipCheck->isChecked());
                return;
            }
        }
        auto item = sectionList->currentItem();
        if (item &&
            loadedSections.value(item->data(Qt::UserRole).toString()).support.map() == ref.map() &&
            window->datumResultKey == window->datumKey()) {
            for (auto value : window->acceptedSections) {
                const auto frame = value.toMap();
                if (frame.value("id") == item->data(Qt::UserRole).toString()) {
                    viewport->setSectionClipping(activeCheck->isChecked(),
                                                 displayVector(frame, "origin") * 1000,
                                                 displayVector(frame, "normal"),
                                                 offsetSpin->value(), flipCheck->isChecked());
                    return;
                }
            }
        }
    } else {
        const auto it = window->datumGeometry.constFind(choice.toString());
        if (it != window->datumGeometry.cend()) {
            const auto &f = it->frame;
            QVector3D u(f.u[0], f.u[1], f.u[2]), v(f.v[0], f.v[1], f.v[2]);
            viewport->setSectionClipping(
                activeCheck->isChecked(), QVector3D(f.origin[0], f.origin[1], f.origin[2]) * 1000,
                QVector3D::crossProduct(u, v), offsetSpin->value(), flipCheck->isChecked());
            return;
        }
    }
    viewport->setSectionClipping(false);
}

void SectionViewDialog::createNew() {
    saveCurrent(sectionList->currentItem());
    int count = sectionList->count() + 1;
    QString name = QString("Section %1").arg(count);
    features::SectionView sv;
    sv.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sv.name = name;
    sv.active = true;
    sv.offset = 0.0;
    sv.flip = false;
    loadedSections[sv.id] = sv;

    updatingSelection = true;
    auto *item = new QListWidgetItem(name, sectionList);
    item->setData(Qt::UserRole, sv.id);
    sectionList->setCurrentItem(item);
    nameEdit->setText(name);
    activeCheck->setChecked(true);
    offsetSpin->setValue(0.0);
    flipCheck->setChecked(false);
    supportCombo->setCurrentIndex(0);
    updatingSelection = false;

    updatePreview();
}

void SectionViewDialog::removeCurrent() {
    auto *item = sectionList->currentItem();
    if (!item)
        return;
    QString id = item->data(Qt::UserRole).toString();
    loadedSections.remove(id);
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

    try {
        auto model = model::editableSupports(activePart(*window));
        saveCurrent(sectionList->currentItem());
        QCborArray views;
        for (int i = 0; i < sectionList->count(); ++i) {
            const auto id = sectionList->item(i)->data(Qt::UserRole).toString();
            auto view = loadedSections.value(id);
            if (view.support.object.startsWith("principal/")) {
                const auto plane = view.support.object.mid(10);
                auto existing =
                    std::find_if(model.datums.begin(), model.datums.end(), [&](const auto &datum) {
                        return datum.mode == "Principal" && datum.kind == "DatumPlane" &&
                               datum.parameters.value("plane") == plane;
                    });
                if (existing == model.datums.end()) {
                    model::Datum principal;
                    principal.kind = "DatumPlane";
                    principal.mode = "Principal";
                    principal.parameters = fields({{"plane", plane}});
                    model.datums.push_back(principal);
                    view.support.object = principal.id;
                } else {
                    view.support.object = existing->id;
                }
            }
            views.append(view.map());
        }
        model.sequence();
        client->projectCommand("SetModelSupports",
                               fields({{"model", model.map()}, {"section_views", views}}));
        accept();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Section View", QString::fromUtf8(e.what()));
    }
}

void SectionViewDialog::reject() {
    if (viewport)
        viewport->setSectionClipping(initialClippingActive, initialOrigin, initialNormal,
                                     initialOffset, initialFlip);
    QDialog::reject();
}

// ============================================================================
// AppearanceDialog
// ============================================================================

AppearanceDialog::AppearanceDialog(MainWindow *win, Client *cli, Viewport *vp, QWidget *parent)
    : QDialog(parent ? parent : win), window(win), client(cli), viewport(vp) {
    setObjectName("appearanceDialog");
    setWindowTitle("Appearance Overrides");
    resize(420, 320);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    scopeCombo = new QComboBox(this);
    scopeCombo->setObjectName("appearanceScope");
    scopeCombo->addItems({"Entire Part", "Body", "Feature", "Face"});
    form->addRow("Scope:", scopeCombo);

    targetLabel = new QLabel("Part", this);
    targetLabel->setObjectName("appearanceTarget");
    form->addRow("Target:", targetLabel);

    auto *colorLayout = new QHBoxLayout;
    colorSwatch = new QLabel(this);
    colorSwatch->setObjectName("appearanceColorSwatch");
    colorSwatch->setFixedSize(36, 24);
    colorSwatch->setAutoFillBackground(true);
    colorButton = new QPushButton("Choose Color...", this);
    colorButton->setObjectName("appearanceChooseColor");
    colorLayout->addWidget(colorSwatch);
    colorLayout->addWidget(colorButton);
    colorLayout->addStretch(1);
    form->addRow("Color:", colorLayout);

    roughnessSpin = new QDoubleSpinBox(this);
    roughnessSpin->setObjectName("appearanceRoughness");
    roughnessSpin->setRange(0.0, 1.0);
    roughnessSpin->setSingleStep(0.05);
    roughnessSpin->setValue(0.5);
    form->addRow("Roughness:", roughnessSpin);

    metalnessSpin = new QDoubleSpinBox(this);
    metalnessSpin->setObjectName("appearanceMetalness");
    metalnessSpin->setRange(0.0, 1.0);
    metalnessSpin->setSingleStep(0.05);
    metalnessSpin->setValue(0.0);
    form->addRow("Metalness:", metalnessSpin);

    transparencySpin = new QDoubleSpinBox(this);
    transparencySpin->setObjectName("appearanceTransparency");
    transparencySpin->setRange(0.0, 1.0);
    transparencySpin->setSingleStep(0.05);
    transparencySpin->setValue(0.0);
    form->addRow("Transparency:", transparencySpin);

    mainLayout->addLayout(form);
    mainLayout->addStretch(1);

    auto *btnLayout = new QHBoxLayout;
    applyButton = new QPushButton("Apply", this);
    applyButton->setObjectName("appearanceApplyButton");
    btnLayout->addWidget(applyButton);

    removeButton = new QPushButton("Remove", this);
    removeButton->setObjectName("appearanceRemoveButton");
    btnLayout->addWidget(removeButton);

    closeButton = new QPushButton("Close", this);
    closeButton->setObjectName("appearanceCloseButton");
    btnLayout->addWidget(closeButton);

    mainLayout->addLayout(btnLayout);

    setColor(QVector4D(0.7f, 0.7f, 0.7f, 1.0f));

    connect(scopeCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &scope) { setScope(scope); });

    connect(colorButton, &QPushButton::clicked, this, [this] {
        QColor initCol(int(currentColor.x() * 255), int(currentColor.y() * 255),
                       int(currentColor.z() * 255), int(currentColor.w() * 255));
        QColor chosen = QColorDialog::getColor(initCol, this, "Select Appearance Color");
        if (chosen.isValid()) {
            setColor(QVector4D(chosen.redF(), chosen.greenF(), chosen.blueF(), chosen.alphaF()));
            updatePreview();
        }
    });

    connect(roughnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &AppearanceDialog::updatePreview);
    connect(metalnessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &AppearanceDialog::updatePreview);
    connect(transparencySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &AppearanceDialog::updatePreview);

    connect(applyButton, &QPushButton::clicked, this, &AppearanceDialog::apply);
    connect(removeButton, &QPushButton::clicked, this, &AppearanceDialog::remove);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void AppearanceDialog::setColor(const QVector4D &color) {
    currentColor = color;
    if (colorSwatch) {
        int r = qBound(0, int(currentColor.x() * 255), 255);
        int g = qBound(0, int(currentColor.y() * 255), 255);
        int b = qBound(0, int(currentColor.z() * 255), 255);
        colorSwatch->setStyleSheet(
            QString(
                "background-color: rgb(%1, %2, %3); border: 1px solid #555; border-radius: 3px;")
                .arg(r)
                .arg(g)
                .arg(b));
    }
}

void AppearanceDialog::refreshTarget() {
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
    initialOverrides = supMap.value("appearance_overrides").toArray();
    initialSaved = true;

    auto sel = window->state().selection;
    if (sel.kind == "Face" && !sel.entity.isEmpty()) {
        scopeCombo->setCurrentText("Face");
        currentFaceRef = faceReference(selectedFace(*client, sel));
        currentTarget = sel.entity;
        targetLabel->setText("Face: " + sel.entity);
    } else if (sel.kind == "Feature" && !sel.entity.isEmpty()) {
        scopeCombo->setCurrentText("Feature");
        currentTarget = sel.entity;
        targetLabel->setText("Feature: " + sel.entity);
    } else if (sel.kind == "Body" && wire::uuid(sel.entity)) {
        scopeCombo->setCurrentText("Body");
        currentTarget = sel.entity;
        targetLabel->setText("Body: " + currentTarget);
    } else {
        scopeCombo->setCurrentText("Entire Part");
        currentTarget = activeDoc;
        targetLabel->setText("Part: " + (activeRecord.value("name").toString().isEmpty()
                                             ? activeDoc
                                             : activeRecord.value("name").toString()));
    }
}

void AppearanceDialog::setScope(const QString &scope) {
    if (!window)
        return;
    auto activeDoc = window->state().activeDocument;
    auto sel = window->state().selection;
    if (scope == "Entire Part") {
        currentTarget = activeDoc;
        targetLabel->setText("Part: " + activeDoc);
    } else if (scope == "Body") {
        currentTarget = sel.kind == "Body" && wire::uuid(sel.entity) ? sel.entity : QString{};
        targetLabel->setText("Body: " + currentTarget);
    } else if (scope == "Feature") {
        currentTarget = sel.kind == "Feature" && wire::uuid(sel.entity) ? sel.entity : QString{};
        targetLabel->setText("Feature: " + currentTarget);
    } else if (scope == "Face") {
        currentFaceRef = faceReference(selectedFace(*client, sel));
        currentTarget = sel.entity;
        targetLabel->setText("Face: " + currentTarget);
    }
    bool valid = scope == "Face" ? wire::uuid(currentFaceRef.object) : wire::uuid(currentTarget);
    applyButton->setEnabled(valid);
    removeButton->setEnabled(valid);
    updatePreview();
}

void AppearanceDialog::updatePreview() {
    if (!viewport)
        return;
    QString scope = scopeCombo->currentText();
    if (scope == "Entire Part") {
        viewport->setPartAppearance(currentColor);
    } else if (scope == "Body") {
        viewport->setBodyAppearance(currentTarget, currentColor);
    } else if (scope == "Feature") {
        viewport->setFeatureAppearance(currentTarget, currentColor);
    } else if (scope == "Face") {
        viewport->setFaceAppearance(currentTarget, currentColor);
        int slash = currentTarget.indexOf('/');
        if (slash > 0)
            viewport->setFaceAppearance(currentTarget.mid(slash + 1), currentColor);
    }
}

void AppearanceDialog::apply() {
    if (!client || !window)
        return;
    auto activeDoc = window->state().activeDocument;
    if (activeDoc.isEmpty())
        return;
    auto model = model::editableSupports(activePart(*window));

    QString scope = scopeCombo->currentText();
    QString level = scope == "Entire Part" ? "Part" : scope;

    features::AppearanceOverride o;
    o.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    o.level = level;
    o.target = currentTarget;
    if (level == "Face") {
        o.faceReference = currentFaceRef;
        o.target = currentTarget;
    }
    o.appearance.color_rgba = {float(currentColor.x()), float(currentColor.y()),
                               float(currentColor.z()), float(currentColor.w())};
    o.appearance.roughness = float(roughnessSpin->value());
    o.appearance.metalness = float(metalnessSpin->value());
    o.appearance.transparency = float(transparencySpin->value());

    QCborArray updatedOverrides;
    for (auto val : model.appearance_overrides) {
        auto existing = features::AppearanceOverride::parse(val.toMap());
        if (existing.level == level) {
            if (level == "Part")
                continue;
            if (level == "Face" && (existing.faceReference.topology == o.faceReference.topology ||
                                    existing.target == o.target))
                continue;
            if ((level == "Body" || level == "Feature") && existing.target == o.target)
                continue;
        }
        updatedOverrides.append(val);
    }
    updatedOverrides.append(o.map());
    model.appearance_overrides = updatedOverrides;

    client->projectCommand("SetModelSupports", fields({{"model", model.map()}}));
    initialOverrides = updatedOverrides;
    updatePreview();
    accept();
}

void AppearanceDialog::remove() {
    if (!client || !window)
        return;
    auto activeDoc = window->state().activeDocument;
    if (activeDoc.isEmpty())
        return;
    auto model = model::editableSupports(activePart(*window));

    QString scope = scopeCombo->currentText();
    QString level = scope == "Entire Part" ? "Part" : scope;

    QCborArray updatedOverrides;
    for (auto val : model.appearance_overrides) {
        auto existing = features::AppearanceOverride::parse(val.toMap());
        if (existing.level == level) {
            if (level == "Part")
                continue;
            if (level == "Face" && (existing.faceReference.topology == currentTarget ||
                                    existing.target == currentTarget))
                continue;
            if ((level == "Body" || level == "Feature") && existing.target == currentTarget)
                continue;
        }
        updatedOverrides.append(val);
    }
    client->projectCommand(
        "SetModelSupports",
        fields({{"model", model.map()}, {"appearance_overrides", updatedOverrides}}));
    initialOverrides = updatedOverrides;

    if (viewport) {
        viewport->clearAppearanceOverrides();
        for (auto val : updatedOverrides) {
            auto ov = features::AppearanceOverride::parse(val.toMap());
            QVector4D col(ov.appearance.color_rgba[0], ov.appearance.color_rgba[1],
                          ov.appearance.color_rgba[2], ov.appearance.color_rgba[3]);
            if (ov.level == "Part")
                viewport->setPartAppearance(col);
            else if (ov.level == "Body")
                viewport->setBodyAppearance(ov.target, col);
            else if (ov.level == "Feature")
                viewport->setFeatureAppearance(ov.target, col);
            else if (ov.level == "Face") {
                QString fid =
                    ov.faceReference.topology.isEmpty() ? ov.target : ov.faceReference.topology;
                viewport->setFaceAppearance(fid, col);
            }
        }
    }
    accept();
}

void AppearanceDialog::reject() {
    if (viewport && initialSaved) {
        viewport->clearAppearanceOverrides();
        for (auto val : initialOverrides) {
            auto ov = features::AppearanceOverride::parse(val.toMap());
            QVector4D col(ov.appearance.color_rgba[0], ov.appearance.color_rgba[1],
                          ov.appearance.color_rgba[2], ov.appearance.color_rgba[3]);
            if (ov.level == "Part")
                viewport->setPartAppearance(col);
            else if (ov.level == "Body")
                viewport->setBodyAppearance(ov.target, col);
            else if (ov.level == "Feature")
                viewport->setFeatureAppearance(ov.target, col);
            else if (ov.level == "Face") {
                QString fid =
                    ov.faceReference.topology.isEmpty() ? ov.target : ov.faceReference.topology;
                viewport->setFaceAppearance(fid, col);
            }
        }
    }
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
    auto *appDlg = new AppearanceDialog(this, client, viewport, this);

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

    connect(commands.get("appearance.manage"), &QAction::triggered, appDlg, [appDlg] {
        appDlg->refreshTarget();
        appDlg->show();
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
    if (commands.get("appearance.manage"))
        commands.get("appearance.manage")->setEnabled(hasPart && idle);
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

void MainWindow::syncViewportSupports() {
    if (!viewport)
        return;
    if (auto *appDlg = findChild<AppearanceDialog *>("appearanceDialog");
        appDlg && appDlg->isVisible())
        return;
    if (auto *secDlg = findChild<SectionViewDialog *>("sectionViewDialog");
        secDlg && secDlg->isVisible())
        return;

    const auto &s = state();
    if (!s.projectOpen || s.activeDocument.isEmpty()) {
        viewport->clearAppearanceOverrides();
        viewport->setSectionClipping(false);
        return;
    }
    QCborMap active;
    for (auto item : s.documentRecords) {
        auto d = item.toMap();
        if (d.value("id").toString() == s.activeDocument) {
            active = d;
            break;
        }
    }
    if (active.isEmpty()) {
        viewport->clearAppearanceOverrides();
        viewport->setSectionClipping(false);
        return;
    }

    auto supMap = active.contains(QStringLiteral("model_supports"))
                      ? active.value("model_supports").toMap()
                      : active.value("supports").toMap();

    QMap<QString, QString> faceFeatures;
    for (auto value : client->acceptedPart().value("topology").toArray()) {
        auto face = value.toMap();
        if (face.contains(QStringLiteral("feature")))
            faceFeatures.insert(face.value("body").toString() + "/" +
                                    face.value("token").toString(),
                                face.value("feature").toString());
    }
    viewport->setFaceFeatures(std::move(faceFeatures));
    // 1. Appearance overrides
    viewport->clearAppearanceOverrides();
    if (supMap.contains(QStringLiteral("appearance_overrides"))) {
        for (auto val : supMap.value("appearance_overrides").toArray()) {
            auto ov = features::AppearanceOverride::parse(val.toMap());
            QVector4D col(ov.appearance.color_rgba[0], ov.appearance.color_rgba[1],
                          ov.appearance.color_rgba[2], ov.appearance.color_rgba[3]);
            if (ov.level == "Part")
                viewport->setPartAppearance(col);
            else if (ov.level == "Body")
                viewport->setBodyAppearance(ov.target, col);
            else if (ov.level == "Feature")
                viewport->setFeatureAppearance(ov.target, col);
            else if (ov.level == "Face") {
                QString fid =
                    ov.faceReference.topology.isEmpty() ? ov.target : ov.faceReference.topology;
                if (!fid.isEmpty()) {
                    viewport->setFaceAppearance(fid, col);
                    int slash = fid.indexOf('/');
                    if (slash > 0)
                        viewport->setFaceAppearance(fid.mid(slash + 1), col);
                }
                if (!ov.target.isEmpty() && ov.target != fid) {
                    viewport->setFaceAppearance(ov.target, col);
                    int slash = ov.target.indexOf('/');
                    if (slash > 0)
                        viewport->setFaceAppearance(ov.target.mid(slash + 1), col);
                }
            }
        }
    }

    const auto part = client->acceptedPart();
    const auto sectionFrames = part.contains(QStringLiteral("section_frames"))
                                   ? part.value("section_frames").toArray()
                               : datumResultKey == datumKey() ? acceptedSections
                                                              : QCborArray{};
    for (auto val : supMap.value("section_views").toArray()) {
        const auto view = features::SectionView::parse(val.toMap());
        if (!view.active)
            continue;
        for (auto value : sectionFrames) {
            const auto frame = value.toMap();
            if (frame.value("id") != view.id)
                continue;
            if (view.cap_color_rgba) {
                const auto &color = *view.cap_color_rgba;
                viewport->setSectionCapColor(
                    {float(color[0]), float(color[1]), float(color[2]), float(color[3])});
            }
            viewport->setSectionClipping(true, displayVector(frame, "origin") * 1000,
                                         displayVector(frame, "normal"), view.offset * 1000.0,
                                         view.flip);
            return;
        }
    }
    viewport->setSectionClipping(false);
}

} // namespace cad

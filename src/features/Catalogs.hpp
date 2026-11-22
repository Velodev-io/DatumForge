#pragma once

#include "domain/GeometryReference.hpp"
#include <QCborArray>
#include <QCborMap>
#include <QString>
#include <array>
#include <optional>
#include <vector>

namespace cad::features {

// ============================================================================
// 1. Hole Catalog Resolution
// ============================================================================

struct HoleCatalogQuery {
    int version = 1;
    QString
        dataset; // "ansi_inch", "ansi_metric", "iso", "din", "bsi", "jis", "gb", "gost", "custom"
    QString standard;          // e.g. "B18.3.4M", "ISO 4762", "DIN 912"
    QString type;              // e.g. "SocketButtonHeadCapScrew", "SocketHeadCapScrew", "HexBolt"
    QString size;              // e.g. "M8", "1/4-20"
    QString fit;               // e.g. "Normal", "Close", "Loose", "Medium", "Fine", "Coarse"
    QCborMap custom_overrides; // optional explicit parameter overrides

    static HoleCatalogQuery parse(const QCborMap &map);
    QCborMap map() const;
};

struct HoleCatalogResolved {
    bool valid = true;
    QString dataset;
    QString standard;
    QString type;
    QString size;
    QString fit;
    QString family;        // "Simple", "Counterbore", "Countersink", "Drilled", "Tapped", "PipeTap"
    double diameter = 0.0; // SI meters
    std::optional<double> counterbore_diameter;
    std::optional<double> counterbore_depth;
    std::optional<double> countersink_diameter;
    std::optional<double> countersink_angle;
    std::optional<double> drill_angle;
    std::optional<double> tap_drill_diameter;
    std::optional<double> pitch;
    bool cosmetic_thread = false;
    QString thread_designation;
    QString diagnostic;

    QCborMap map() const;
    static HoleCatalogResolved parse(const QCborMap &map);
};

HoleCatalogResolved resolveHoleCatalog(const HoleCatalogQuery &query);
QStringList availableHoleDatasets();
QStringList availableHoleStandards(const QString &dataset);
QStringList availableHoleTypes(const QString &dataset, const QString &standard);
QStringList availableHoleSizes(const QString &dataset, const QString &standard,
                               const QString &type);
QStringList availableHoleFits(const QString &dataset, const QString &standard, const QString &type,
                              const QString &size);

// ============================================================================
// 2. Materials & Physical Properties
// ============================================================================

struct MaterialProperties {
    double density_kg_m3 = 0.0;
    std::optional<double> elastic_modulus_pa;
    std::optional<double> poissons_ratio;
    std::optional<double> shear_modulus_pa;
    std::optional<double> thermal_conductivity_w_mk;
    std::optional<double> specific_heat_j_kgk;
    std::optional<double> yield_strength_pa;
    std::optional<double> tensile_strength_pa;

    QCborMap map() const;
    static MaterialProperties parse(const QCborMap &map);
};

struct Material {
    int version = 1;
    QString id;
    QString name;
    QString family; // "Steel", "Aluminum", "Copper", "Titanium", "Iron", "Plastics", "Custom"
    MaterialProperties properties;
    QCborMap appearance; // optional visual properties

    QCborMap map() const;
    static Material parse(const QCborMap &map);
};

std::vector<Material> standardMaterials();
std::optional<Material> findMaterial(const QString &id);
QStringList availableMaterialFamilies();

// ============================================================================
// 3. Exact Mass Properties Evaluation
// ============================================================================

struct MassPropertiesRequest {
    qint64 revision = 0;
    QString scopeType = "Part";            // "Part", "Body", "AssemblyOccurrence"
    QString targetId;                      // body UUID or occurrence path
    QString frame = "Part";                // "Part", "CenterOfMass"
    std::optional<double> overrideDensity; // kg/m^3

    QCborMap map() const;
    static MassPropertiesRequest parse(const QCborMap &map);
};

struct MassPropertiesResult {
    qint64 revision = 0;
    bool valid = true;
    QString reason;
    int solids_count = 0;
    double volume_m3 = 0.0;
    double surface_area_m2 = 0.0;
    std::optional<double> density_kg_m3;
    bool density_missing = false;
    std::optional<double> mass_kg;
    std::array<double, 3> center_of_mass_m = {0, 0, 0};
    std::array<double, 6> inertia_tensor_kg_m2 = {0, 0, 0, 0, 0, 0}; // Ixx, Iyy, Izz, Ixy, Ixz, Iyz
    std::array<double, 3> principal_moments_kg_m2 = {0, 0, 0};       // I1 <= I2 <= I3
    std::array<std::array<double, 3>, 3> principal_axes = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

    QCborMap map() const;
    static MassPropertiesResult parse(const QCborMap &map);
};

// ============================================================================
// 4. Exact Planar Section Properties Evaluation
// ============================================================================

struct SectionPropertiesRequest {
    qint64 revision = 0;
    QString targetType = "Sketch"; // "Sketch", "Face"
    QString sketchId;
    QString sketchMode = "AllClosed";
    domain::GeometryReference faceReference;

    QCborMap map() const;
    static SectionPropertiesRequest parse(const QCborMap &map);
};

struct SectionPropertiesResult {
    qint64 revision = 0;
    bool valid = true;
    QString reason;
    double area_m2 = 0.0;
    double perimeter_m = 0.0;
    std::array<double, 2> centroid_local_m = {0, 0};
    std::array<double, 3> centroid_part_m = {0, 0, 0};
    std::array<double, 3> second_moments_m4 = {0, 0, 0}; // Iuu, Ivv, Iuv
    double polar_moment_m4 = 0.0;                        // J = Iuu + Ivv
    std::array<double, 2> principal_moments_m4 = {0, 0}; // I1, I2
    double principal_angle_rad = 0.0;

    QCborMap map() const;
    static SectionPropertiesResult parse(const QCborMap &map);
};

// ============================================================================
// 5. Appearance Precedence and Overrides
// ============================================================================

struct Appearance {
    std::array<float, 4> color_rgba = {0.8f, 0.8f, 0.8f, 1.0f};
    float roughness = 0.5f;
    float metalness = 0.0f;
    float transparency = 0.0f;

    QCborMap map() const;
    static Appearance parse(const QCborMap &map);
    bool operator==(const Appearance &other) const;
};

struct AppearanceOverride {
    QString id;
    QString level; // "Face", "Feature", "Body", "Part"
    QString target;
    domain::GeometryReference faceReference;
    Appearance appearance;

    QCborMap map() const;
    static AppearanceOverride parse(const QCborMap &map);
};

Appearance resolveAppearance(const std::optional<Appearance> &faceOverride,
                             const std::optional<Appearance> &featureOverride,
                             const std::optional<Appearance> &bodyOverride,
                             const std::optional<Appearance> &partOverride,
                             const std::optional<Appearance> &materialDefault);

// ============================================================================
// 6. Named Section Views
// ============================================================================

struct SectionView {
    int version = 1;
    QString id;
    QString name;
    bool active = true;
    domain::GeometryReference support; // role "Plane" or "Face"
    double offset = 0.0;               // signed meters
    bool flip = false;
    std::optional<std::array<float, 4>> cap_color_rgba;
    QString clip_mode = "ViewOnly";

    QCborMap map() const;
    static SectionView parse(const QCborMap &map);
};

} // namespace cad::features

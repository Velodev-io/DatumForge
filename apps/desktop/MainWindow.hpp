#pragma once
#include "CommandRegistry.hpp"
#include "diagnostics/Journal.hpp"
#include "domain/WorkspaceState.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QSettings>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <memory>
namespace cad {
class Client;
class Viewport;
class SketchPanel;
class FeaturePanel;
class HistoryPanel;
class AssemblyPanel;
class DrawingPanel;
class DrawingCanvas;
class MainWindow : public QMainWindow {
    Q_OBJECT
    CommandRegistry commands;
    std::unique_ptr<QSettings> settings;
    Client *client = nullptr;
    Viewport *viewport = nullptr;
    SketchPanel *sketchPanel = nullptr;
    FeaturePanel *featurePanel = nullptr;
    HistoryPanel *historyPanel = nullptr;
    AssemblyPanel *assemblyPanel = nullptr;
    DrawingPanel *drawingPanel = nullptr;
    DrawingCanvas *drawingCanvas = nullptr;
    std::unique_ptr<diagnostics::Journal> journal;
    void buildInteractionUI(QString settingsFile);
    void nameAccessibleWidgets();
    void buildDrawingUI();
    void refreshDrawingUI();
    void buildAssemblyUI();
    void refreshAssemblyUI();
    void refreshFeatures();
    QTreeWidget *tree = nullptr;
    QTabWidget *commandTabs = nullptr;
    QStackedWidget *propertyPages = nullptr;
    QByteArray treeProjection;
    QString treeActiveDocument;
    QLabel *editContext = nullptr;
    bool wasSketchEditing = false;
    bool exchangeShown = false;
    void buildCommandTabs();
    void refreshCommandTabs();
    QTabBar *documents = nullptr;
    QLabel *status = nullptr;
    QLabel *notice = nullptr;
    QLineEdit *search = nullptr;
    QDoubleSpinBox *widthInput = nullptr, *heightInput = nullptr, *depthInput = nullptr;
    QComboBox *filter = nullptr, *viewMode = nullptr;
    bool syncing = false, recoveryChecked = false;
    QLineEdit *documentName = nullptr, *parameterName = nullptr;
    QComboBox *documentUnits = nullptr, *parameterDimension = nullptr, *parameterUnit = nullptr;
    QDoubleSpinBox *parameterValue = nullptr;
    QLabel *documentInfo = nullptr;
    void buildExchangeUI();
    void buildProjectUI();
    void refreshProjectUI();
    bool confirmRecoverable(QString action);
    void chooseSave(bool saveAs = false);
    void showRecoveries(QCborArray offers);
    void buildFrame();
    void refresh();
    void rebuildTree();
    void choose(const Selection &selection);

  public:
    explicit MainWindow(QString settingsFile = {}, QWidget *parent = nullptr,
                        QString recoveryRoot = {});
    ~MainWindow() override;
    const WorkspaceState &state() const;
    Client *workerClient() const {
        return client;
    }
    Viewport *sceneView() const {
        return viewport;
    }
    QAction *command(QString id) const {
        return commands.get(id);
    }
    void saveLayout();

  protected:
    void closeEvent(QCloseEvent *event) override;
};
} // namespace cad

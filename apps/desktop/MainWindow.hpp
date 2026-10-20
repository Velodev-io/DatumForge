#pragma once
#include "CommandRegistry.hpp"
#include "domain/WorkspaceState.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QSettings>
#include <QTabBar>
#include <QTreeWidget>
#include <memory>
namespace cad {
class Client;
class Viewport;
class MainWindow : public QMainWindow {
    Q_OBJECT
    CommandRegistry commands;
    std::unique_ptr<QSettings> settings;
    Client *client = nullptr;
    Viewport *viewport = nullptr;
    QTreeWidget *tree = nullptr;
    QTabBar *documents = nullptr;
    QLabel *status = nullptr;
    QLabel *notice = nullptr;
    QLineEdit *search = nullptr;
    QDoubleSpinBox *widthInput = nullptr, *heightInput = nullptr, *depthInput = nullptr;
    QComboBox *filter = nullptr, *viewMode = nullptr;
    bool syncing = false;
    void buildFrame();
    void refresh();
    void rebuildTree();
    void choose(const Selection &selection);

  public:
    explicit MainWindow(QString settingsFile = {}, QWidget *parent = nullptr);
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

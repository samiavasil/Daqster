#pragma once

#include "FlowUiSection.h"
#include "NodeEditorWidget.h"
#include "QBasePluginObject.h"
#include <QtNodes/Definitions>

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QHash>
#include <vector>

namespace Daqster {
class IWidgetProvider;
}

class RuntimeShell : public Daqster::QBasePluginObject
{
    Q_OBJECT
public:
    RuntimeShell(QObject* parent = nullptr);
    virtual ~RuntimeShell();

    /// Entry point for runtime mode (REQ-SW-PL-048): loads `flowPath` and shows
    /// deembedded widgets arranged in MDI workspaces as the application UI.
    /// Returns true on success, false on error (error message already shown).
    bool RunRuntime(const QString& flowPath);

    /// No-op implementation for QBasePluginObject interface (runtime mode uses
    /// RunRuntime instead of Initialize).
    virtual bool Initialize() override { return true; }

protected:
    virtual void DeInitialize() override;

private slots:
    void onMainWindowDestroyed(QObject* obj);

private:
    // Initialization sequence
    bool registerNodes();
    bool buildCanvas();
    bool loadFlow(const QString& flowPath);
    void arrangeWorkspaces(const FlowUi::UiSection& ui);
    void autoStartNodes(const FlowUi::UiSection& ui);

    /// REQ-SW-PL-051: re-discover and register nodes from newly loaded
    /// INodeProvider plugins (connected to QPluginManager::PluginsListChangeDetected).
    void discoverAndRegisterExternalNodes();

    /// REQ-SW-PL-051: hands an IWidgetProvider to the editor widget and
    /// watches its plugin object, re-discovering if it dies.
    void adoptWidgetProvider(Daqster::IWidgetProvider* provider);

public:
    void stopAllNodes();
private:

    // Helpers
    QMdiSubWindow* createSubWindow(QWidget* widget, const FlowUi::Geometry& geometry);
    void clearWindowFlags(QWidget* widget);

    QMainWindow* m_mainWindow = nullptr;
    NodeEditorWidget* m_editorWidget = nullptr;
    std::vector<QMdiArea*> m_mdiAreas;  // One per workspace
    QHash<int, QMdiArea*> m_workspaceIdToMdiArea;
    QHash<QtNodes::NodeId, bool> m_autoStartNodes;
    std::vector<FlowUi::WorkspaceUi> m_workspaces;
};
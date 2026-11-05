#pragma once

#include "SceneOpen.h"

#include <QListWidget>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QToolButton;

/**
 * @brief Problems `ISceneOpening` reported for the scene that was just loaded
 *        (REQ-SW-PL-082).
 *
 * The IDE half of "loads, marks, does not start": the scene stays on the canvas
 * exactly as saved, the offending nodes carry a `NodeValidationState::Error`,
 * and this panel says why — in a list the user can act on. Double-click (or
 * Enter) on a row emits `problemActivated(nodeId)` so the host can select and
 * centre the node; the panel itself never touches the graph.
 *
 * Collapsed by default: one header row (`Проблеми в сцената (N)`) until the user
 * asks for the list, because a clean scene is the normal case and a permanently
 * open list would steal canvas space from it. The panel hides itself entirely
 * when there is nothing to report.
 *
 * Deliberately QtWidgets + QListWidget only: no QtNetwork (nothing is fetched
 * here) and no graph model (the ids are strings; the host resolves them).
 */
class SceneProblemsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SceneProblemsPanel(QWidget* parent = nullptr);
    ~SceneProblemsPanel() override;

    /// Fills the list, stores the node ids and shows/hides the panel.
    /// An empty vector hides the panel and clears the previous list.
    void setProblems(const QVector<Daqster::SceneOpenProblem>& problems);

    /// `"<nodeName> (<nodeId>): <message> — <hint>"`, empty parts left out —
    /// the same shape `sceneOpeningMessage()` uses for logs.
    static QString textForProblem(const Daqster::SceneOpenProblem& problem);

    int problemCount() const;
    /// The header text when something is reported, an empty string otherwise.
    QString headerText() const;
    /// Node ids in list order (empty string for a problem about the whole scene).
    QStringList nodeIds() const;

    QListWidget* listWidget() const { return m_list; }
    bool isCollapsed() const;

    void setCollapsed(bool collapsed);

    /// Emit `problemActivated()` for @p row, ignoring a row out of range.
    /// The same path double-click and Enter take, exposed for tests.
    void activateRow(int row);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    /// The user asked to see the node a problem is about. Empty `nodeId` for a
    /// scene-wide problem (the host then has nothing to select).
    void problemActivated(const QString& nodeId);

private:
    void toggleCollapsed();

    QToolButton* m_header = nullptr;
    QListWidget* m_list = nullptr;
    QStringList m_nodeIds;
};

#pragma once

#include "SceneOpen.h"

#include <QListWidget>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
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

    /// Installs the two halves of the "Поправи" affordance (REQ-SW-PL-085).
    ///
    /// @p label returns the button text for a problem, or an empty string when
    /// no fixer handles it (then the row has no button, exactly as before).
    /// @p fix performs the repair; a `false` return with a filled `error` is
    /// shown in the panel's own status line — never a modal dialog.
    ///
    /// Both callables are called on the GUI thread. Passing empty functions
    /// removes the buttons. The panel never keeps the problems it was given
    /// after a successful fix: it emits `fixRequested()` so the host refreshes
    /// the list from the scene.
    void setFixHandler(std::function<QString(const Daqster::SceneOpenProblem&)> label,
                       std::function<bool(const Daqster::SceneOpenProblem&, QString*)> fix);

    /// The failure text currently shown, empty when there is none. Exposed for
    /// tests; the same text is visible in the panel's status line.
    QString statusText() const;

    /// The "Поправи" button of @p row, or null when that row has no fixer.
    /// Exposed for tests (and for a host that wants to drive the button).
    QToolButton* fixButtonAt(int row) const;

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

    /// A fixer reported success for @p problem. The host re-checks the scene
    /// and refreshes the panel; the panel itself cannot know whether the
    /// problem is really gone.
    void fixRequested(const Daqster::SceneOpenProblem& problem);

private:
    void toggleCollapsed();
    /// Builds the row widget: the text plus, when a fixer handles the problem,
    /// a "Поправи" button. Keeps `item->text()` intact for logs and tests.
    void installRowWidget(QListWidgetItem* item, int index,
                          const Daqster::SceneOpenProblem& problem);
    void runFix(int index);
    void setStatus(const QString& text);

    QToolButton* m_header = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_status = nullptr;
    QStringList m_nodeIds;
    QVector<Daqster::SceneOpenProblem> m_problems;
    std::function<QString(const Daqster::SceneOpenProblem&)> m_fixLabel;
    std::function<bool(const Daqster::SceneOpenProblem&, QString*)> m_fix;
};

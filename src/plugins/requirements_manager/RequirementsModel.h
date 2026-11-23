#pragma once

#include <QAbstractItemModel>
#include <QColor>
#include <QDate>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Qt item model over a set of parsed requirements.
 *
 * The model supports two view modes:
 *   - Sections:  root "active"/"archive", flat requirement lists underneath.
 *   - Hierarchy: root "active"/"archive", requirements nested by their
 *                "Родител:" parent-child relationship.
 *
 * Columns: ID, Title, Status, Priority, Assignee, Repo, Създадено, Възраст.
 *
 * All data comes from the structured Requirement records produced by
 * RequirementsParser — the model never re-parses raw Markdown.
 *
 * @par Dates and age (REQ-SW-PL-100)
 * `Създадено` is the `- **Дата:**` field verbatim; `Възраст` is the whole-day
 * age rendered as "N дни" ("—" when the date is missing or invalid). The age
 * is also exposed numerically through AgeDaysRole, because "-1 последно" and
 * "12 дни" must sort numerically — the display text cannot. A DRAFT older than
 * RequirementDates::kStaleDraftDays is painted in the warning colour through
 * Qt::ForegroundRole: the same per-row-role mechanism the status colours use,
 * so the tree needs no delegate.
 */
class RequirementsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column {
        IdColumn = 0,
        TitleColumn,
        StatusColumn,
        PriorityColumn,
        AssigneeColumn,
        RepoColumn,
        CreatedColumn,  //!< "Създадено" — the - **Дата:** field (REQ-SW-PL-100)
        AgeColumn,      //!< "Възраст" — whole days since that date
        ColumnCount
    };

    enum Role {
        RequirementRole = Qt::UserRole + 1,
        SectionRole,
        AgeDaysRole,   //!< int: whole days since "Създадено"/"Дата", -1 when unknown
        DraftVerdictRole, //!< QString: the last verdict of the draft review table
                         //!< ("за корекция", "откажи") — empty when no verdict was
                         //!< recorded (REQ-SW-PL-102 AC4). Computed once, in
                         //!< setRequirements(), so the counter never parses in paint.
        DoneCandidateRole //!< bool: IMPLEMENTED with every DONE precondition met
                          //!< (REQ-SW-PL-107). Filled by DoneGate::evaluateLocal()
                          //!< plus the asynchronous develop answer
                          //!< (setDoneCandidates()) — the row badge and the
                          //!< counter only read it, so paint never asks git.
    };

    enum class ViewMode {
        Sections,  //!< flat list per section ("active"/"archive")
        Hierarchy  //!< nested tree following parentId
    };

    explicit RequirementsModel(QObject *parent = nullptr);

    void setRequirements(const QVector<Requirement> &requirements);

    /**
     * @brief Re-marks the „кандидат за DONE“ rows without rebuilding the tree.
     *
     * The develop fact is answered by git ASYNCHRONOUSLY, after the tree is
     * already on screen (REQ-SW-PL-107; a wait in the plugin is forbidden, see
     * check_no_event_loops.cmake). A model reset would drop the selection, the
     * expansion and the scroll position of a tree nobody asked to reload, so
     * only the badge/colour roles are refreshed in place.
     *
     * @param candidateIds IDs whose four preconditions hold; every other row
     *        stops being a candidate.
     */
    void setDoneCandidates(const QStringList &candidateIds);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    /**
     * @brief Sets the reference day the "Възраст" column is computed from.
     *
     * Defaults to QDate::currentDate(); a test (or another caller wanting a
     * frozen clock) overrides it so "90 дни" is reproducible.
     *
     * @param today Reference day; an invalid date is ignored.
     */
    void setToday(const QDate &today);

    /**
     * @brief Returns the warning colour of an overdue DRAFT row.
     *
     * Same value the validation report paints warnings with (#C07A00), so the
     * tree and the report agree on what "warning" looks like.
     */
    static QColor staleDraftColour();

    /**
     * @brief Returns the colour of a „кандидат за DONE“ row (REQ-SW-PL-107).
     *
     * A candidate is not a status: it is `IMPLEMENTED` with every precondition
     * met, waiting for the owner. The marker is the ID's `✓` plus this colour,
     * so the same row the action would accept is the one that looks ready.
     */
    static QColor doneCandidateColour();

    const Requirement *requirementAt(const QModelIndex &index) const;

    /**
     * @brief Finds the model index of a requirement by its ID in the
     *        current view mode. Returns an invalid index when not found.
     */
    QModelIndex indexForId(const QString &id) const;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    enum class Root { Active, Archive, Count };

    void rebuildHierarchy();
    int indexOfId(const QString &id) const;
    //!< immediate parent requirement index for reqIndex, or -1
    int immediateParentIndex(int reqIndex) const;
    //!< cycle-safe hierarchy parent (breaks parent cycles by returning -1)
    int resolveHierarchyParent(int reqIndex) const;
    //!< row of a requirement within its siblings under the given parent node
    int siblingRow(int reqIndex, int parentReqIndexOrMinusOne) const;
    //!< emits dataChanged for every requirement row under @p parent
    void emitRowsChanged(const QModelIndex &parent = QModelIndex());

    QVector<Requirement> m_requirements;
    QVector<QString> m_draftVerdicts; //!< parallel to m_requirements (DraftVerdictRole)
    QStringList m_sections; //!< ordered distinct sections ("active", "archive")
    ViewMode m_viewMode = ViewMode::Sections;
    QDate m_today = QDate::currentDate(); //!< reference day for the age column

    // Hierarchy mode caches
    QVector<int> m_hierarchyParent;        //!< per req index; -1 = top-level
    QVector<QVector<int>> m_children;      //!< per req index; direct children
    QVector<QVector<int>> m_topLevelBySection; //!< per section row; top-level req indexes
};

} // namespace Daqster

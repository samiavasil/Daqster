#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Flat table model for the Traceability Matrix view (REQ-SW-PL-010).
 *
 * Shows one row per parsed requirement with the columns that make up the
 * traceability matrix: ID, Title, Status, Priority, Parent, Dependencies,
 * Commits, Code, Tests, Section.
 *
 * Row filtering (AC2):
 *   - status filter:  "All" | "ACTIVE" | "DONE" (matched against req.status)
 *   - domain filter:  an id prefix such as "SW", "REQ-SW" or "REQ-SW-"
 *                     (empty = all). A bare domain code is normalized to
 *                     "REQ-<code>" before matching.
 *
 * All data comes from the structured Requirement records produced by
 * RequirementsParser — the model never re-parses raw Markdown.
 */
class TraceabilityMatrixModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        IdColumn = 0,
        TitleColumn,
        StatusColumn,
        PriorityColumn,
        ParentColumn,
        DependenciesColumn,
        CommitsColumn,
        CodeColumn,
        TestsColumn,
        SectionColumn,
        ColumnCount
    };

    enum Role {
        RequirementRole = Qt::UserRole + 1
    };

    explicit TraceabilityMatrixModel(QObject *parent = nullptr);

    void setRequirements(const QVector<Requirement> &requirements);

    /**
     * @brief Sets the status filter. "All" shows every requirement; any other
     *        value matches req.status exactly (e.g. "ACTIVE", "DONE").
     */
    void setStatusFilter(const QString &status);

    /**
     * @brief Sets the domain prefix filter. Empty = all. A bare code like "SW"
     *        is normalized to "REQ-SW" before prefix-matching the ID.
     */
    void setDomainFilter(const QString &prefix);

    QString statusFilter() const;
    QString domainFilter() const;

    /**
     * @brief Returns the requirement shown at the given visible row, or
     *        nullptr when the row is out of range.
     */
    const Requirement *requirementAt(int row) const;

    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void rebuildVisibleRows();

    QVector<Requirement> m_requirements; //!< full parsed set (unfiltered)
    QVector<int> m_visible;              //!< row indexes into m_requirements
    QString m_statusFilter;              //!< "All" or an exact status
    QString m_domainFilter;              //!< id prefix, empty = all
};

} // namespace Daqster

#pragma once

#include <QAbstractItemModel>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Qt item model over a set of parsed requirements.
 *
 * Tree structure:
 *   +- "active"  (root)
 *   |    +- REQ-FW-001  General Requirements Viewer/Editor Tool
 *   +- "archive" (root)
 *        +- ...
 *
 * Columns: ID, Title, Status, Priority, Assignee.
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
        ColumnCount
    };

    enum Role {
        RequirementRole = Qt::UserRole + 1,
        SectionRole
    };

    explicit RequirementsModel(QObject *parent = nullptr);

    void setRequirements(const QVector<Requirement> &requirements);

    const Requirement *requirementAt(const QModelIndex &index) const;

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

    QVector<Requirement> m_requirements;
    QStringList m_sections; //!< ordered distinct sections ("active", "archive")
};

} // namespace Daqster

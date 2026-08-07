#include "TraceabilityMatrixModel.h"

namespace Daqster {

namespace {

// Display value for an empty matrix cell; matches the "—" used across the
// requirements tooling and in traceability-matrix.md.
QString matrixCell(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value;
}

} // namespace

TraceabilityMatrixModel::TraceabilityMatrixModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_statusFilter(QStringLiteral("All"))
{
}

void TraceabilityMatrixModel::setRequirements(const QVector<Requirement> &requirements)
{
    beginResetModel();
    m_requirements = requirements;
    rebuildVisibleRows();
    endResetModel();
}

void TraceabilityMatrixModel::setStatusFilter(const QString &status)
{
    if (m_statusFilter == status)
        return;
    beginResetModel();
    m_statusFilter = status.trimmed();
    rebuildVisibleRows();
    endResetModel();
}

void TraceabilityMatrixModel::setDomainFilter(const QString &prefix)
{
    if (m_domainFilter == prefix)
        return;
    beginResetModel();
    m_domainFilter = prefix.trimmed();
    rebuildVisibleRows();
    endResetModel();
}

QString TraceabilityMatrixModel::statusFilter() const
{
    return m_statusFilter;
}

QString TraceabilityMatrixModel::domainFilter() const
{
    return m_domainFilter;
}

const Requirement *TraceabilityMatrixModel::requirementAt(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return nullptr;
    return &m_requirements.at(m_visible.at(row));
}

void TraceabilityMatrixModel::rebuildVisibleRows()
{
    m_visible.clear();

    QString normalizedDomain = m_domainFilter;
    if (!normalizedDomain.isEmpty()
        && !normalizedDomain.startsWith(QStringLiteral("REQ-"),
                                        Qt::CaseInsensitive)) {
        normalizedDomain.prepend(QStringLiteral("REQ-"));
    }

    for (int i = 0; i < m_requirements.size(); ++i) {
        const Requirement &req = m_requirements.at(i);

        if (m_statusFilter != QStringLiteral("All")
            && req.status != m_statusFilter)
            continue;

        if (!normalizedDomain.isEmpty()
            && !req.id.startsWith(normalizedDomain, Qt::CaseInsensitive))
            continue;

        m_visible.append(i);
    }
}

int TraceabilityMatrixModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visible.size();
}

int TraceabilityMatrixModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant TraceabilityMatrixModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const Requirement *req = requirementAt(index.row());
    if (!req)
        return QVariant();

    if (role == RequirementRole)
        return QVariant::fromValue(*req);

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    switch (index.column()) {
    case IdColumn:          return req->id;
    case TitleColumn:       return req->title;
    case StatusColumn:      return req->status;
    case PriorityColumn:    return req->priority;
    case ParentColumn:      return matrixCell(req->parentId);
    case DependenciesColumn: return matrixCell(req->dependencies.join(QStringLiteral(", ")));
    case CommitsColumn:     return matrixCell(req->commits);
    case CodeColumn:        return matrixCell(req->code);
    case TestsColumn:       return matrixCell(req->tests);
    case SectionColumn:     return req->section;
    case RepoColumn:        return req->repo;
    default:                return QVariant();
    }
}

QVariant TraceabilityMatrixModel::headerData(int section, Qt::Orientation orientation,
                                             int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case IdColumn:          return QStringLiteral("ID");
    case TitleColumn:       return QStringLiteral("Title");
    case StatusColumn:      return QStringLiteral("Status");
    case PriorityColumn:    return QStringLiteral("Priority");
    case ParentColumn:      return QStringLiteral("Parent");
    case DependenciesColumn: return QStringLiteral("Dependencies");
    case CommitsColumn:     return QStringLiteral("Commits");
    case CodeColumn:        return QStringLiteral("Code");
    case TestsColumn:       return QStringLiteral("Tests");
    case SectionColumn:     return QStringLiteral("Section");
    case RepoColumn:        return QStringLiteral("Repo");
    default:                return QVariant();
    }
}

} // namespace Daqster

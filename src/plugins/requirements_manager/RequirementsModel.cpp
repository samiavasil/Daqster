#include "RequirementsModel.h"

namespace Daqster {

namespace {

constexpr int RootItemInternal = 0; //!< internal "id" of a root (section) item
constexpr int RequirementInternalBase = 1; //!< offset for requirement items

} // namespace

RequirementsModel::RequirementsModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void RequirementsModel::setRequirements(const QVector<Requirement> &requirements)
{
    beginResetModel();
    m_requirements = requirements;

    QStringList sections;
    for (const Requirement &req : m_requirements) {
        if (!sections.contains(req.section))
            sections.append(req.section);
    }
    std::sort(sections.begin(), sections.end());
    m_sections = sections;
    endResetModel();
}

const Requirement *RequirementsModel::requirementAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    const int internalId = index.internalId();
    if (internalId < RequirementInternalBase)
        return nullptr;
    const int reqIndex = internalId - RequirementInternalBase;
    if (reqIndex < 0 || reqIndex >= m_requirements.size())
        return nullptr;
    return &m_requirements.at(reqIndex);
}

QModelIndex RequirementsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= ColumnCount)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row >= m_sections.size())
            return QModelIndex();
        return createIndex(row, column, RootItemInternal);
    }

    const int sectionRow = parent.row();
    if (sectionRow < 0 || sectionRow >= m_sections.size())
        return QModelIndex();

    int reqIndex = 0;
    const QString section = m_sections.at(sectionRow);
    for (int i = 0; i < m_requirements.size(); ++i) {
        if (m_requirements.at(i).section == section) {
            if (reqIndex == row)
                return createIndex(row, column, RequirementInternalBase + i);
            ++reqIndex;
        }
    }
    return QModelIndex();
}

QModelIndex RequirementsModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();
    const int internalId = child.internalId();
    if (internalId < RequirementInternalBase)
        return QModelIndex();

    const Requirement &req = m_requirements.at(internalId - RequirementInternalBase);
    const int sectionRow = m_sections.indexOf(req.section);
    if (sectionRow < 0)
        return QModelIndex();
    return createIndex(sectionRow, 0, RootItemInternal);
}

int RequirementsModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_sections.size();

    const int internalId = parent.internalId();
    if (internalId == RootItemInternal) {
        const QString section = m_sections.at(parent.row());
        int count = 0;
        for (const Requirement &req : m_requirements) {
            if (req.section == section)
                ++count;
        }
        return count;
    }
    return 0;
}

int RequirementsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant RequirementsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const int internalId = index.internalId();
    if (internalId == RootItemInternal) {
        if (role == Qt::DisplayRole && index.column() == IdColumn)
            return m_sections.at(index.row());
        if (role == SectionRole)
            return m_sections.at(index.row());
        return QVariant();
    }

    const Requirement *req = requirementAt(index);
    if (!req)
        return QVariant();

    if (role == RequirementRole)
        return QVariant::fromValue(*req);
    if (role == SectionRole)
        return req->section;

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    switch (index.column()) {
    case IdColumn:        return req->id;
    case TitleColumn:     return req->title;
    case StatusColumn:    return req->status;
    case PriorityColumn:  return req->priority;
    case AssigneeColumn:  return req->assignee;
    default:              return QVariant();
    }
}

QVariant RequirementsModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case IdColumn:       return QStringLiteral("ID");
    case TitleColumn:    return QStringLiteral("Title");
    case StatusColumn:   return QStringLiteral("Status");
    case PriorityColumn: return QStringLiteral("Priority");
    case AssigneeColumn: return QStringLiteral("Assignee");
    default:             return QVariant();
    }
}

} // namespace Daqster

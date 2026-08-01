#include "RequirementsModel.h"

#include <QSet>

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

    rebuildHierarchy();
    endResetModel();
}

void RequirementsModel::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode)
        return;
    beginResetModel();
    m_viewMode = mode;
    endResetModel();
}

RequirementsModel::ViewMode RequirementsModel::viewMode() const
{
    return m_viewMode;
}

void RequirementsModel::rebuildHierarchy()
{
    const int n = m_requirements.size();
    m_hierarchyParent.fill(-1, n);
    m_children.clear();
    m_children.resize(n);
    m_topLevelBySection.clear();
    m_topLevelBySection.resize(m_sections.size());

    for (int i = 0; i < n; ++i)
        m_hierarchyParent[i] = resolveHierarchyParent(i);

    for (int i = 0; i < n; ++i) {
        const int parent = m_hierarchyParent.at(i);
        if (parent >= 0) {
            m_children[parent].append(i);
        } else {
            const int sectionRow = m_sections.indexOf(m_requirements.at(i).section);
            if (sectionRow >= 0)
                m_topLevelBySection[sectionRow].append(i);
        }
    }
}

int RequirementsModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_requirements.size(); ++i) {
        if (QString::compare(m_requirements.at(i).id, id, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

int RequirementsModel::immediateParentIndex(int reqIndex) const
{
    const QString parentId = m_requirements.at(reqIndex).parentId;
    if (parentId.trimmed().isEmpty())
        return -1;
    if (parentId == m_requirements.at(reqIndex).id)
        return -1;
    const int parentIdx = indexOfId(parentId);
    if (parentIdx < 0)
        return -1;
    // Only nest within the same section; cross-section parents stay top-level.
    if (m_requirements.at(reqIndex).section != m_requirements.at(parentIdx).section)
        return -1;
    return parentIdx;
}

int RequirementsModel::resolveHierarchyParent(int reqIndex) const
{
    // Walk the immediate-parent chain. If the chain revisits a node, a parent
    // cycle exists — break it by treating the node as top-level (the validator
    // reports the cycle as an error separately).
    QSet<int> seen;
    seen.insert(reqIndex);
    int cursor = reqIndex;
    int parent = immediateParentIndex(cursor);
    while (parent != -1) {
        if (seen.contains(parent))
            return -1;
        seen.insert(parent);
        cursor = parent;
        parent = immediateParentIndex(cursor);
    }
    return immediateParentIndex(reqIndex);
}

int RequirementsModel::siblingRow(int reqIndex, int parentReqIndexOrMinusOne) const
{
    const QVector<int> *siblings = nullptr;
    if (parentReqIndexOrMinusOne < 0) {
        const int sectionRow = m_sections.indexOf(m_requirements.at(reqIndex).section);
        if (sectionRow < 0)
            return -1;
        siblings = &m_topLevelBySection.at(sectionRow);
    } else {
        siblings = &m_children.at(parentReqIndexOrMinusOne);
    }
    return siblings->indexOf(reqIndex);
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

    const int parentInternalId = parent.internalId();

    if (parentInternalId == RootItemInternal) {
        const int sectionRow = parent.row();
        if (sectionRow < 0 || sectionRow >= m_sections.size())
            return QModelIndex();
        const QString section = m_sections.at(sectionRow);

        if (m_viewMode == ViewMode::Sections) {
            int reqIndex = 0;
            for (int i = 0; i < m_requirements.size(); ++i) {
                if (m_requirements.at(i).section == section) {
                    if (reqIndex == row)
                        return createIndex(row, column, RequirementInternalBase + i);
                    ++reqIndex;
                }
            }
            return QModelIndex();
        }

        if (row >= m_topLevelBySection.at(sectionRow).size())
            return QModelIndex();
        return createIndex(row, column,
                           RequirementInternalBase + m_topLevelBySection.at(sectionRow).at(row));
    }

    // Parent is a requirement node: only hierarchy mode has children.
    if (m_viewMode != ViewMode::Hierarchy)
        return QModelIndex();
    const int parentReqIndex = parentInternalId - RequirementInternalBase;
    if (parentReqIndex < 0 || parentReqIndex >= m_requirements.size())
        return QModelIndex();
    const QVector<int> &children = m_children.at(parentReqIndex);
    if (row >= children.size())
        return QModelIndex();
    return createIndex(row, column, RequirementInternalBase + children.at(row));
}

QModelIndex RequirementsModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();
    const int internalId = child.internalId();
    if (internalId < RequirementInternalBase)
        return QModelIndex();

    const int reqIndex = internalId - RequirementInternalBase;
    if (reqIndex < 0 || reqIndex >= m_requirements.size())
        return QModelIndex();
    const Requirement &req = m_requirements.at(reqIndex);

    if (m_viewMode == ViewMode::Sections) {
        const int sectionRow = m_sections.indexOf(req.section);
        if (sectionRow < 0)
            return QModelIndex();
        return createIndex(sectionRow, 0, RootItemInternal);
    }

    const int parentIdx = m_hierarchyParent.at(reqIndex);
    if (parentIdx < 0) {
        const int sectionRow = m_sections.indexOf(req.section);
        if (sectionRow < 0)
            return QModelIndex();
        return createIndex(sectionRow, 0, RootItemInternal);
    }
    const int row = siblingRow(parentIdx, m_hierarchyParent.at(parentIdx));
    if (row < 0)
        return QModelIndex();
    return createIndex(row, 0, RequirementInternalBase + parentIdx);
}

int RequirementsModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_sections.size();

    const int internalId = parent.internalId();

    if (internalId == RootItemInternal) {
        const int sectionRow = parent.row();
        if (sectionRow < 0 || sectionRow >= m_sections.size())
            return 0;
        if (m_viewMode == ViewMode::Sections) {
            const QString section = m_sections.at(sectionRow);
            int count = 0;
            for (const Requirement &req : m_requirements) {
                if (req.section == section)
                    ++count;
            }
            return count;
        }
        return m_topLevelBySection.at(sectionRow).size();
    }

    if (m_viewMode != ViewMode::Hierarchy)
        return 0;
    const int reqIndex = internalId - RequirementInternalBase;
    if (reqIndex < 0 || reqIndex >= m_requirements.size())
        return 0;
    return m_children.at(reqIndex).size();
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

QModelIndex RequirementsModel::indexForId(const QString &id) const
{
    const int targetIdx = indexOfId(id);
    if (targetIdx < 0)
        return QModelIndex();

    const Requirement &target = m_requirements.at(targetIdx);
    const int sectionRow = m_sections.indexOf(target.section);
    if (sectionRow < 0)
        return QModelIndex();

    if (m_viewMode == ViewMode::Sections) {
        int row = 0;
        for (int i = 0; i < m_requirements.size(); ++i) {
            if (m_requirements.at(i).section == target.section) {
                if (i == targetIdx)
                    return createIndex(row, 0, RequirementInternalBase + targetIdx);
                ++row;
            }
        }
        return QModelIndex();
    }

    // Hierarchy: build the chain from the top-level node down to target.
    QVector<int> chain;
    int cursor = targetIdx;
    while (cursor >= 0) {
        chain.prepend(cursor);
        cursor = m_hierarchyParent.at(cursor);
    }

    QModelIndex parentIndex; // invalid = root
    for (int depth = 0; depth < chain.size(); ++depth) {
        const int nodeIdx = chain.at(depth);
        const int parentNode = depth == 0 ? -1 : chain.at(depth - 1);
        const int row = siblingRow(nodeIdx, parentNode);
        if (row < 0)
            return QModelIndex();
        parentIndex = createIndex(row, 0, RequirementInternalBase + nodeIdx);
    }
    return parentIndex;
}

} // namespace Daqster

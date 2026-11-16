#include "IdeaModel.h"

namespace Daqster {

IdeaTableModel::IdeaTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void IdeaTableModel::setIdeas(const QVector<Idea> &ideas)
{
    beginResetModel();
    m_ideas = ideas;
    endResetModel();
}

const QVector<Idea> &IdeaTableModel::ideas() const
{
    return m_ideas;
}

const Idea *IdeaTableModel::ideaAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_ideas.size())
        return nullptr;
    return &m_ideas.at(index.row());
}

int IdeaTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_ideas.size();
}

int IdeaTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant IdeaTableModel::data(const QModelIndex &index, int role) const
{
    const Idea *idea = ideaAt(index);
    if (!idea)
        return QVariant();

    if (role == IdeaRole)
        return QVariant::fromValue(*idea);

    if (role == Qt::ToolTipRole)
        return idea->path;

    if (role == Qt::TextAlignmentRole && index.column() == AttachmentsColumn)
        return QVariant(int(Qt::AlignRight | Qt::AlignVCenter));

    if (role != Qt::DisplayRole)
        return QVariant();

    switch (index.column()) {
    case IdColumn:
        return idea->id;
    case TitleColumn:
        return idea->title;
    case StatusColumn:
        return idea->status;
    case AddedColumn:
        return idea->added;
    case NextReviewColumn:
        return idea->nextReview;
    case AttachmentsColumn:
        return idea->attachmentCount;
    default:
        return QVariant();
    }
}

QVariant IdeaTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return section + 1;

    switch (section) {
    case IdColumn:
        return tr("ID");
    case TitleColumn:
        return tr("Заглавие");
    case StatusColumn:
        return tr("Статус");
    case AddedColumn:
        return tr("Добавена");
    case NextReviewColumn:
        return tr("Следващ преглед");
    case AttachmentsColumn:
        return tr("Приложения");
    default:
        return QVariant();
    }
}

IdeaFilterProxyModel::IdeaFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void IdeaFilterProxyModel::setQuery(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (m_query == trimmed)
        return;
    m_query = trimmed;
    invalidateFilter();
}

QString IdeaFilterProxyModel::query() const
{
    return m_query;
}

bool IdeaFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_query.isEmpty())
        return true;

    const QAbstractItemModel *model = sourceModel();
    if (!model)
        return true;

    for (int column : {IdeaTableModel::IdColumn, IdeaTableModel::TitleColumn}) {
        const QString value = model->index(sourceRow, column, sourceParent)
                                  .data(Qt::DisplayRole).toString();
        if (value.contains(m_query, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace Daqster

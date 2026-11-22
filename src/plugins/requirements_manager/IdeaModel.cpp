#include "IdeaModel.h"

#include "RequirementsValidator.h"

#include <QBrush>
#include <QColor>
#include <QFont>

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

void IdeaTableModel::setRequirements(const QVector<Requirement> &requirements)
{
    QHash<QString, QString> statuses;
    for (const Requirement &req : requirements) {
        const QString bare = RequirementsValidator::normalizedStatus(req.status);
        statuses.insert(req.id.trimmed().toUpper(),
                        bare.isEmpty() ? req.status.trimmed() : bare);
    }
    if (statuses == m_statusById)
        return;
    m_statusById = statuses;
    // Only the "Изискване" column changes: no reset, the selection survives.
    if (!m_ideas.isEmpty())
        emit dataChanged(index(0, RequirementColumn),
                         index(m_ideas.size() - 1, RequirementColumn));
}

QString IdeaTableModel::promotedRequirementAt(const QModelIndex &index) const
{
    const Idea *idea = ideaAt(index);
    return idea ? idea->promotedId.trimmed() : QString();
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

    if (role == RequirementRole)
        return idea->promotedId.trimmed();

    // The promotion cell is a link: underlined and blue only when it leads
    // somewhere, so an empty cell cannot look clickable (REQ-SW-PL-098).
    const bool hasPromotion = index.column() == RequirementColumn
                              && !idea->promotedId.trimmed().isEmpty();
    if (role == Qt::ForegroundRole && hasPromotion)
        return QBrush(QColor(0x1565C0)); // blue 800, same tone as the IDE
    if (role == Qt::FontRole && hasPromotion) {
        QFont font;
        font.setUnderline(true);
        return font;
    }

    if (role == Qt::ToolTipRole) {
        if (index.column() == RequirementColumn && hasPromotion)
            return tr("Отваря %1").arg(idea->promotedId.trimmed());
        return idea->path;
    }

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
    case RequirementColumn: {
        const QString promoted = idea->promotedId.trimmed();
        if (promoted.isEmpty())
            return QString();
        const QString status = m_statusById.value(promoted.toUpper());
        return status.isEmpty() ? promoted
                                : QStringLiteral("%1 · %2").arg(promoted, status);
    }
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
    case RequirementColumn:
        return tr("Изискване");
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

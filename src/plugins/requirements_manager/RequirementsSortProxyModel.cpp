#include "RequirementsSortProxyModel.h"

namespace Daqster {

RequirementsSortProxyModel::RequirementsSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void RequirementsSortProxyModel::sort(int column, Qt::SortOrder order)
{
    m_direction = order;
    QSortFilterProxyModel::sort(column, order);
}

bool RequirementsSortProxyModel::lessThan(const QModelIndex &left,
                                          const QModelIndex &right) const
{
    if (!left.isValid() || !right.isValid())
        return false;

    const QModelIndex leftParent = left.parent();
    const QModelIndex rightParent = right.parent();

    // Section rows (top level): never reorder them — "active" before "archive"
    // as setRequirements() sorted them. QSortFilterProxyModel applies the
    // descending direction to the top-level list as well (argument swap or a
    // per-parent reversal), so the comparison is made *against* the requested
    // direction; the two flips cancel and the sections stay put while the
    // requirements inside them sort normally.
    if (!leftParent.isValid() && !rightParent.isValid()) {
        return m_direction == Qt::AscendingOrder ? left.row() < right.row()
                                                 : right.row() < left.row();
    }

    // Different parents: order by the parent's row so children never migrate
    // out of their section/parent (the view sorts siblings only, this is a
    // safety net for proxy implementations that compare across parents).
    if (leftParent != rightParent) {
        const int leftParentRow = leftParent.isValid() ? leftParent.row() : -1;
        const int rightParentRow = rightParent.isValid() ? rightParent.row() : -1;
        return leftParentRow < rightParentRow;
    }

    const QString leftValue = left.data(Qt::DisplayRole).toString();
    const QString rightValue = right.data(Qt::DisplayRole).toString();
    const int cmp = QString::compare(leftValue, rightValue, Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;

    // Equal keys: "before" in both directions, so the proxy's stable sort keeps
    // the model's order instead of reversing it on a descending pass.
    return false;
}

} // namespace Daqster

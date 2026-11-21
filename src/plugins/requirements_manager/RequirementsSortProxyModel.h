#pragma once

#include <QSortFilterProxyModel>

namespace Daqster {

/**
 * @brief Sorting proxy for the requirements tree (REQ-SW-PL-001).
 *
 * The source (RequirementsModel) is a two-level-or-deeper tree whose top-level
 * rows are the scene sections ("active", "archive"). Sorting must therefore
 * happen *within* a section: the sections stay where the source model put them
 * and never become a sorted list of their own, otherwise the tree structure
 * (section -> requirements) is destroyed.
 *
 * Column choice follows the view: whatever column the user clicks in
 * QTreeView sorts the requirements, while the section rows keep their source
 * order.
 *
 * The "Възраст" column is compared numerically through
 * RequirementsModel::AgeDaysRole, not by its display text ("12 дни" < "9 дни"
 * alphabetically) — see REQ-SW-PL-100.
 */
class RequirementsSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit RequirementsSortProxyModel(QObject *parent = nullptr);

    /**
     * @brief Sorts, remembering the direction for the section-row comparison.
     *
     * The base proxy flips its own direction handling for descending sorts
     * (arguments swap, or the per-parent list is reversed), which would also
     * flip the top-level section rows. Section rows therefore compare their
     * source row *against* the requested direction, cancelling the flip: the
     * sections keep the model's order and only the requirements move.
     *
     * @param column Source column to sort by.
     * @param order Requested order for the requirement rows.
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

protected:
    /**
     * @brief Compares two rows, keeping sections in source order.
     *
     * @param left Left source index.
     * @param right Right source index.
     * @return true when @p left sorts before @p right.
     */
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    Qt::SortOrder m_direction = Qt::AscendingOrder; //!< last requested order
};

} // namespace Daqster

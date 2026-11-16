#pragma once

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QVector>

#include "IdeaReader.h"

namespace Daqster {

/**
 * @brief Flat table model over the parsed ideas (read-only).
 *
 * Columns: ID | Заглавие | Статус | Добавена | Следващ преглед | Приложения.
 *
 * The model holds no Requirement records; sorting and filtering are done by
 * IdeaFilterProxyModel in front of it, the same shape as
 * RequirementsSortProxyModel in front of RequirementsModel.
 */
class IdeaTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        IdColumn = 0,
        TitleColumn,
        StatusColumn,
        AddedColumn,
        NextReviewColumn,
        AttachmentsColumn,
        ColumnCount
    };

    enum Role {
        IdeaRole = Qt::UserRole + 1 //!< the whole Idea for the row (preview)
    };

    explicit IdeaTableModel(QObject *parent = nullptr);

    void setIdeas(const QVector<Idea> &ideas);
    const QVector<Idea> &ideas() const;
    //!< Source-model idea behind @p index, or nullptr for an invalid index.
    const Idea *ideaAt(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVector<Idea> m_ideas;
};

/**
 * @brief Sorting + id/title search over IdeaTableModel.
 *
 * Sorting is plain column sorting (no sections to protect); the filter narrows
 * the table to ideas whose ID or title contains the query, case-insensitively.
 */
class IdeaFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit IdeaFilterProxyModel(QObject *parent = nullptr);

    void setQuery(const QString &query);
    QString query() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_query;
};

} // namespace Daqster

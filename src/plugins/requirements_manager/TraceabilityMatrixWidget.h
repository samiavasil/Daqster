#pragma once

#include <QWidget>
#include <QVector>
#include "MatrixExporter.h"
#include "RequirementsParser.h"

class QTableView;
class QComboBox;
class QLabel;
class QPushButton;
class QTextBrowser;
class QSortFilterProxyModel;

namespace Daqster {

class TraceabilityMatrixModel;

/**
 * @brief Traceability Matrix view + export (REQ-SW-PL-010).
 *
 * Shows every requirement in a flat table (ID, Title, Status, Priority,
 * Parent, Dependencies, Commits, Code, Tests, Section) with live filtering by
 * status (All/ACTIVE/DONE) and domain prefix (distinct "REQ-<X>-" prefixes of
 * the parsed IDs).
 *
 * The metrics report (buildSummary(), AC4) lives in a collapsible read-only
 * QTextBrowser above the table instead of a word-wrapped label: it can be
 * selected, copied („Копирай“) and — the reason it is collapsible — it no
 * longer eats the table's height. It starts collapsed on narrow windows and
 * expands on wide ones until the user toggles it by hand (REQ-SW-PL-001).
 *
 * Every column is sortable through a QSortFilterProxyModel; the model keeps
 * its own status/domain filtering.
 *
 * Three export buttons write the matrix to disk through QFileDialog + QFile:
 * Markdown report, RFC 4180 CSV, and JSON (AC3 + AC5).
 */
class TraceabilityMatrixWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TraceabilityMatrixWidget(QWidget *parent = nullptr);

    /**
     * @brief Rebuilds the matrix, the domain filter options and the metrics
     *        report from the given requirements.
     */
    void setRequirements(const QVector<Requirement> &requirements);

    /**
     * @brief Shows/hides the metrics panel.
     *
     * @param expanded true = visible, false = collapsed to just the toggle.
     */
    void setMetricsExpanded(bool expanded);

    //!< Current metrics panel state.
    bool metricsExpanded() const;

    //!< The metrics text (the clipboard content of „Копирай“).
    QString metricsText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onStatusFilterChanged(int index);
    void onDomainFilterChanged(int index);
    void onExportMarkdown();
    void onExportCsv();
    void onExportJson();
    void onToggleMetrics();
    void onCopyMetrics();

private:
    void refreshDomainCombo();
    //!< Collapse/expand the metrics panel from the current width (no-op once
    //!< the user has toggled it by hand).
    void applyMetricsWidthPolicy();
    void exportToFile(const QString &filePath, MatrixExporter::ExportFunction exporter);

    TraceabilityMatrixModel *m_model;
    QSortFilterProxyModel *m_sortProxy;
    QTableView *m_table;
    QComboBox *m_statusCombo;
    QComboBox *m_domainCombo;
    QTextBrowser *m_metricsView;
    QPushButton *m_metricsToggle;
    QPushButton *m_metricsCopy;
    QLabel *m_statusLabel;
    QPushButton *m_markdownButton;
    QPushButton *m_csvButton;
    QPushButton *m_jsonButton;

    QVector<Requirement> m_requirements;
    bool m_metricsExpanded = false;
    bool m_metricsUserToggled = false;
};

} // namespace Daqster

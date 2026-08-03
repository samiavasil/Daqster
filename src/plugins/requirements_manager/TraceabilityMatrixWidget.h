#pragma once

#include <QWidget>
#include <QVector>
#include "MatrixExporter.h"
#include "RequirementsParser.h"

class QTableView;
class QComboBox;
class QLabel;
class QPushButton;

namespace Daqster {

class TraceabilityMatrixModel;

/**
 * @brief Traceability Matrix view + export (REQ-SW-PL-010).
 *
 * Shows every requirement in a flat table (ID, Title, Status, Priority,
 * Parent, Dependencies, Commits, Code, Tests, Section) with live filtering by
 * status (All/ACTIVE/DONE) and domain prefix (distinct "REQ-<X>-" prefixes of
 * the parsed IDs). A metrics label reports the buildSummary() report (AC4).
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

private slots:
    void onStatusFilterChanged(int index);
    void onDomainFilterChanged(int index);
    void onExportMarkdown();
    void onExportCsv();
    void onExportJson();

private:
    void refreshDomainCombo();
    void exportToFile(const QString &filePath, MatrixExporter::ExportFunction exporter);

    TraceabilityMatrixModel *m_model;
    QTableView *m_table;
    QComboBox *m_statusCombo;
    QComboBox *m_domainCombo;
    QLabel *m_metricsLabel;
    QLabel *m_statusLabel;
    QPushButton *m_markdownButton;
    QPushButton *m_csvButton;
    QPushButton *m_jsonButton;

    QVector<Requirement> m_requirements;
};

} // namespace Daqster

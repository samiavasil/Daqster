#include "TraceabilityMatrixWidget.h"

#include "TraceabilityMatrixModel.h"
#include "LogCategories.h"

#include <QTableView>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QHeaderView>
#include <QAbstractItemView>

#include <algorithm>

namespace Daqster {

TraceabilityMatrixWidget::TraceabilityMatrixWidget(QWidget *parent)
    : QWidget(parent)
{
    m_model = new TraceabilityMatrixModel(this);

    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItem(tr("All"), QStringLiteral("All"));
    m_statusCombo->addItem(QStringLiteral("ACTIVE"), QStringLiteral("ACTIVE"));
    m_statusCombo->addItem(QStringLiteral("DONE"), QStringLiteral("DONE"));

    m_domainCombo = new QComboBox(this);
    m_domainCombo->addItem(tr("All domains"), QString());

    m_metricsLabel = new QLabel(this);
    m_metricsLabel->setWordWrap(true);

    m_markdownButton = new QPushButton(tr("Export Markdown"), this);
    m_csvButton = new QPushButton(tr("Export CSV"), this);
    m_jsonButton = new QPushButton(tr("Export JSON"), this);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    QHBoxLayout *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Status:"), this));
    filterRow->addWidget(m_statusCombo);
    filterRow->addWidget(new QLabel(tr("Domain:"), this));
    filterRow->addWidget(m_domainCombo);
    filterRow->addStretch();

    QHBoxLayout *exportRow = new QHBoxLayout;
    exportRow->addWidget(m_markdownButton);
    exportRow->addWidget(m_csvButton);
    exportRow->addWidget(m_jsonButton);
    exportRow->addStretch();
    exportRow->addWidget(m_statusLabel);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(filterRow);
    layout->addWidget(m_metricsLabel);
    layout->addWidget(m_table, 1);
    layout->addLayout(exportRow);
    setLayout(layout);

    connect(m_statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TraceabilityMatrixWidget::onStatusFilterChanged);
    connect(m_domainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TraceabilityMatrixWidget::onDomainFilterChanged);
    connect(m_markdownButton, &QPushButton::clicked,
            this, &TraceabilityMatrixWidget::onExportMarkdown);
    connect(m_csvButton, &QPushButton::clicked,
            this, &TraceabilityMatrixWidget::onExportCsv);
    connect(m_jsonButton, &QPushButton::clicked,
            this, &TraceabilityMatrixWidget::onExportJson);
}

void TraceabilityMatrixWidget::setRequirements(const QVector<Requirement> &requirements)
{
    m_requirements = requirements;
    m_model->setRequirements(m_requirements);
    refreshDomainCombo();
    m_metricsLabel->setText(MatrixExporter::buildSummary(m_requirements));
    m_statusLabel->clear();
    qCInfo(lcFramework) << "TraceabilityMatrixWidget: showing"
                        << m_requirements.size() << "requirements";
}

void TraceabilityMatrixWidget::refreshDomainCombo()
{
    const QString previous = m_domainCombo->currentData().toString();

    QStringList prefixes;
    for (const Requirement &req : m_requirements) {
        const QString &id = req.id;
        const int first = id.indexOf(QLatin1Char('-'));
        const int second = id.indexOf(QLatin1Char('-'), first + 1);
        if (second < 0)
            continue;
        // Extract the domain prefix up to the THIRD dash when present so the
        // typed scheme gets distinct filters: "REQ-SW-PL-", "REQ-SW-FW-",
        // "REQ-SW-APP-", "REQ-SW-BLD-". 3-segment IDs ("REQ-PLG-001") fall
        // back to the second dash: "REQ-PLG-".
        const int third = id.indexOf(QLatin1Char('-'), second + 1);
        const QString prefix =
            (third >= 0) ? id.left(third + 1) : id.left(second + 1);
        if (!prefixes.contains(prefix))
            prefixes.append(prefix);
    }
    std::sort(prefixes.begin(), prefixes.end());

    m_domainCombo->blockSignals(true);
    m_domainCombo->clear();
    m_domainCombo->addItem(tr("All domains"), QString());
    for (const QString &prefix : prefixes)
        m_domainCombo->addItem(prefix, prefix);

    const int restore = m_domainCombo->findData(previous);
    m_domainCombo->setCurrentIndex(restore >= 0 ? restore : 0);
    m_domainCombo->blockSignals(false);

    m_model->setDomainFilter(m_domainCombo->currentData().toString());
}

void TraceabilityMatrixWidget::onStatusFilterChanged(int index)
{
    const QString value = m_statusCombo->itemData(index).toString();
    m_model->setStatusFilter(value.isEmpty() ? QStringLiteral("All") : value);
}

void TraceabilityMatrixWidget::onDomainFilterChanged(int index)
{
    m_model->setDomainFilter(m_domainCombo->itemData(index).toString());
}

void TraceabilityMatrixWidget::onExportMarkdown()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export to Markdown Report"), QString(),
        tr("Markdown Report (*.md)"));
    if (filePath.isEmpty())
        return;
    exportToFile(filePath, &MatrixExporter::exportMarkdown);
}

void TraceabilityMatrixWidget::onExportCsv()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export to CSV Traceability Matrix"), QString(),
        tr("CSV Traceability Matrix (*.csv)"));
    if (filePath.isEmpty())
        return;
    exportToFile(filePath, &MatrixExporter::exportCsv);
}

void TraceabilityMatrixWidget::onExportJson()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export to JSON"), QString(),
        tr("JSON Export (*.json)"));
    if (filePath.isEmpty())
        return;
    exportToFile(filePath, &MatrixExporter::exportJson);
}

void TraceabilityMatrixWidget::exportToFile(const QString &filePath,
                                            MatrixExporter::ExportFunction exporter)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Cannot open file for writing:\n%1").arg(filePath));
        return;
    }
    const bool ok = exporter(m_requirements, file);
    file.close();

    if (ok) {
        m_statusLabel->setText(
            tr("Exported %1 requirements to %2").arg(m_requirements.size()).arg(filePath));
        qCInfo(lcFramework) << "TraceabilityMatrixWidget: exported" << m_requirements.size()
                            << "requirements to" << filePath;
    } else {
        m_statusLabel->setText(tr("Export failed."));
        QMessageBox::warning(this, tr("Export"),
                             tr("Export failed for:\n%1").arg(filePath));
    }
}

} // namespace Daqster

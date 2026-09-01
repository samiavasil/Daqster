#include "ValidationDialog.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QBrush>
#include <QLabel>

namespace Daqster {

ValidationDialog::ValidationDialog(const QVector<RequirementsValidator::Issue> &issues,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Requirements Validation Report"));
    resize(720, 420);

    m_issueList = new QListWidget(this);

    const int errorCount =
        RequirementsValidator::countSeverity(issues, RequirementsValidator::Severity::Error);
    const int warningCount =
        RequirementsValidator::countSeverity(issues, RequirementsValidator::Severity::Warning);

    if (issues.isEmpty()) {
        m_issueList->addItem(tr("No issues found. All requirements are consistent."));
    } else {
        for (const RequirementsValidator::Issue &issue : issues) {
            const QString severity = issue.severity == RequirementsValidator::Severity::Error
                ? QStringLiteral("ERROR")
                : QStringLiteral("WARNING");
            const QString prefix = issue.id.isEmpty()
                ? QStringLiteral("[%1]").arg(severity)
                : QStringLiteral("[%1] %2").arg(severity, issue.id);
            const QString text = QStringLiteral("%1 — %2 (%3)")
                                     .arg(prefix, issue.message, issue.field);

            QListWidgetItem *item = new QListWidgetItem(text, m_issueList);
            const QColor color = issue.severity == RequirementsValidator::Severity::Error
                ? QColor(Qt::red)
                : QColor(0xC0, 0x7A, 0x00); // dark yellow/orange
            item->setForeground(QBrush(color));
            if (!issue.id.isEmpty()) {
                item->setData(Qt::UserRole, issue.id);
                item->setToolTip(tr("Click to navigate to %1").arg(issue.id));
            }
        }
    }

    connect(m_issueList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
                const QString id = item->data(Qt::UserRole).toString();
                if (!id.isEmpty())
                    emit navigateRequested(id);
            });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Validation result: %1 error(s), %2 warning(s)")
                                     .arg(errorCount).arg(warningCount), this));
    layout->addWidget(m_issueList);
    layout->addWidget(buttons);
    setLayout(layout);
}

} // namespace Daqster

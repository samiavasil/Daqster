#pragma once

#include <QDialog>
#include <QVector>
#include "RequirementsValidator.h"

class QListWidget;
class QPushButton;

namespace Daqster {

/**
 * @brief Report dialog showing validation issues.
 *
 * Errors are shown in red, warnings in yellow/orange. Clicking an issue
 * emits navigateRequested(id) so the caller can jump to the requirement.
 *
 * Two copy actions (REQ-SW-PL-001): „Копирай“ (Ctrl+C) puts the markdown
 * bullets of every issue on the clipboard, „Копирай като справка“ puts a
 * pasteable status report (header + bullets) so the result can be saved as a
 * status note. The formatting itself lives in ValidationReport.
 */
class ValidationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ValidationDialog(const QVector<RequirementsValidator::Issue> &issues,
                              QWidget *parent = nullptr);

    /**
     * @brief The markdown bullets the „Копирай“ action puts on the clipboard.
     */
    QString markdownText() const;

    /**
     * @brief The status report the „Копирай като справка“ action produces.
     */
    QString reportText() const;

signals:
    void navigateRequested(const QString &id);

private:
    void copyToClipboard(const QString &text);

    QListWidget *m_issueList;
    QPushButton *m_copyButton;
    QPushButton *m_copyReportButton;
    QVector<RequirementsValidator::Issue> m_issues;
};

} // namespace Daqster

#pragma once

#include <QDialog>
#include <QVector>
#include "RequirementsValidator.h"

class QListWidget;

namespace Daqster {

/**
 * @brief Report dialog showing validation issues.
 *
 * Errors are shown in red, warnings in yellow/orange. Clicking an issue
 * emits navigateRequested(id) so the caller can jump to the requirement.
 */
class ValidationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ValidationDialog(const QVector<RequirementsValidator::Issue> &issues,
                              QWidget *parent = nullptr);

signals:
    void navigateRequested(const QString &id);

private:
    QListWidget *m_issueList;
};

} // namespace Daqster

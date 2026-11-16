#include "ValidationReport.h"

#include <QFileInfo>

namespace Daqster {
namespace ValidationReport {

QString severityToken(RequirementsValidator::Severity severity)
{
    return severity == RequirementsValidator::Severity::Error
               ? QStringLiteral("error")
               : QStringLiteral("warning");
}

QString locationOf(const RequirementsValidator::Issue &issue)
{
    QString location = issue.filePath.trimmed();
    if (!location.isEmpty()) {
        // Base name only: the requirement tree root is machine-specific and an
        // absolute path in a pasted report is noise.
        const QString baseName = QFileInfo(location).fileName();
        location = baseName.isEmpty() ? location : baseName;
        if (issue.line > 0)
            location += QStringLiteral(":%1").arg(issue.line);
        return location;
    }

    if (!issue.id.trimmed().isEmpty()) {
        return issue.field.trimmed().isEmpty()
                   ? issue.id.trimmed()
                   : QStringLiteral("%1:%2").arg(issue.id.trimmed(), issue.field.trimmed());
    }

    return QStringLiteral("—");
}

QString issuesAsMarkdown(const QVector<RequirementsValidator::Issue> &issues)
{
    QString text;
    for (const RequirementsValidator::Issue &issue : issues) {
        if (!text.isEmpty())
            text += QLatin1Char('\n');
        text += QStringLiteral("- %1: %2 — %3")
                    .arg(severityToken(issue.severity), locationOf(issue), issue.message);
    }
    return text;
}

QString asStatusReport(const QVector<RequirementsValidator::Issue> &issues,
                       const QDateTime &now)
{
    const int errors =
        RequirementsValidator::countSeverity(issues, RequirementsValidator::Severity::Error);
    const int warnings =
        RequirementsValidator::countSeverity(issues, RequirementsValidator::Severity::Warning);

    QString header = QStringLiteral("## Валидация %1 — %2 грешки, %3 предупреждения")
                         .arg(now.toString(QStringLiteral("yyyy-MM-dd HH:mm")))
                         .arg(errors)
                         .arg(warnings);

    if (issues.isEmpty())
        return header + QStringLiteral("\n\n- няма намерени проблеми");

    return header + QStringLiteral("\n\n") + issuesAsMarkdown(issues);
}

} // namespace ValidationReport
} // namespace Daqster

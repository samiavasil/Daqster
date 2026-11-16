#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>
#include "RequirementsValidator.h"

namespace Daqster {

/**
 * @brief Clipboard/report formatting for validation issues.
 *
 * The formatting lives here, outside ValidationDialog, so it is pure
 * (no widgets, no clipboard) and directly unit-testable in the headless test
 * binary. The dialog only puts the result on the clipboard.
 *
 * Issue markdown line format:
 * @code
 * - <severity>: <location> — <message>
 * @endcode
 * where @c severity is @c error or @c warning and @c location is the base name
 * of the issue's @c filePath with @c :line appended when the line is known
 * (the absolute root is machine-specific and is dropped on purpose), falling
 * back to @c <id>:<field> for issues the validator raised without a file, and
 * to @c — when the issue carries neither.
 */
namespace ValidationReport {

/**
 * @brief One markdown bullet per issue, newline separated, no header.
 *
 * @param issues Issues in display order (as produced by the validator).
 * @return Markdown lines; empty string when @p issues is empty.
 */
QString issuesAsMarkdown(const QVector<RequirementsValidator::Issue> &issues);

/**
 * @brief Header line plus the markdown bullets — a pasteable status report.
 *
 * Header: `## Валидация YYYY-MM-DD HH:MM — N грешки, M предупреждения`.
 *
 * @param issues Issues in display order.
 * @param now Timestamp for the header (injectable so tests are deterministic).
 * @return The full markdown block (header, blank line, bullets).
 */
QString asStatusReport(const QVector<RequirementsValidator::Issue> &issues,
                       const QDateTime &now = QDateTime::currentDateTime());

/**
 * @brief The location token of one issue (see the format contract above).
 */
QString locationOf(const RequirementsValidator::Issue &issue);

/**
 * @brief The severity token of one issue ("error" / "warning").
 */
QString severityToken(RequirementsValidator::Severity severity);

} // namespace ValidationReport
} // namespace Daqster

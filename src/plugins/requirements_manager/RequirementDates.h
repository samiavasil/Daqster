#pragma once

#include <QDate>
#include <QString>

namespace Daqster {

/**
 * @brief Creation-date parsing and age arithmetic for requirements
 *        (REQ-SW-PL-100).
 *
 * `- **Дата:**` is the only date a requirement file carries, and it was a
 * free-form QString: nothing checked the shape, nothing showed the age. This
 * namespace is the single place that turns the field into a day count, so the
 * model (columns), the preview (details header) and the widget (waiting
 * counter) cannot disagree about what "12 days" means.
 *
 * QtCore-only and free of any model/widget state, so it is unit-testable in
 * the headless requirements_manager_tests binary.
 */
namespace RequirementDates {

/**
 * @brief How long a DRAFT may wait before it is flagged as overdue.
 *
 * Two weeks: long enough that a normal review round does not light up, short
 * enough that a forgotten proposal is visible before a month passes.
 */
inline constexpr int kStaleDraftDays = 14;

/**
 * @brief Parses a `- **Дата:**` value strictly as `YYYY-MM-DD`.
 *
 * Strict means the string must be exactly the ISO calendar date — surrounding
 * whitespace is tolerated, anything else (a locale date, a year-first variant
 * without zero padding, a free-form note) is not a date. Callers report the
 * invalid value as a warning and keep the file readable: an old requirement
 * with a sloppy date must not become an error.
 *
 * @param value The raw field value.
 * @return true when @p value parses as an ISO date.
 */
bool isValidIsoDate(const QString &value);

/**
 * @brief Whole days between @p date and @p today.
 *
 * A missing, unparsable or future date has no age: it returns -1, which is the
 * "unknown" value the model exposes through AgeDaysRole and renders as "—".
 *
 * @param date The `- **Дата:**` value (ISO, per isValidIsoDate()).
 * @param today The reference day (usually QDate::currentDate(), a parameter so
 *        the result is testable).
 * @return Days elapsed in [0, …), or -1 when the age is unknown.
 */
int ageInDays(const QString &date, const QDate &today);

/**
 * @brief Renders an age as the tree's `Възраст` cell text.
 *
 * @param days Result of ageInDays().
 * @return "12 дни" for a known age, "—" when the date is missing/invalid.
 */
QString ageLabel(int days);

/**
 * @brief Renders the preview header's creation half.
 *
 * @param date The `- **Дата:**` value.
 * @param today The reference day.
 * @return "2026-11-21 · преди 12 дни", "2026-11-21 · днес" for the same day,
 *         or "—" when the date is missing/invalid.
 */
QString createdLabel(const QString &date, const QDate &today);

/**
 * @brief Whether an age has crossed the waiting threshold.
 *
 * @param days Result of ageInDays().
 * @return true when @p days is strictly greater than kStaleDraftDays.
 */
bool isOverdue(int days);

} // namespace RequirementDates
} // namespace Daqster

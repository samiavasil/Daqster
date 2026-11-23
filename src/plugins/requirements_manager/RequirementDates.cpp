#include "RequirementDates.h"

#include <QRegularExpression>

namespace Daqster {
namespace RequirementDates {

namespace {

constexpr int kUnknownAge = -1;

// Exactly ten ASCII digits split as 4-2-2, and the calendar has to agree with
// it: 2026-02-30 looks like a date but is not one.
const QRegularExpression &isoDateRe()
{
    static const QRegularExpression re(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    return re;
}

} // namespace

bool isValidIsoDate(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (!isoDateRe().match(trimmed).hasMatch())
        return false;
    // fromString enforces the real calendar (month 1-12, day within the month),
    // which the regex cannot: it rejects 2026-02-30 and 2026-13-01.
    return QDate::fromString(trimmed, Qt::ISODate).isValid();
}

int ageInDays(const QString &date, const QDate &today)
{
    if (!isValidIsoDate(date))
        return kUnknownAge;
    if (!today.isValid())
        return kUnknownAge;

    const QDate created = QDate::fromString(date.trimmed(), Qt::ISODate);
    // A future date has no age yet; the validator's ISO check still accepted
    // it, so it is reported as unknown rather than as a negative number.
    if (created > today)
        return kUnknownAge;
    return static_cast<int>(created.daysTo(today));
}

QString ageLabel(int days)
{
    if (days < 0)
        return QStringLiteral("—");
    return QStringLiteral("%1 дни").arg(days);
}

QString createdLabel(const QString &date, const QDate &today)
{
    if (!isValidIsoDate(date))
        return QStringLiteral("—");

    const QString iso = date.trimmed();
    const int days = ageInDays(iso, today);
    if (days < 0)
        return iso;
    if (days == 0)
        return QStringLiteral("%1 · днес").arg(iso);
    return QStringLiteral("%1 · преди %2 дни").arg(iso).arg(days);
}

bool isOverdue(int days)
{
    return days > kStaleDraftDays;
}

} // namespace RequirementDates
} // namespace Daqster

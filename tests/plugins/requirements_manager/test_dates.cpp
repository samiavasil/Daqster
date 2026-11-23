#include <QtTest>

#include "RequirementDates.h"
#include "test_dates.h"

using namespace Daqster;

void TestDates::isoDateParsing_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("valid");

    QTest::newRow("canonical") << QStringLiteral("2026-11-21") << true;
    QTest::newRow("zero padded") << QStringLiteral("2026-01-05") << true;
    QTest::newRow("leap day") << QStringLiteral("2024-02-29") << true;
    QTest::newRow("surrounding spaces") << QStringLiteral(" 2026-11-21 ") << true;
    QTest::newRow("empty (missing)") << QString() << false;
    QTest::newRow("dotted") << QStringLiteral("21.11.2026") << false;
    QTest::newRow("unpadded month") << QStringLiteral("2026-2-1") << false;
    QTest::newRow("impossible day") << QStringLiteral("2026-02-30") << false;
    QTest::newRow("impossible month") << QStringLiteral("2026-13-01") << false;
    QTest::newRow("non-leap 29 feb") << QStringLiteral("2025-02-29") << false;
    QTest::newRow("prose") << QStringLiteral("ноември 2026") << false;
}

void TestDates::isoDateParsing()
{
    QFETCH(QString, value);
    QFETCH(bool, valid);

    QCOMPARE(RequirementDates::isValidIsoDate(value), valid);
}

void TestDates::ageInDays_data()
{
    QTest::addColumn<QString>("date");
    QTest::addColumn<QDate>("today");
    QTest::addColumn<int>("days");

    const QDate today(2026, 11, 21);
    QTest::newRow("same day") << QStringLiteral("2026-11-21") << today << 0;
    QTest::newRow("one day") << QStringLiteral("2026-11-20") << today << 1;
    QTest::newRow("ninety days") << QStringLiteral("2026-08-23") << today << 90;
    // Leap-year boundary: 2024-02-29 exists, 2025-02-29 does not.
    QTest::newRow("leap year -> 365")
        << QStringLiteral("2024-02-29") << QDate(2025, 2, 28) << 365;
    QTest::newRow("leap year -> 366")
        << QStringLiteral("2024-02-29") << QDate(2025, 3, 1) << 366;
    QTest::newRow("missing date") << QString() << today << -1;
    QTest::newRow("invalid date") << QStringLiteral("вчера") << today << -1;
    QTest::newRow("future date")
        << QStringLiteral("2026-12-01") << today << -1;
}

void TestDates::ageInDays()
{
    QFETCH(QString, date);
    QFETCH(QDate, today);
    QFETCH(int, days);

    QCOMPARE(RequirementDates::ageInDays(date, today), days);
}

void TestDates::ageLabels()
{
    QCOMPARE(RequirementDates::ageLabel(0), QStringLiteral("0 дни"));
    QCOMPARE(RequirementDates::ageLabel(1), QStringLiteral("1 дни"));
    QCOMPARE(RequirementDates::ageLabel(90), QStringLiteral("90 дни"));
    QCOMPARE(RequirementDates::ageLabel(-1), QStringLiteral("—"));
}

void TestDates::createdLabel_rendersDateWithAge()
{
    const QDate today(2026, 11, 21);

    QCOMPARE(RequirementDates::createdLabel(QStringLiteral("2026-11-09"), today),
             QStringLiteral("2026-11-09 · преди 12 дни"));
    QCOMPARE(RequirementDates::createdLabel(QStringLiteral("2026-11-21"), today),
             QStringLiteral("2026-11-21 · днес"));
    QCOMPARE(RequirementDates::createdLabel(QString(), today), QStringLiteral("—"));
    QCOMPARE(RequirementDates::createdLabel(QStringLiteral("14/11/2026"), today),
             QStringLiteral("—"));
}

void TestDates::overdueThreshold_isStrictlyGreaterThanTwoWeeks()
{
    QCOMPARE(RequirementDates::kStaleDraftDays, 14);
    QVERIFY(!RequirementDates::isOverdue(13));
    QVERIFY(!RequirementDates::isOverdue(14));
    QVERIFY(RequirementDates::isOverdue(15));
    QVERIFY(!RequirementDates::isOverdue(-1)); // unknown age is not "old"
}

// No QTEST_GUILESS_MAIN here: the classes share one binary whose main lives in
// test_main.cpp.

#include <QtTest>

#include "ValidationReport.h"
#include "RequirementsValidator.h"
#include "test_report.h"

using namespace Daqster;

namespace {

RequirementsValidator::Issue makeIssue(const QString &id, const QString &field,
                                       const QString &message,
                                       RequirementsValidator::Severity severity,
                                       const QString &filePath = QString(),
                                       int line = 0)
{
    RequirementsValidator::Issue issue;
    issue.id = id;
    issue.field = field;
    issue.message = message;
    issue.severity = severity;
    issue.filePath = filePath;
    issue.line = line;
    return issue;
}

} // namespace

void TestValidationReport::issueLineFormat_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("severity");
    QTest::addColumn<QString>("message");
    QTest::addColumn<QString>("expected");

    // File + line: the report addresses the issue where a reader can find it.
    QTest::newRow("file and line")
        << "REQ-SW-PL-001" << "status" << "/tmp/req/REQ-SW-PL-001-tool.md" << 12
        << int(RequirementsValidator::Severity::Error) << "невалиден статус"
        << QStringLiteral("- error: REQ-SW-PL-001-tool.md:12 — невалиден статус");
    // File without a known line (the parser records none).
    QTest::newRow("file only")
        << "REQ-SW-PL-002" << "status" << "/tmp/req/REQ-SW-PL-002-parse.md" << 0
        << int(RequirementsValidator::Severity::Warning) << "липсва поле"
        << QStringLiteral("- warning: REQ-SW-PL-002-parse.md — липсва поле");
    // No file: fall back to id:field so the line still identifies something.
    QTest::newRow("id and field")
        << "REQ-SW-PL-003" << "dependencies" << QString() << 0
        << int(RequirementsValidator::Severity::Error) << "цикъл"
        << QStringLiteral("- error: REQ-SW-PL-003:dependencies — цикъл");
    // Global issue: neither file nor id.
    QTest::newRow("global issue")
        << QString() << QString() << QString() << 0
        << int(RequirementsValidator::Severity::Warning) << "дублиран ID"
        << QStringLiteral("- warning: — — дублиран ID");
}

void TestValidationReport::issueLineFormat()
{
    QFETCH(QString, id);
    QFETCH(QString, field);
    QFETCH(QString, filePath);
    QFETCH(int, line);
    QFETCH(int, severity);
    QFETCH(QString, message);
    QFETCH(QString, expected);

    const RequirementsValidator::Issue issue =
        makeIssue(id, field, message,
                  static_cast<RequirementsValidator::Severity>(severity), filePath, line);

    QCOMPARE(ValidationReport::issuesAsMarkdown({issue}), expected);
}

void TestValidationReport::issueLinesKeepOrder()
{
    const QVector<RequirementsValidator::Issue> issues{
        makeIssue(QStringLiteral("REQ-A"), QStringLiteral("status"),
                  QStringLiteral("първа"), RequirementsValidator::Severity::Error,
                  QStringLiteral("/r/REQ-A.md")),
        makeIssue(QStringLiteral("REQ-B"), QStringLiteral("parentId"),
                  QStringLiteral("втора"), RequirementsValidator::Severity::Warning,
                  QStringLiteral("/r/REQ-B.md"))};

    const QString markdown = ValidationReport::issuesAsMarkdown(issues);
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0), QStringLiteral("- error: REQ-A.md — първа"));
    QCOMPARE(lines.at(1), QStringLiteral("- warning: REQ-B.md — втора"));
    QVERIFY(ValidationReport::issuesAsMarkdown({}).isEmpty());
}

void TestValidationReport::statusReportHeader()
{
    const QVector<RequirementsValidator::Issue> issues{
        makeIssue(QStringLiteral("REQ-A"), QStringLiteral("status"),
                  QStringLiteral("лош статус"), RequirementsValidator::Severity::Error,
                  QStringLiteral("/r/REQ-A.md")),
        makeIssue(QStringLiteral("REQ-B"), QStringLiteral("date"),
                  QStringLiteral("липсва дата"), RequirementsValidator::Severity::Warning,
                  QStringLiteral("/r/REQ-B.md"))};

    const QDateTime now(QDate(2026, 11, 16), QTime(14, 5));
    const QString report = ValidationReport::asStatusReport(issues, now);
    const QStringList lines = report.split(QLatin1Char('\n'));

    QCOMPARE(lines.at(0), QStringLiteral("## Валидация 2026-11-16 14:05 — 1 грешки, 1 предупреждения"));
    QCOMPARE(lines.at(1), QString());
    QCOMPARE(lines.at(2), QStringLiteral("- error: REQ-A.md — лош статус"));
    QCOMPARE(lines.at(3), QStringLiteral("- warning: REQ-B.md — липсва дата"));
}

void TestValidationReport::statusReportEmpty()
{
    const QDateTime now(QDate(2026, 11, 16), QTime(23, 59));
    const QString report = ValidationReport::asStatusReport({}, now);
    QVERIFY(report.contains(QStringLiteral("0 грешки, 0 предупреждения")));
    QVERIFY(report.contains(QStringLiteral("няма намерени проблеми")));
}

void TestValidationReport::validatorAttachesFile()
{
    Requirement req;
    req.id = QStringLiteral("REQ-SW-PL-900");
    req.title = QStringLiteral("Fixture");
    req.status = QStringLiteral("ACTIVE");
    req.section = QStringLiteral("active");
    req.filePath = QStringLiteral("/tmp/req/REQ-SW-PL-900-fixture.md");

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate({req});

    QVERIFY(!issues.isEmpty());
    for (const RequirementsValidator::Issue &issue : issues) {
        QCOMPARE(issue.filePath, req.filePath);
        QVERIFY(ValidationReport::issuesAsMarkdown({issue})
                    .contains(QStringLiteral("REQ-SW-PL-900-fixture.md")));
    }
}

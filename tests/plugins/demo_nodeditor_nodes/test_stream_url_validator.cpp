#include <QtTest>

#include "StreamUrlValidator.h"
#include "test_stream_url_validator.h"

// REQ-SW-PL-018/019 stream URL validation unit tests. The exact error message
// strings must match the user-facing messages produced by the Stream Source
// node contract: "Enter a stream URL first" (trimmed-empty input), "Invalid
// stream URL" (QUrl invalid or empty scheme), "Unsupported stream scheme: %1".
namespace {

// Returns the errorOut value; QString() (null) when the validator returned
// true and left errorOut untouched.
QString validateAndError(const QString &input, bool *okOut)
{
    QString error(QStringLiteral("sentinel"));
    *okOut = StreamUrlValidator::isValidStreamUrl(input, &error);
    return error;
}

} // namespace

void TestStreamUrlValidator::validHttp()
{
    bool ok = false;
    QString error = validateAndError(QStringLiteral("http://example.com/stream"), &ok);
    QVERIFY(ok);
    QCOMPARE(error, QStringLiteral("sentinel"));
}

void TestStreamUrlValidator::validHttps()
{
    bool ok = false;
    QString error = validateAndError(QStringLiteral("https://example.com/stream.m3u8"), &ok);
    QVERIFY(ok);
    QCOMPARE(error, QStringLiteral("sentinel"));
}

void TestStreamUrlValidator::validRtsp()
{
    bool ok = false;
    QString error = validateAndError(QStringLiteral("rtsp://host/path"), &ok);
    QVERIFY(ok);
    QCOMPARE(error, QStringLiteral("sentinel"));
}

void TestStreamUrlValidator::validSchemeCaseInsensitive()
{
    // QUrl normalizes the scheme to lowercase; the validator lower-cases the
    // scheme before comparing, so an uppercase scheme is accepted.
    bool ok = false;
    QString error = validateAndError(QStringLiteral("HTTP://example.com/stream"), &ok);
    QVERIFY(ok);
    QCOMPARE(error, QStringLiteral("sentinel"));
}

void TestStreamUrlValidator::invalidEmpty()
{
    bool ok = true;
    QString error = validateAndError(QString(), &ok);
    QVERIFY(!ok);
    QCOMPARE(error, QStringLiteral("Enter a stream URL first"));
}

void TestStreamUrlValidator::invalidWhitespaceOnly()
{
    bool ok = true;
    QString error = validateAndError(QStringLiteral("   \t  "), &ok);
    QVERIFY(!ok);
    QCOMPARE(error, QStringLiteral("Enter a stream URL first"));
}

void TestStreamUrlValidator::invalidNoScheme()
{
    // "not a url" parses as a relative reference: QUrl is "valid" but the
    // scheme is empty, which must be reported as an invalid stream URL.
    bool ok = true;
    QString error = validateAndError(QStringLiteral("not a url"), &ok);
    QVERIFY(!ok);
    QCOMPARE(error, QStringLiteral("Invalid stream URL"));
}

void TestStreamUrlValidator::invalidUnsupportedScheme()
{
    bool ftpOk = true;
    QString ftpError = validateAndError(QStringLiteral("ftp://host/file"), &ftpOk);
    QVERIFY(!ftpOk);
    QCOMPARE(ftpError, QStringLiteral("Unsupported stream scheme: ftp"));

    bool fileOk = true;
    QString fileError = validateAndError(QStringLiteral("file:///tmp/x"), &fileOk);
    QVERIFY(!fileOk);
    QCOMPARE(fileError, QStringLiteral("Unsupported stream scheme: file"));
}

void TestStreamUrlValidator::errorOut_untouchedOnSuccess()
{
    // On success the error pointer is left untouched (kept at its sentinel).
    QString error(QStringLiteral("untouched"));
    QVERIFY(StreamUrlValidator::isValidStreamUrl(QStringLiteral("http://example.com"), &error));
    QCOMPARE(error, QStringLiteral("untouched"));
}

// No QTEST_GUILESS_MAIN here: the two video test classes share one binary
// whose main lives in test_main.cpp.

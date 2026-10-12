#pragma once

#include <QtTest>

// Test class for StreamUrlValidator. Declared in a header (instead of relying
// on QTEST_GUILESS_MAIN inside the .cpp) so the two video test classes can
// share a single test binary - see test_main.cpp.
class TestStreamUrlValidator : public QObject
{
    Q_OBJECT

private slots:
    void validHttp();
    void validHttps();
    void validRtsp();
    void validSchemeCaseInsensitive();
    void invalidEmpty();
    void invalidWhitespaceOnly();
    void invalidNoScheme();
    void invalidUnsupportedScheme();
    void errorOut_untouchedOnSuccess();
};

#pragma once

#include <QtTest>

// Copyable validation report tests (REQ-SW-PL-001 / REQ-SW-PL-008): the exact
// markdown lines the dialog puts on the clipboard, the status-report header,
// and the file the validator attaches to an issue. Headless — ValidationReport
// is pure formatting, the dialog only delegates to it.
class TestValidationReport : public QObject
{
    Q_OBJECT

private slots:
    void issueLineFormat_data();
    void issueLineFormat();
    void issueLinesKeepOrder();
    void statusReportHeader();
    void statusReportEmpty();
    void validatorAttachesFile();
};

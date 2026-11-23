#pragma once

#include <QtTest>

// Date and age arithmetic for the "- **Дата:**" field (REQ-SW-PL-100).
// RequirementDates is QtCore-only, so this class runs in the headless
// requirements_manager_tests binary (see test_main.cpp).
class TestDates : public QObject
{
    Q_OBJECT

private slots:
    void isoDateParsing_data();
    void isoDateParsing();
    void ageInDays_data();
    void ageInDays();
    void ageLabels();
    void createdLabel_rendersDateWithAge();
    void overdueThreshold_isStrictlyGreaterThanTwoWeeks();
};

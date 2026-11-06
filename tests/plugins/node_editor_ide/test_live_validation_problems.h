#pragma once

#include <QtTest>

// REQ-SW-PL-082: "red but no text" on the canvas. A node in
// NodeValidationState::Error is painted red, but its message is only a tooltip
// (and the affordance icon is drawn outside the node in the nodeeditor
// submodule), so the IDE must list the live validation errors in the problems
// panel — next to the open-time verdict, without duplicating a row and without
// leaving a stale one behind when the node becomes valid again.
class TestLiveValidationProblems : public QObject
{
    Q_OBJECT

private slots:
    void liveValidationErrorsAppearInPanel();
    void panelDeduplicatesSameProblem();
    void panelClearsWhenNodeBecomesValid();
};

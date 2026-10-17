#pragma once

#include <QObject>

/**
 * Regression tests for LLamaModelWidget (REQ-SW-PL-074, AC 1).
 *
 * The defect: the "Стартирай сървър" button's enabled state depended only on
 * setExePath()/setModelPath(). NodeWidgetFactory hydrates the widget with the
 * model's paths once, and a user typing/pasting a path (or the "..." dialog)
 * bypassed those setters — so the button was locked in whatever state the
 * first hydration produced (disabled, for a fresh node with empty paths).
 */
class TestLlamaModelWidget : public QObject
{
    Q_OBJECT

private slots:
    void startButton_disabledOnFreshWidget();
    void startButton_disabledWithOnlyExePath();
    void startButton_disabledWithOnlyModelPath();
    void startButton_enabledWhenBothPathsTyped();
    void startButton_disabledAgainWhenPathCleared();
    void startButton_reflectsSetters();
    // AC 5 — the two halves of the response routing.
    void appendExchange_addsTreeEntryWithoutChatBubble();
    void addChatResponse_addsChatBubble();
};

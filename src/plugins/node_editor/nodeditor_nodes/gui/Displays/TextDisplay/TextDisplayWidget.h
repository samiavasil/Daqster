#pragma once

#include <QtWidgets/QWidget>

#include "Displays/TextDisplay/TextDisplayNode.h"

class QLabel;

/**
 * @brief Body of the Text Display node (REQ-SW-PL-090).
 *
 * Renders what the core model computed: a truncated preview, the stream
 * counters, and the whole text in the tooltip. No data parsing happens here —
 * the model's worker pass already produced the strings, so this widget only
 * assigns text (REQ-SW-PL-051 core/gui split).
 */
class TextDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TextDisplayWidget(QWidget *parent = nullptr);
    ~TextDisplayWidget() override = default;

    QLabel *previewLabel() const { return m_preview; }
    QLabel *statsLabel() const { return m_stats; }

public slots:
    /// Rendered by TextDisplayNode::snapshotReady().
    void setSnapshot(TextDisplaySnapshot const &snapshot);

private:
    QLabel *m_preview = nullptr;
    QLabel *m_stats = nullptr;
};

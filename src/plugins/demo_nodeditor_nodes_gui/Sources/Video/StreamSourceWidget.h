#pragma once

#include <QtWidgets/QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief GUI widget of the Stream Source node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the URL field, the Connect/Stop button and the status line. The player
 * (QMediaPlayer + frame probe) stays in the core StreamSourceNode.
 */
class StreamSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StreamSourceWidget(QWidget* parent = nullptr);
    ~StreamSourceWidget() override = default;

public slots:
    /// Model → widget: the URL was set programmatically (load()).
    void setUrl(const QString& url);
    /// Model → widget: status line text + color.
    void setStatus(const QString& text, bool ok);
    /// Model → widget: switch the button between Connect and Stop.
    void setPlaying(bool playing);

signals:
    void connectClicked();
    void urlChanged(const QString& url);

private:
    QLineEdit* m_urlEdit = nullptr;
    QPushButton* m_connectButton = nullptr;
    QLabel* m_statusLabel = nullptr;
};

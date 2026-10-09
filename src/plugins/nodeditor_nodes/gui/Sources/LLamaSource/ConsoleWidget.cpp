#include "ConsoleWidget.h"
#include "ChatBaseWidget.h"

#include <QtWidgets/QVBoxLayout>

ConsoleWidget::ConsoleWidget(QWidget *parent)
    : QWidget(parent)
    , m_chatWidget(new ChatBaseWidget())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    layout->addWidget(m_chatWidget, 1);

    // By default, hide the config panel (the user can toggle it with the
    // Config button inside ChatBaseWidget).
    m_chatWidget->setConfigVisible(false);

    connect(m_chatWidget, &ChatBaseWidget::sendRequested,
            this, &ConsoleWidget::sendRequested);
    connect(m_chatWidget, &ChatBaseWidget::configChanged,
            this, &ConsoleWidget::configChanged);
}

void ConsoleWidget::addResponse(QString const& content, QJsonObject const& rawJson)
{
    if (!rawJson.isEmpty())
        m_chatWidget->appendJsonToTree(rawJson, false);
    m_chatWidget->addResponse(content);
}

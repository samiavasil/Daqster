#include "LLamaModelWidget.h"
#include "ChatBaseWidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QSplitter>

LLamaModelWidget::LLamaModelWidget(QWidget *parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createServerTab(), tr("Сървър"));
    m_tabWidget->addTab(createDebugTab(), tr("Дебаг"));
    m_tabWidget->addTab(createChatTab(), tr("Чат"));

    mainLayout->addWidget(m_tabWidget);
}

QWidget* LLamaModelWidget::createServerTab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setSpacing(8);

    auto* connGroup = new QGroupBox(tr("Съществуващ сървър"));
    auto* connLayout = new QFormLayout(connGroup);

    auto* hostPortLayout = new QHBoxLayout();
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);

    hostPortLayout->addWidget(m_hostEdit);
    hostPortLayout->addWidget(m_portSpin);

    connLayout->addRow(tr("Хост:"), hostPortLayout);

    auto* connBtnLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton(tr("Свържи"));
    m_statusLabel = new QLabel(tr("Не е свързан"));
    m_statusLabel->setStyleSheet("color: gray;");
    connBtnLayout->addWidget(m_connectBtn);
    connBtnLayout->addWidget(m_statusLabel);
    connBtnLayout->addStretch();
    connLayout->addRow("", connBtnLayout);

    layout->addWidget(connGroup);

    auto* localGroup = new QGroupBox(tr("Локален сървър"));
    auto* localLayout = new QFormLayout(localGroup);

    auto* exeLayout = new QHBoxLayout();
    m_exePath = new QLineEdit();
    m_exePath->setPlaceholderText(tr("Път до llama.cpp (./server)"));
    m_browseExeBtn = new QPushButton("...");
    m_browseExeBtn->setMaximumWidth(30);
    exeLayout->addWidget(m_exePath);
    exeLayout->addWidget(m_browseExeBtn);
    localLayout->addRow(tr("Изпълним:"), exeLayout);

    auto* modelLayout = new QHBoxLayout();
    m_modelPath = new QLineEdit();
    m_modelPath->setPlaceholderText(tr("Път до модел (.gguf)"));
    m_browseModelBtn = new QPushButton("...");
    m_browseModelBtn->setMaximumWidth(30);
    modelLayout->addWidget(m_modelPath);
    modelLayout->addWidget(m_browseModelBtn);
    localLayout->addRow(tr("Модел:"), modelLayout);

    m_ctxSizeSpin = new QSpinBox();
    m_ctxSizeSpin->setRange(128, 65536);
    m_ctxSizeSpin->setValue(2048);
    m_ctxSizeSpin->setSingleStep(512);
    localLayout->addRow(tr("Контекст:"), m_ctxSizeSpin);

    m_useGpuCheck = new QCheckBox(tr("Използвай GPU (ако наличен)"));
    m_useGpuCheck->setChecked(true);
    localLayout->addRow("", m_useGpuCheck);

    m_startBtn = new QPushButton(tr("Стартирай сървър"));
    m_startBtn->setStyleSheet("QPushButton { color: white; background-color: #4CAF50; }");
    localLayout->addRow("", m_startBtn);

    layout->addWidget(localGroup);
    layout->addStretch();

    connect(m_connectBtn, &QPushButton::clicked, this, &LLamaModelWidget::connectClicked);
    connect(m_startBtn, &QPushButton::clicked, this, &LLamaModelWidget::startServerClicked);

    connect(m_browseExeBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Изберете llama.cpp сървър"));
        if (!path.isEmpty()) {
            m_exePath->setText(path);
            Q_EMIT exePathSelected(path);
        }
    });
    connect(m_browseModelBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Изберете модел (.gguf)"),
                                                    QString(), tr("GGUF файлове (*.gguf)"));
        if (!path.isEmpty()) {
            m_modelPath->setText(path);
            Q_EMIT modelPathSelected(path);
        }
    });

    // Free-text edits in the path fields must reach the model too, not only
    // the "..." browse buttons.
    connect(m_exePath, &QLineEdit::textChanged, this, &LLamaModelWidget::exePathSelected);
    connect(m_modelPath, &QLineEdit::textChanged, this, &LLamaModelWidget::modelPathSelected);

    return w;
}

void LLamaModelWidget::updateStartButtonEnabled() {
    m_startBtn->setEnabled(!m_exePath->text().trimmed().isEmpty()
                           && !m_modelPath->text().trimmed().isEmpty());
}

QWidget* LLamaModelWidget::createDebugTab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);

    auto* pathLayout = new QHBoxLayout();
    m_debugPath = new QLineEdit(QStringLiteral("/completion"));
    m_debugSendBtn = new QPushButton(tr("Изпрати"));
    pathLayout->addWidget(new QLabel(tr("Endpoint:"), this));
    pathLayout->addWidget(m_debugPath);
    pathLayout->addWidget(m_debugSendBtn);

    m_debugBody = new QTextEdit();
    m_debugBody->setPlaceholderText(tr("JSON body (напр. {\"prompt\":\"Hello\",\"n_predict\":128})"));
    m_debugBody->setMaximumHeight(120);

    m_debugResponse = new QTextEdit();
    m_debugResponse->setReadOnly(true);
    m_debugResponse->setPlaceholderText(tr("Отговор..."));

    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(m_debugBody);
    splitter->addWidget(m_debugResponse);

    layout->addLayout(pathLayout);
    layout->addWidget(splitter);

    connect(m_debugSendBtn, &QPushButton::clicked, this, &LLamaModelWidget::debugSendClicked);
    connect(m_debugPath, &QLineEdit::textChanged, this, &LLamaModelWidget::debugPathChanged);
    // QTextEdit::textChanged() carries no argument — wrap it.
    connect(m_debugBody, &QTextEdit::textChanged, this, [this]() {
        Q_EMIT debugBodyChanged(m_debugBody->toPlainText());
    });

    return w;
}

QWidget* LLamaModelWidget::createChatTab() {
    m_chatWidget = new ChatBaseWidget(this);

    connect(m_chatWidget, &ChatBaseWidget::sendRequested,
            this, &LLamaModelWidget::chatSendRequested);
    connect(m_chatWidget, &ChatBaseWidget::configChanged,
            this, &LLamaModelWidget::chatConfigChanged);

    return m_chatWidget;
}

void LLamaModelWidget::setStatus(QString const& status, bool connected) {
    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(connected ? "color: green;" : "color: gray;");
}

void LLamaModelWidget::setConnectButton(QString const& text, bool enabled) {
    m_connectBtn->setText(text);
    m_connectBtn->setEnabled(enabled);
}

void LLamaModelWidget::setStartButton(QString const& text, QString const& styleSheet) {
    m_startBtn->setText(text);
    m_startBtn->setStyleSheet(styleSheet);
}

void LLamaModelWidget::setDebugResponse(QString const& result) {
    m_debugResponse->setPlainText(result);
}

void LLamaModelWidget::addChatResponse(QString const& content, QJsonObject const& rawJson) {
    if (m_chatWidget == nullptr)
        return;
    if (!rawJson.isEmpty())
        m_chatWidget->appendJsonToTree(rawJson, false);
    m_chatWidget->addResponse(content);
}

void LLamaModelWidget::setHost(QString const& host) {
    const QSignalBlocker b(m_hostEdit);
    m_hostEdit->setText(host);
}

void LLamaModelWidget::setPort(int port) {
    const QSignalBlocker b(m_portSpin);
    m_portSpin->setValue(port);
}

void LLamaModelWidget::setExePath(QString const& path) {
    const QSignalBlocker b(m_exePath);
    m_exePath->setText(path);
    updateStartButtonEnabled();
}

void LLamaModelWidget::setModelPath(QString const& path) {
    const QSignalBlocker b(m_modelPath);
    m_modelPath->setText(path);
    updateStartButtonEnabled();
}

void LLamaModelWidget::setCtxSize(int ctxSize) {
    const QSignalBlocker b(m_ctxSizeSpin);
    m_ctxSizeSpin->setValue(ctxSize);
}

void LLamaModelWidget::setUseGpu(bool useGpu) {
    const QSignalBlocker b(m_useGpuCheck);
    m_useGpuCheck->setChecked(useGpu);
}

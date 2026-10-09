#include "DaqDisplayWidget.h"

#include "QtChartsCompat.h"

#include <QtGui/QPainter>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QVBoxLayout>

using Card = DaqDisplayNode::PlotCard;

DaqDisplayWidget::DaqDisplayWidget(DaqDisplayNode* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(2, 2, 2, 2);
    rootLayout->setSpacing(2);

    // Header: domain/rate label + buffer spinbox + Add Plot button (REQ-SW-PL-023 §1).
    auto* header = new QHBoxLayout();
    m_domainLabel = new QLabel(tr("sampled"), this);
    m_ringSpinBox = new QDoubleSpinBox(this);
    m_ringSpinBox->setRange(1.0, 120.0);
    m_ringSpinBox->setSingleStep(1.0);
    m_ringSpinBox->setSuffix(QStringLiteral(" s"));
    m_ringSpinBox->setValue(m_model != nullptr ? m_model->ringSeconds() : 10.0);
    m_ringSpinBox->setToolTip(tr("Ring-buffer duration in seconds"));
    auto* addPlotButton = new QPushButton(tr("Add Plot"), this);
    header->addWidget(m_domainLabel, 1);
    header->addWidget(new QLabel(tr("Buffer:"), this));
    header->addWidget(m_ringSpinBox);
    header->addWidget(addPlotButton);
    rootLayout->addLayout(header);

    // Scroll area hosting the configurable plot cards. No QStackedWidget — all
    // cards are visible, so the FFT view is reachable (AC 3, bug fix §2).
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_cardsContainer = new QWidget(m_scroll);
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(2, 2, 2, 2);
    m_cardsLayout->setSpacing(6);
    m_scroll->setWidget(m_cardsContainer);
    rootLayout->addWidget(m_scroll, 1);

    // Empty-state hint when no cards exist.
    m_emptyLabel = new QLabel(tr("No plots — press Add Plot"), m_cardsContainer);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_cardsLayout->addWidget(m_emptyLabel);

    connect(addPlotButton, &QPushButton::clicked, this, &DaqDisplayWidget::addPlotRequested);
    connect(m_ringSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &DaqDisplayWidget::ringSecondsChanged);

    // Initial build — restore()/load() may already have populated the model.
    rebuildCards();
}

void DaqDisplayWidget::updateEmptyState()
{
    m_emptyLabel->setVisible(m_cards.isEmpty());
}

void DaqDisplayWidget::buildCard(const DaqDisplayNode::PlotCard& config, int index)
{
    CardWidgets card;

    card.root = new QWidget(m_cardsContainer);
    auto* cardLayout = new QVBoxLayout(card.root);
    cardLayout->setContentsMargins(2, 2, 2, 2);
    cardLayout->setSpacing(2);

    // Card header: title label + processing combo + channel combo + delete.
    card.titleLabel = new QLabel(config.title, card.root);
    card.procCombo = new QComboBox(card.root);
    card.procCombo->addItem(tr("Time Domain"), int(Card::ProcessingType::TimeDomain));
    card.procCombo->addItem(tr("FFT"), int(Card::ProcessingType::FrequencySpectrum));
    card.procCombo->setCurrentIndex(
        config.processingType == Card::ProcessingType::FrequencySpectrum ? 1 : 0);

    card.chanCombo = new QComboBox(card.root);
    card.chanCombo->setMinimumWidth(72);
    for (int i = 0; i < m_channelNames.size(); ++i)
        card.chanCombo->addItem(m_channelNames.at(i), i);
    if (!m_channelNames.isEmpty())
        card.chanCombo->setCurrentIndex(qBound(0, config.channelIndex,
                                               m_channelNames.size() - 1));

    auto* deleteBtn = new QPushButton(tr("✕"), card.root);
    deleteBtn->setToolTip(tr("Remove plot"));
    deleteBtn->setMaximumWidth(28);

    auto* header = new QHBoxLayout();
    header->addWidget(card.titleLabel, 1);
    header->addWidget(card.procCombo);
    header->addWidget(new QLabel(tr("Ch:"), card.root));
    header->addWidget(card.chanCombo);
    header->addWidget(deleteBtn);
    cardLayout->addLayout(header);

    // Real Qt Charts graph (same construction style as the legacy slots).
    auto* chart = new QtChartsCompat::Chart();
    chart->setTitle(config.title);
    chart->legend()->hide();
    chart->setAnimationOptions(QtChartsCompat::Chart::NoAnimation);

    auto* axisX = new QtChartsCompat::ValueAxis();
    auto* axisY = new QtChartsCompat::ValueAxis();
    axisX->setLabelFormat(QStringLiteral("%.1f"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));
    axisX->setTitleText(config.axisTitleX);
    axisY->setTitleText(config.axisTitleY);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    auto* series = new QtChartsCompat::LineSeries();
    series->setName(config.title);
    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    auto* view = new QtChartsCompat::ChartView(chart);
    view->setMinimumSize(360, 180);
    view->setRenderHint(QPainter::Antialiasing);
    cardLayout->addWidget(view);

    card.series = series;
    card.axisX = axisX;
    card.axisY = axisY;
    m_cardsLayout->addWidget(card.root);

    // `index` and the combo pointers are captured by value — the card widgets
    // outlive these lambdas, and rebuildCards() recreates them from scratch.
    QComboBox* procCombo = card.procCombo;
    QComboBox* chanCombo = card.chanCombo;
    connect(procCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, index, procCombo](int) {
                Q_EMIT cardProcessingChanged(index, procCombo->currentData().toInt());
            });
    connect(chanCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, index, chanCombo](int) {
                Q_EMIT cardChannelChanged(index, chanCombo->currentData().toInt());
            });
    connect(deleteBtn, &QPushButton::clicked, this, [this, index](bool) {
        Q_EMIT removePlotRequested(index);
    });

    m_cards.append(card);
}

void DaqDisplayWidget::destroyCard(CardWidgets& card)
{
    if (card.root != nullptr)
        card.root->deleteLater(); // view → scene → chart → series/axes cleanup
    card = CardWidgets{};
}

void DaqDisplayWidget::rebuildCards()
{
    if (m_model == nullptr)
        return;

    // Drop the old cards first — the model drives the whole list.
    // destroyCard() only clears the entry in place; the entry itself must also
    // be removed, or isEmpty() (which tests the size) never turns true and this
    // loop spins forever — freezing the GUI thread on the first data update.
    while (!m_cards.isEmpty()) {
        destroyCard(m_cards.last());
        m_cards.removeLast();
    }

    const int count = m_model->cardCount();
    m_cards.reserve(count);
    for (int i = 0; i < count; ++i)
        buildCard(m_model->cardAt(i), i);

    updateEmptyState();
}

void DaqDisplayWidget::applyResult(const PlotResult& result)
{
    // GUI thread: repaint ONLY — series->replace + axis->setRange (§4).
    const int n = qMin(result.series.size(), m_cards.size());
    for (int i = 0; i < n; ++i) {
        m_cards[i].series->replace(result.series.at(i));
        if (i < result.ranges.size()) {
            m_cards[i].axisX->setRange(result.ranges.at(i).first.x(),
                                       result.ranges.at(i).first.y());
            m_cards[i].axisY->setRange(result.ranges.at(i).second.x(),
                                       result.ranges.at(i).second.y());
        }
    }
}

void DaqDisplayWidget::setDescriptorInfo(const QString& headerText,
                                         const QStringList& channelNames)
{
    m_domainLabel->setText(headerText);
    m_channelNames = channelNames;

    // Repopulate the channel combos and re-apply the descriptor-derived axis
    // titles. QSignalBlocker keeps the refresh from looking like a user edit.
    for (int i = 0; i < m_cards.size(); ++i) {
        CardWidgets& cardWidgets = m_cards[i];

        const QSignalBlocker blockChan(cardWidgets.chanCombo);
        cardWidgets.chanCombo->clear();
        for (int ch = 0; ch < m_channelNames.size(); ++ch)
            cardWidgets.chanCombo->addItem(m_channelNames.at(ch), ch);
        if (!m_channelNames.isEmpty() && m_model != nullptr) {
            const int clamped = qBound(0, m_model->cardAt(i).channelIndex,
                                       m_channelNames.size() - 1);
            cardWidgets.chanCombo->setCurrentIndex(clamped);
        }

        if (m_model == nullptr)
            continue;
        const DaqDisplayNode::PlotCard& config = m_model->cardAt(i);
        cardWidgets.axisX->setTitleText(config.axisTitleX);
        cardWidgets.axisY->setTitleText(config.axisTitleY);
    }
}

void DaqDisplayWidget::setRingSeconds(double seconds)
{
    const QSignalBlocker blocker(m_ringSpinBox);
    m_ringSpinBox->setValue(seconds);
}

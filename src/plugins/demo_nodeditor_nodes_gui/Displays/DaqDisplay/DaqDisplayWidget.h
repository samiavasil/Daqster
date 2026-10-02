#pragma once

#include <QtWidgets/QWidget>

#include <QStringList>
#include <QVector>

#include <Displays/DaqDisplay/DaqDisplayNode.h>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

// QtChartsCompat maps the Qt5/Qt6 namespace differences (Qt5: QtCharts::*,
// Qt6: global *). It is alias-based, so the header must be included rather than
// forward-declared.
#include <QtChartsCompat.h>

/**
 * @brief GUI widget of the DAQ Display node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the whole card UI: header (descriptor label, ring-buffer spin box,
 * "Add Plot" button), the scroll area and one card per DaqDisplayNode::PlotCard
 * configuration. Every card carries its own Qt Charts view, series and axes.
 *
 * The model stays widget-free (see DaqDisplayNode.h): it reports changes through
 * signals and receives user edits through public slots. The model pointer is
 * NON-OWNING — the node outlives the widget, and the factory wires the two
 * directions.
 */
class DaqDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    /// @param model non-owning pointer to the core model (reads its card config).
    explicit DaqDisplayWidget(DaqDisplayNode* model, QWidget* parent = nullptr);
    ~DaqDisplayWidget() override = default;

public slots:
    /// Model → widget: the card list changed (add / remove / restore). Rebuilds
    /// the cards from the model's current configuration.
    void rebuildCards();

    /// Model → widget: a compute pass finished — repaint series + axis ranges.
    void applyResult(const PlotResult& result);

    /// Model → widget: the input descriptor changed.
    void setDescriptorInfo(const QString& headerText, const QStringList& channelNames);

    /// Model → widget: the descriptor recommended a ring-buffer duration.
    void setRingSeconds(double seconds);

signals:
    void addPlotRequested();
    void removePlotRequested(int index);
    void cardProcessingChanged(int index, int processingType);
    void cardChannelChanged(int index, int channelIndex);
    void ringSecondsChanged(double seconds);

private:
    struct CardWidgets
    {
        QWidget* root = nullptr;
        QLabel* titleLabel = nullptr;
        QComboBox* procCombo = nullptr;
        QComboBox* chanCombo = nullptr;
        QtChartsCompat::LineSeries* series = nullptr;
        QtChartsCompat::ValueAxis* axisX = nullptr;
        QtChartsCompat::ValueAxis* axisY = nullptr;
    };

    void buildCard(const DaqDisplayNode::PlotCard& config, int index);
    void destroyCard(CardWidgets& card);
    void updateEmptyState();

private:
    DaqDisplayNode* m_model = nullptr;   // non-owning

    QLabel* m_domainLabel = nullptr;
    QDoubleSpinBox* m_ringSpinBox = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_cardsContainer = nullptr;
    QVBoxLayout* m_cardsLayout = nullptr;
    QLabel* m_emptyLabel = nullptr;

    QVector<CardWidgets> m_cards;
    QStringList m_channelNames;          // last descriptor's channel names
};

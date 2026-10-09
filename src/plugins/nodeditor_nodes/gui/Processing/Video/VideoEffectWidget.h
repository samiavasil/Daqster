#pragma once
#include <VideoEffectOps.h>

#include <QtWidgets/QWidget>

#include <QVector>

class QComboBox;
class QLabel;
class QSlider;
class QStackedWidget;
class VideoFrameData;

/**
 * @brief GUI widget of the Video Effect node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the effect combo, the stacked parameter pages and the CPU metric label.
 * The effect processing logic stays in the core VideoEffectNode.
 */
class VideoEffectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoEffectWidget(QWidget* parent = nullptr);
    ~VideoEffectWidget() override = default;

public slots:
    /// Model → widget: the effect or a parameter changed (or load() restored them).
    void setConfig(int effectIndex, const EffectParams& params);
    /// Model → widget: GPU path was used for the last frame.
    void onGpuPathUsed();
    /// Model → widget: CPU path was used for the last frame.
    void onCpuPathUsed();

signals:
    void effectIndexChanged(int index);
    void brightnessChanged(int value);
    void contrastChanged(int value);
    void flipChanged(int index);
    void blurChanged(int value);
#ifdef HAVE_OPENCV
    void gaussianChanged(int value);
    void cannyLowChanged(int value);
    void cannyHighChanged(int value);
    void thresholdChanged(int value);
#endif

private:
    struct PageWidgets
    {
        QWidget* page = nullptr;
        QSlider* slider = nullptr;
        QLabel* valueLabel = nullptr;
        QComboBox* combo = nullptr;
    };

    struct ParamSpec
    {
        int min = 0;
        int max = 100;
        int step = 1;
        const char* title = "";
        const char* paramName = ""; // "brightness", "contrast", etc.
    };

    void createPages();
    void updateMetricLabel();

    QComboBox* m_effectCombo = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_metricLabel = nullptr;
    QVector<PageWidgets> m_pages;
    QVector<ParamSpec> m_paramSpecs;

    quint64 m_submitted = 0;
    quint64 m_completed = 0;
    quint64 m_skipped = 0;
    double m_fps = 0.0;
};
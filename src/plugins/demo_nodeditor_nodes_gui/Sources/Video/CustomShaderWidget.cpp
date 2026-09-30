#include "CustomShaderWidget.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtGui/QFont>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QVBoxLayout>

CustomShaderWidget::CustomShaderWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // GLSL editor (monospace, fixed height).
    m_glslEditor = new QPlainTextEdit(this);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_glslEditor->setFont(monoFont);
    m_glslEditor->setFixedHeight(140);
    m_glslEditor->setPlaceholderText(QStringLiteral(
        "// Shadertoy-style: write void mainImage(out vec4 fragColor, in vec2 fragCoord)\n"
        "// Available uniforms: u_tex, u_resolution, u_time, u_param0..3"));
    layout->addWidget(m_glslEditor);

    // Compile button.
    m_compileButton = new QPushButton(tr("Compile & Apply"), this);
    layout->addWidget(m_compileButton);
    connect(m_compileButton, &QPushButton::clicked,
            this, &CustomShaderWidget::compileRequested);

    // Error log (read-only, fixed height).
    m_errorLog = new QPlainTextEdit(this);
    m_errorLog->setFixedHeight(70);
    m_errorLog->setReadOnly(true);
    QFont errorFont(QStringLiteral("Monospace"));
    errorFont.setStyleHint(QFont::TypeWriter);
    errorFont.setPointSize(8);
    m_errorLog->setFont(errorFont);
    m_errorLog->setPlaceholderText(tr("Shader compilation log..."));
    layout->addWidget(m_errorLog);

    // 4x slider rows: QSlider (0-100) + QLabel ("u_param0: 0.00").
    const QStringList paramNames = {
        QStringLiteral("u_param0"), QStringLiteral("u_param1"),
        QStringLiteral("u_param2"), QStringLiteral("u_param3")};
    for (int i = 0; i < 4; ++i) {
        auto* row = new QHBoxLayout();
        m_sliders[i] = new QSlider(Qt::Horizontal, this);
        m_sliders[i]->setRange(0, 100);
        m_sliders[i]->setValue(0);
        m_sliderLabels[i] = new QLabel(
            QStringLiteral("%1: 0.00").arg(paramNames[i]), this);
        m_sliderLabels[i]->setMinimumWidth(90);
        row->addWidget(m_sliders[i], 1);
        row->addWidget(m_sliderLabels[i]);
        layout->addLayout(row);

        connect(m_sliders[i], &QSlider::valueChanged, this,
            [this, i](int value) {
                Q_EMIT paramChanged(i, value);
            });
    }

    // Animate checkbox.
    m_animateCheck = new QCheckBox(tr("Animate (u_time)"), this);
    layout->addWidget(m_animateCheck);
    connect(m_animateCheck, &QCheckBox::toggled,
            this, &CustomShaderWidget::animateToggled);

    // Connect editor text changes
    connect(m_glslEditor, &QPlainTextEdit::textChanged, this, [this]() {
        Q_EMIT sourceChanged(m_glslEditor->toPlainText());
    });
}

void CustomShaderWidget::setConfig(const QString& source, const float param[4], bool animate)
{
    const QSignalBlocker blockEditor(m_glslEditor);
    m_glslEditor->setPlainText(source);

    for (int i = 0; i < 4; ++i) {
        const int value = int(param[i] * 100.0f + 0.5f);
        const QSignalBlocker blockSlider(m_sliders[i]);
        m_sliders[i]->setValue(value);
        m_sliderLabels[i]->setText(
            QStringLiteral("%1: %2").arg(QStringLiteral("u_param%1").arg(i)).arg(value / 100.0f, 0, 'f', 2));
    }

    const QSignalBlocker blockAnimate(m_animateCheck);
    m_animateCheck->setChecked(animate);
}

void CustomShaderWidget::onHardwareGlMissing()
{
    m_errorLog->setPlainText(QStringLiteral("Custom shader requires hardware OpenGL"));
}

void CustomShaderWidget::onCompilationError(const QString& error)
{
    m_errorLog->setPlainText(error);
}

void CustomShaderWidget::onCompilationOk()
{
    m_errorLog->clear();
}
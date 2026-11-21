#include "DraftNoteDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Daqster {

DraftNoteDialog::DraftNoteDialog(const QString &title, const QString &label,
                                 const QString &confirmText, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setObjectName(QStringLiteral("draftNoteDialog"));

    auto *labelWidget = new QLabel(label, this);
    labelWidget->setWordWrap(true);

    m_noteEdit = new QLineEdit(this);
    m_noteEdit->setObjectName(QStringLiteral("draftNoteEdit"));
    m_noteEdit->setPlaceholderText(tr("Задължително: какво липсва или защо не става"));

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_confirmButton = buttons->button(QDialogButtonBox::Ok);
    m_confirmButton->setObjectName(QStringLiteral("draftNoteConfirm"));
    m_confirmButton->setText(confirmText);
    // Empty note = no verdict: the button is the rule, made visible.
    m_confirmButton->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(labelWidget);
    layout->addWidget(m_noteEdit);
    layout->addWidget(buttons);
    setLayout(layout);

    connect(m_noteEdit, &QLineEdit::textChanged, this, [this]() {
        m_confirmButton->setEnabled(!note().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(460, 150);
}

QString DraftNoteDialog::note() const
{
    return m_noteEdit ? m_noteEdit->text().trimmed() : QString();
}

QLineEdit *DraftNoteDialog::noteEdit() const
{
    return m_noteEdit;
}

QPushButton *DraftNoteDialog::confirmButton() const
{
    return m_confirmButton;
}

void DraftNoteDialog::setNote(const QString &note)
{
    if (m_noteEdit)
        m_noteEdit->setText(note);
}

} // namespace Daqster

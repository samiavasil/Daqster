#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

namespace Daqster {

/**
 * @brief The one field a draft verdict needs (REQ-SW-PL-102).
 *
 * „За корекция“ and „Откажи“ both ask for a reason, and both refuse an empty
 * one. The refusal lives HERE, in the dialog, not in the caller: a confirm
 * button that stays disabled while the field is empty is the only way the rule
 * is visible to the person typing instead of a warning after the fact.
 *
 * QtWidgets-only, no file I/O — testable in the offscreen GUI test binary.
 */
class DraftNoteDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param title Window title ("За корекция — REQ-…").
     * @param label What the note is for ("Какво липсва…").
     * @param confirmText Text of the confirm button ("Запиши присъдата").
     * @param parent Parent widget.
     */
    DraftNoteDialog(const QString &title, const QString &label,
                    const QString &confirmText, QWidget *parent = nullptr);

    /**
     * @brief The trimmed note currently typed.
     * @return The text; empty when nothing (or only spaces) was entered.
     */
    QString note() const;

    /** @brief The single input field (object name `draftNoteEdit`). */
    QLineEdit *noteEdit() const;

    /** @brief The confirm button (object name `draftNoteConfirm`). */
    QPushButton *confirmButton() const;

    /** @brief Pre-fills the field, for a caller that suggests a reason. */
    void setNote(const QString &note);

private:
    QLineEdit *m_noteEdit = nullptr;
    QPushButton *m_confirmButton = nullptr;
};

} // namespace Daqster

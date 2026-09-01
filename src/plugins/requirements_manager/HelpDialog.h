#pragma once

#include <QDialog>

class QTextBrowser;

namespace Daqster {

/**
 * @brief Built-in Help / Format documentation dialog.
 *
 * Renders an embedded HTML document covering the RDD workflow, the
 * requirement Markdown template, ID prefixes and the relationship axes
 * (Родител / Деца / Зависи от / Зависими от). Accessible from the
 * Requirements Manager toolbar ("Help / Format").
 */
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_browser;
};

} // namespace Daqster

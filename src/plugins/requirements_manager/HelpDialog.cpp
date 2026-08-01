#include "HelpDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace Daqster {

namespace {

QString documentationHtml()
{
    return QStringLiteral(R"(<html><body>
<h2>Requirements Manager — Help / Format</h2>

<h3>1. RDD (Requirements Driven Development) Process</h3>
<p>Every change in the project starts as a <b>requirement</b> (REQ) stored as a
Markdown file in <code>DevelopmentProcess/requirements/active/</code>. The multi-agent workflow is:</p>
<table border="1" cellspacing="0" cellpadding="4">
<tr><th>Step</th><th>Role</th><th>Responsibility</th></tr>
<tr><td>1</td><td>PM Agent</td><td>Defines the REQ + Acceptance Criteria</td></tr>
<tr><td>2</td><td>Architect Agent</td><td>Verifies architectural compatibility</td></tr>
<tr><td>3</td><td>Implementation Agent</td><td>Codes and links the commit to the REQ ID</td></tr>
<tr><td>4</td><td>QA Agent</td><td>Writes tests against the Acceptance Criteria and verifies</td></tr>
</table>
<p>Finished requirements are moved to <code>DevelopmentProcess/requirements/archive/</code> via
<b>Mark Done &amp; Archive</b>; archived ones can be reopened with <b>Reopen</b>.</p>

<h3>2. Requirement File Template</h3>
<pre>
# REQ-SW-XXX: Short Descriptive Title

- **Статус:** ACTIVE
- **Приоритет:** High | Medium | Low
- **Отговорник (роля):** PM | Architect | Implementation | QA
- **Дата:** YYYY-MM-DD
- **Родител:** REQ-XXX (or —)
- **Зависи от:** REQ-XXX, REQ-YYY (or —)

## Описание

Free-text description of the requirement.

## Acceptance Criteria

- [ ] Criterion 1
- [ ] Criterion 2

## Проследимост

- **Коммити:** —
- **Код:** path/
- **Документация:** docs/
- **Тестове:** —
</pre>

<h3>3. ID Naming &amp; Prefixes</h3>
<p>IDs use the form <code>REQ-&lt;PREFIX&gt;-&lt;NNN&gt;</code> with a zero-padded
three-digit number. <code>generateNextId()</code> automatically computes the next
free number when creating a new requirement.</p>
<table border="1" cellspacing="0" cellpadding="4">
<tr><th>Prefix</th><th>Area</th></tr>
<tr><td>SW</td><td>Requirements Manager &amp; Framework Tools</td></tr>
<tr><td>PLG</td><td>Plugin framework (discovery, loading, security)</td></tr>
<tr><td>AI</td><td>AI Studio features</td></tr>
<tr><td>SEC</td><td>Security</td></tr>
<tr><td>DOC</td><td>Documentation and processes</td></tr>
</table>

<h3>4. Relationship Axes</h3>
<table border="1" cellspacing="0" cellpadding="4">
<tr><th>Axis</th><th>Metadata</th><th>Meaning</th></tr>
<tr><td>Родител</td><td><code>- **Родител:**</code></td><td>Parent requirement this one derives from (hierarchy)</td></tr>
<tr><td>Деца</td><td>computed</td><td>Requirements that list this one as their parent</td></tr>
<tr><td>Зависи от</td><td><code>- **Зависи от:**</code></td><td>Requirements that must be done before/with this one</td></tr>
<tr><td>Зависими от</td><td>computed</td><td>Requirements that depend on this one</td></tr>
</table>
<p>Click any link in the details panel to jump to the referenced requirement.
Run <b>Validate</b> to detect dangling references, dependency cycles and missing fields.</p>
</body></html>)");
}

} // namespace

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Requirements Manager — Help / Format"));
    resize(760, 640);

    m_browser = new QTextBrowser(this);
    m_browser->setHtml(documentationHtml());
    m_browser->setOpenExternalLinks(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_browser);
    layout->addWidget(buttons);
    setLayout(layout);
}

} // namespace Daqster

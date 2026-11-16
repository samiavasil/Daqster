#include "RequirementPreview.h"

#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFragment>

namespace Daqster {
namespace RequirementPreview {

namespace {

QString linkFor(const QString &id)
{
    return QStringLiteral("<a href=\"req:%1\">%1</a>").arg(id);
}

//!< Inserts Markdown at the cursor with the platform's Markdown engine.
void insertMarkdown(QTextCursor &cursor, const QString &markdown)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    cursor.insertFragment(QTextDocumentFragment::fromMarkdown(markdown));
#else
    // Qt 5.14-5.15 render Markdown only *into* a document; a document fragment
    // would keep the text but drop the block formats (every heading becomes a
    // plain paragraph), so the blocks are copied format by format.
    QTextDocument scratch;
    scratch.setMarkdown(markdown);
    bool first = true;
    for (QTextBlock block = scratch.begin(); block.isValid(); block = block.next()) {
        if (!first)
            cursor.insertBlock(block.blockFormat());
        first = false;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.text().isEmpty())
                continue;
            cursor.insertText(fragment.text(), fragment.charFormat());
        }
    }
#endif
}

QString linkList(const QStringList &ids)
{
    if (ids.isEmpty())
        return QStringLiteral("—");
    QStringList links;
    for (const QString &id : ids)
        links.append(linkFor(id));
    return links.join(QStringLiteral(", "));
}

} // namespace

QString headerHtml(const Requirement &req,
                   const QStringList &childIds,
                   const QStringList &reverseIds)
{
    QString text;
    text += QStringLiteral("<h2>%1: %2</h2>")
                .arg(req.id.toHtmlEscaped(), req.title.toHtmlEscaped());
    text += QStringLiteral("<p><b>Status:</b> %1 &nbsp; <b>Priority:</b> %2 &nbsp; "
                           "<b>Assignee:</b> %3 &nbsp; <b>Date:</b> %4</p>")
                .arg(req.status.toHtmlEscaped(), req.priority.toHtmlEscaped(),
                     req.assignee.toHtmlEscaped(), req.date.toHtmlEscaped());

    // Repo badge (REQ-SW-PL-012): which requirements tree this requirement
    // comes from in the merged view.
    text += QStringLiteral("<p><b>Repo:</b> %1</p>")
                .arg(req.repo.isEmpty() ? QStringLiteral("—") : req.repo.toHtmlEscaped());

    // Phase status indicators (REQ-SW-PL-017): informative only — "✓" means the
    // traceability field is recorded ("записано"), NOT that the phase is verified.
    const PhaseStatus phase = phaseStatus(req);
    text += QStringLiteral("<p><b>Фази:</b> Архитектура %1 · Имплементация %2 · Тестове %3</p>")
                .arg(phase.architecture ? QStringLiteral("✓") : QStringLiteral("✗"),
                     phase.implementation ? QStringLiteral("✓") : QStringLiteral("✗"),
                     phase.testing ? QStringLiteral("✓") : QStringLiteral("✗"));

    // Relationship axes: Родител / Деца / Зависи от / Зависими от
    text += QStringLiteral("<p><b>Родител:</b> %1</p>")
                .arg(req.parentId.trimmed().isEmpty() ? QStringLiteral("—")
                                                      : linkFor(req.parentId));
    text += QStringLiteral("<p><b>Деца:</b> %1</p>").arg(linkList(childIds));
    text += QStringLiteral("<p><b>Зависи от:</b> %1</p>").arg(linkList(req.dependencies));
    text += QStringLiteral("<p><b>Зависими от:</b> %1</p>").arg(linkList(reverseIds));
    text += QStringLiteral("<hr/>");
    return text;
}

QString markdownSource(const Requirement &req)
{
    if (!req.rawContent.trimmed().isEmpty())
        return req.rawContent;
    // Fallback for records parsed without raw content (e.g. a test fixture or a
    // caller that built the Requirement by hand): render the description under
    // the canonical heading (RDD-PROCESS.md → „Поколения на шаблона“).
    return QStringLiteral("## Накратко\n\n%1\n").arg(req.description);
}

void render(QTextBrowser *view,
            const Requirement &req,
            const QStringList &childIds,
            const QStringList &reverseIds)
{
    if (!view)
        return;

    QTextDocument *document = view->document();
    document->setHtml(headerHtml(req, childIds, reverseIds));

    // Append the file's own Markdown after the header: the whole .md becomes
    // visible (headings, lists, tables, traceability) instead of only the
    // fields the parser extracted.
    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    insertMarkdown(cursor, markdownSource(req));

    // Headers longer than the pane are useless: start at the top.
    cursor.movePosition(QTextCursor::Start);
    view->setTextCursor(cursor);
}

} // namespace RequirementPreview
} // namespace Daqster

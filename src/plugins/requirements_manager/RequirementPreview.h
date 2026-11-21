#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include "RequirementsParser.h"

class QTextBrowser;

namespace Daqster {

/**
 * @brief Rendering of a requirement into the preview pane (REQ-SW-PL-001).
 *
 * Two halves, kept apart so both are testable without the whole widget:
 *
 *   - a small rich-text header built from the PARSED fields (id/title/status,
 *     repo badge, phase indicators, relationship links);
 *   - the file's own Markdown, rendered with the real Markdown engine
 *     (`QTextDocument::setMarkdown`, Qt 5.14+) instead of hand-built HTML, so
 *     every section of the .md stays visible.
 *
 * The relationship/child/reverse lists are supplied by the caller because only
 * the widget holds the full requirement set.
 */
namespace RequirementPreview {

/**
 * @brief Builds the rich-text header block shown above the Markdown body.
 *
 * @param req The requirement (parsed fields + raw Markdown).
 * @param childIds IDs of requirements whose parentId is @p req (Деца).
 * @param reverseIds IDs of requirements that depend on @p req (Зависими от).
 * @param today Reference day for the age shown next to the creation date
 *        ("Създадено: 2026-11-21 · преди 12 дни"); invalid means the current
 *        day, which is what the widget passes (REQ-SW-PL-100).
 * @return HTML fragment; links use the `req:<ID>` scheme the widget handles.
 */
QString headerHtml(const Requirement &req,
                   const QStringList &childIds,
                   const QStringList &reverseIds,
                   const QDate &today = QDate());

/**
 * @brief Renders the header plus the whole raw Markdown into @p view.
 *
 * The document is rebuilt from scratch (header HTML, then a Markdown fragment
 * appended at the end), and the view is left without a fixed height so it
 * scrolls as one document.
 *
 * @param view The preview browser (must already have setOpenLinks(false) when
 *        the caller wants to handle navigation itself).
 * @param req The requirement to render.
 * @param childIds Forwarded to headerHtml().
 * @param reverseIds Forwarded to headerHtml().
 * @param today Reference day for the creation age (invalid = today).
 */
void render(QTextBrowser *view,
            const Requirement &req,
            const QStringList &childIds,
            const QStringList &reverseIds,
            const QDate &today = QDate());

/**
 * @brief The Markdown source to render for @p req.
 *
 * Exposed so a test can prove the body comes from the file (not from the
 * parsed fields) and that an empty rawContent falls back to the description.
 */
QString markdownSource(const Requirement &req);

} // namespace RequirementPreview
} // namespace Daqster

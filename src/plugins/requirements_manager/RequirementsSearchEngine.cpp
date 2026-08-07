#include "RequirementsSearchEngine.h"

namespace Daqster {

namespace {

bool isKnownFieldKey(const QString &key)
{
    return key == QStringLiteral("id")
           || key == QStringLiteral("status")
           || key == QStringLiteral("priority")
           || key == QStringLiteral("assignee")
           || key == QStringLiteral("repo")
           || key == QStringLiteral("section");
}

QString fieldValue(const Requirement &req, const QString &key)
{
    if (key == QStringLiteral("id"))
        return req.id;
    if (key == QStringLiteral("status"))
        return req.status;
    if (key == QStringLiteral("priority"))
        return req.priority;
    if (key == QStringLiteral("assignee"))
        return req.assignee;
    if (key == QStringLiteral("repo"))
        return req.repo;
    if (key == QStringLiteral("section"))
        return req.section;
    return QString();
}

// True when @p term matches @p req. A term of the form "<known-key>:<value>"
// (split on the FIRST colon) restricts the match to that structured field;
// anything else falls back to full-text search over the normalized blob.
bool matchesTerm(const QString &normalizedBlob, const Requirement &req,
                 const QString &term)
{
    const int colon = term.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        const QString key = term.left(colon);
        const QString value = term.mid(colon + 1).trimmed();
        if (!value.isEmpty() && isKnownFieldKey(key)) {
            return fieldValue(req, key).toLower().contains(value.toLower());
        }
    }
    return normalizedBlob.contains(term.toLower());
}

} // namespace

QStringList RequirementsSearchEngine::tokenize(const QString &query)
{
    QStringList tokens;
    QString current;
    for (const QChar &ch : query) {
        if (ch.isSpace()) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else {
            current.append(ch);
        }
    }
    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

QString RequirementsSearchEngine::normalizedText(const Requirement &req)
{
    QStringList parts;
    parts << req.id
          << req.title
          << req.description
          << req.acceptanceCriteria
          << req.traceability
          << req.commits
          << req.code
          << req.tests
          << req.parentId
          << req.dependencies
          << req.status
          << req.priority
          << req.assignee
          << req.repo
          << req.section
          << req.fileName
          << req.date;
    return parts.join(QLatin1Char(' ')).toLower();
}

QVector<Requirement> RequirementsSearchEngine::filter(
    const QVector<Requirement> &requirements, const QString &query)
{
    const QStringList terms = tokenize(query);
    if (terms.isEmpty())
        return requirements;

    QVector<Requirement> result;
    for (const Requirement &req : requirements) {
        // One lowercased, normalized index blob per requirement in a single
        // pass — this is the per-call index (REQ-SW-PL-011 AC4).
        const QString blob = normalizedText(req);
        bool allMatch = true;
        for (const QString &term : terms) {
            if (!matchesTerm(blob, req, term)) {
                allMatch = false;
                break;
            }
        }
        if (allMatch)
            result.append(req);
    }
    return result;
}

} // namespace Daqster

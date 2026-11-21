#include "RequirementsParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QFile>

#include "LogCategories.h"

namespace Daqster {

namespace {

// True when every character of s is a digit (and s is non-empty). Used by
// generateNextId() to enforce an exact prefix-family match: the remainder
// after "REQ-<PREFIX>-" must be a pure number so typed families (REQ-SW-PL-*,
// REQ-SW-FW-*, ...) never pollute each other's max+1 scan.
bool numberStrIsDigits(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar &c : s) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

// A requirement reference inside a "Родител:" / "Зависи от:" / "Деца:" value:
// REQ-<PREFIX>-<NNN> (private scheme) or REQ-SW-<TYPE>-<NNN> (typed public one).
// The ID is what a reference IS; the text around it is prose.
const QRegularExpression &requirementIdRe()
{
    static const QRegularExpression re(
        QStringLiteral("\\b(REQ-[A-Z]+(?:-[A-Z]+)?-\\d{3})\\b"));
    return re;
}

// Any other document ID a header may cite (ADR-AI-013, ISS-007, CND-046). Such
// a token is still a reference — kept and reported as "not a requirement"
// instead of being silently dropped or looked up as a requirement ID.
const QRegularExpression &documentIdRe()
{
    static const QRegularExpression re(
        QStringLiteral("\\b([A-Z]{2,}(?:-[A-Z]{2,})*-\\d{2,4})\\b"));
    return re;
}

// The description section has three spellings in the tree, one per template
// generation: "Накратко" (v2 — canonical, see RDD-PROCESS.md „Поколения на
// шаблона"), "Описание" (v1) and "Проблем" plus its optional "Цел" (v0, the
// oldest files). Recognizing only the first made 60 requirements look
// undescribed while they plainly had a description.
//
// The generation itself is kept in Requirement::templateVersion so the
// validator can report a deviation as a Warning without hiding the text.
QString descriptionVersion(const QString &heading)
{
    return RequirementTemplate::versionForHeading(heading);
}

// The acceptance-criteria section: "Acceptance Criteria",
// "Изисквания (Acceptance Criteria)", the template's
// "Как ще разберем, че работи[(AC)]" and the Bulgarian translation of the
// first one, "Критерии за приемане". Missing the last one was not cosmetic:
// the checkbox bullets were still recognized (they are recognized anywhere),
// but a "### ACn" heading or a numbered/table entry under that heading was
// invisible, so those requirements were graded "no acceptance criteria".
bool isCriteriaHeading(const QString &heading)
{
    const QString text = heading.trimmed();
    if (text.contains(QStringLiteral("Acceptance Criteria"), Qt::CaseInsensitive))
        return true;
    if (text.startsWith(QStringLiteral("Как ще разберем, че работи")))
        return true;
    if (text.startsWith(QStringLiteral("Критерии за приемане")))
        return true;
    return text == QStringLiteral("Изисквания");
}

// A criterion written as a heading: "### [x] AC1 — text" (state in the
// checkbox) or "### AC1 — text" (no state — the criterion stays open, exactly
// as an unchecked bullet does).
bool appendCriterionHeading(const QString &line, Requirement &out)
{
    static const QRegularExpression boxed(
        QStringLiteral("^###\\s+\\[([ xX])\\]\\s*(?:AC\\s*\\d+)?\\s*[—–:.-]?\\s*(.*)$"));
    static const QRegularExpression plain(
        QStringLiteral("^###\\s+AC\\s*\\d+\\b\\s*[—–:.-]?\\s*(.*)$"));

    QRegularExpressionMatch match = boxed.match(line);
    bool done = false;
    if (match.hasMatch()) {
        done = match.captured(1).toLower() == QStringLiteral("x");
    } else {
        match = plain.match(line);
        if (!match.hasMatch())
            return false;
    }
    const QString text = match.captured(match.lastCapturedIndex()).trimmed();
    if (text.isEmpty())
        return false;
    out.acceptanceCriteria.append(text);
    out.criteriaDone.append(done);
    return true;
}

// Accepts one of the three ways the tree writes acceptance criteria (a
// checkbox bullet, an "### ACn" heading or a numbered entry inside the
// criteria section) plus the table form "| 3 | text | ✅ |". Everything here is
// section-scoped by the caller: a numbered list or a table elsewhere in the
// file is prose, not criteria.
bool appendCriterionLine(const QString &line, Requirement &out)
{
    static const QRegularExpression numbered(
        QStringLiteral("^\\d+\\.\\s+(?:\\*\\*)?(.*?)(?:\\*\\*)?\\s*$"));
    static const QRegularExpression tableRow(
        QStringLiteral("^\\|\\s*(\\d+)\\s*\\|([^|]*)\\|([^|]*)\\|"));

    const QString trimmed = line.trimmed();
    const QRegularExpressionMatch row = tableRow.match(trimmed);
    if (row.hasMatch()) {
        const QString text = row.captured(2).trimmed();
        if (text.isEmpty())
            return false;
        // The status cell is human prose; ✅ means done, ☐ explicitly open.
        out.acceptanceCriteria.append(text);
        out.criteriaDone.append(row.captured(3).contains(QStringLiteral("✅")));
        return true;
    }

    const QRegularExpressionMatch item = numbered.match(trimmed);
    if (!item.hasMatch())
        return false;
    const QString text = item.captured(1).trimmed();
    if (text.isEmpty())
        return false;
    out.acceptanceCriteria.append(text);
    out.criteriaDone.append(false);
    return true;
}

/**
 * @brief One reference parsed out of a metadata value.
 */
struct ParsedReference
{
    QString id;                    //!< the ID token, empty for prose
    QString hint;                  //!< annotation text (parenthesised), or empty
    bool isRequirement = false;    //!< true when the token matched REQ-<…>
};

// Splits a metadata value on TOP-LEVEL commas only — a comma inside
// parentheses belongs to the annotation, not to the next reference.
//
// Splitting on every comma was the defect: an annotated dependency such as
// "REQ-AI-005 (интероп слой — предшественик, `DONE`)" became two "references"
// ("REQ-AI-005 (интероп слой — предшественик" and "`DONE` )"), each reported as
// a dangling dependency — 40 of the 46 errors the GUI showed on the real tree
// while the CLI showed 2.
QStringList splitTopLevelCommas(const QString &value)
{
    QStringList parts;
    QString current;
    int depth = 0;
    for (const QChar &c : value) {
        if (c == QLatin1Char('('))
            ++depth;
        else if (c == QLatin1Char(')') && depth > 0)
            --depth;
        if (c == QLatin1Char(',') && depth == 0) {
            parts.append(current);
            current.clear();
            continue;
        }
        current.append(c);
    }
    parts.append(current);
    return parts;
}

// Parses one top-level item of a metadata value: the ID token it starts from
// plus the annotation attached to it. Items with no ID at all are prose ("—",
// "DONE") and yield an empty id — a reference is an ID, never a sentence.
ParsedReference parseReferenceItem(const QString &rawItem)
{
    ParsedReference ref;
    const QString item = rawItem.trimmed();
    if (item.isEmpty() || item.startsWith(QChar(0x2014))) // "—"
        return ref;

    QRegularExpressionMatch match = requirementIdRe().match(item);
    if (match.hasMatch()) {
        ref.isRequirement = true;
    } else {
        match = documentIdRe().match(item);
        if (!match.hasMatch())
            return ref;
    }
    ref.id = match.captured(1);

    // The annotation is the parenthesised text right after the ID. Prose that
    // follows without parentheses is a comment, not a repo hint.
    const QString rest = item.mid(match.capturedEnd()).trimmed();
    if (rest.startsWith(QLatin1Char('('))) {
        const int close = rest.lastIndexOf(QLatin1Char(')'));
        if (close > 0)
            ref.hint = rest.mid(1, close - 1).trimmed();
    }
    return ref;
}

// Appends the references of a whole metadata value to a list of IDs, keyed
// annotations into @p hints, and returns the first requirement ID found (used
// as the single "Родител:" of the tree).
QString appendReferences(const QString &value, QStringList &ids,
                         QHash<QString, QString> &hints)
{
    ids.clear();
    QString firstRequirementId;
    for (const QString &item : splitTopLevelCommas(value)) {
        const ParsedReference ref = parseReferenceItem(item);
        if (ref.id.isEmpty())
            continue;
        if (!ids.contains(ref.id))
            ids.append(ref.id);
        if (!ref.hint.isEmpty() && !hints.contains(ref.id))
            hints.insert(ref.id, ref.hint);
        if (ref.isRequirement && firstRequirementId.isEmpty())
            firstRequirementId = ref.id;
    }
    return firstRequirementId;
}

QString sectionOf(const QString &baseDir, const QString &absolutePath)
{
    const QString dir = QDir(baseDir).absolutePath();
    // baseDir may point at the repo root (contains DevelopmentProcess/
    // requirements) or at the DevelopmentProcess/requirements/ directory itself.
    const QStringList candidates = {
        QDir(dir).filePath(QString::fromUtf8(kRequirementsSubdir)),
        dir
    };
    // Normalize the absolute path once for consistent comparison.
    const QString cleanAbsolutePath = QDir::toNativeSeparators(QDir::cleanPath(absolutePath));
    for (const QString &candidate : candidates) {
        const QString activePath = QDir::toNativeSeparators(QDir::cleanPath(QDir(candidate).filePath(QStringLiteral("active"))));
        const QString archivePath = QDir::toNativeSeparators(QDir::cleanPath(QDir(candidate).filePath(QStringLiteral("archive"))));
        const QString activePrefix = activePath + QDir::separator();
        const QString archivePrefix = archivePath + QDir::separator();
        // Use case-insensitive comparison for cross-platform compatibility
        // (Windows FS is case-insensitive; harmless on Linux).
        if (cleanAbsolutePath.startsWith(activePrefix, Qt::CaseInsensitive))
            return QStringLiteral("active");
        if (cleanAbsolutePath.startsWith(archivePrefix, Qt::CaseInsensitive))
            return QStringLiteral("archive");
    }
    return QString();
}

void collectMarkdownFiles(const QString &dirPath, QVector<QFileInfo> &out)
{
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectMarkdownFiles(entry.absoluteFilePath(), out);
        } else if (entry.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0) {
            out.append(entry);
        }
    }
}

// Resolves the directory a parse must WALK — the scope of the requirement set.
//
// A caller may pass a repo root (containing DevelopmentProcess/requirements) or
// the requirements directory itself. Walking the repo root collected every
// Markdown file under it whose H1 merely looks like a requirement: drafts in
// DevelopmentProcess/plans/ (DRAFT-PL-06x under ideas/ plus their archive/
// copies → the same ID twice → "duplicate requirement ID"), design documents
// that quote a template, and one full copy of REQ-SW-PL-050 living in plans/.
// The traceability CLI never sees those — it scans
// DevelopmentProcess/requirements — so the GUI reported ~47 errors where the
// CLI reported 2. The scope is therefore the requirements tree itself.
QString scanRootOf(const QString &baseDir)
{
    const QDir base(baseDir);
    const QString tree = base.filePath(QString::fromUtf8(kRequirementsSubdir));
    if (QDir(tree).exists())
        return tree;
    // The caller already pointed at the requirements tree (or at a directory
    // with no requirements subdirectory at all, e.g. a test fixture).
    return base.absolutePath();
}

// Appends one row to the Markdown table under @p heading, creating the heading
// and the header/separator pair when either is absent (REQ-SW-PL-101/102).
//
// One code path for the history table and the draft review table: both are
// "a table at the end of the file", and two implementations would drift on the
// blank-line handling that decides whether the table renders at all.
QString appendTableRow(const QString &content, const QString &heading,
                       const QString &header, const QString &separator,
                       const QString &row, const QString &eol)
{
    const auto line = [&eol](const QString &text) { return text + eol; };
    QStringList lines = content.split(QLatin1Char('\n'));
    const int headingIndex = [&lines, &heading]() {
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).trimmed() == heading)
                return i;
        }
        return -1;
    }();

    if (headingIndex < 0) {
        // The section does not exist yet: it goes at the END of the file, after
        // the traceability block, so the metadata stays at the top.
        while (lines.size() > 1 && lines.last().trimmed().isEmpty())
            lines.removeLast();
        lines.append(line(QString()));
        lines.append(line(heading));
        lines.append(line(QString()));
        lines.append(line(header));
        lines.append(line(separator));
        lines.append(line(row));
        // A bare empty last item: join() then ends the file with one newline
        // (with the file's own ending), the same way every other file ends.
        lines.append(QString());
        return lines.join(QLatin1Char('\n'));
    }

    int lastPipe = -1;
    for (int i = headingIndex + 1; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!trimmed.startsWith(QLatin1Char('|')))
            break;
        lastPipe = i;
    }

    if (lastPipe >= 0) {
        lines.insert(lastPipe + 1, line(row));
        return lines.join(QLatin1Char('\n'));
    }

    // Heading present but no table under it: add the header pair, keeping the
    // one blank line a Markdown table needs between the heading and itself.
    const int next = headingIndex + 1;
    const bool blankNext = next < lines.size() && lines.at(next).trimmed().isEmpty();
    const int insertAt = blankNext ? next + 1 : next;
    if (!blankNext)
        lines.insert(insertAt, line(QString()));
    const int at = blankNext ? insertAt : insertAt + 1;
    lines.insert(at, line(header));
    lines.insert(at + 1, line(separator));
    lines.insert(at + 2, line(row));
    return lines.join(QLatin1Char('\n'));
}

// The data rows of the table under @p heading (header and separator excluded).
QStringList tableDataRows(const QString &content, const QString &heading)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    int headingIndex = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).trimmed() == heading) {
            headingIndex = i;
            break;
        }
    }
    if (headingIndex < 0)
        return {};

    QStringList rows;
    bool sawSeparator = false;
    for (int i = headingIndex + 1; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (!trimmed.startsWith(QLatin1Char('|'))) {
            if (rows.isEmpty() && trimmed.isEmpty())
                continue;
            if (!rows.isEmpty())
                break;
            continue;
        }
        // The `|---|` separator carries no verdict.
        if (!sawSeparator
            && trimmed.count(QLatin1Char('-')) >= trimmed.count(QLatin1Char('|'))) {
            sawSeparator = true;
            continue;
        }
        const QStringList cells = trimmed.split(QLatin1Char('|'));
        // "| a | b |" splits into ["", " a ", " b ", ""]: the first cell is
        // the header word ("кръг", "дата"), never a row.
        const QString first = cells.value(1).trimmed();
        if (first == QStringLiteral("кръг") || first == QStringLiteral("дата"))
            continue;
        rows.append(trimmed);
    }
    return rows;
}

// Reads the current "- **Статус:**" token of a file's content, or an empty
// string when there is no status line.
QString statusTokenInContent(const QString &content)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
        if (!trimmed.startsWith(QStringLiteral("- **Статус:**")))
            continue;
        QString value = trimmed;
        value.remove(0, value.indexOf(QLatin1Char(':')) + 1);
        value.remove(QLatin1Char('*')).remove(QLatin1Char('`'));
        return value.simplified().section(QLatin1Char(' '), 0, 0);
    }
    return QString();
}

// Rewrites the status line in @p content; returns the new content.
QString contentWithStatus(const QString &content, const QString &status,
                          const QString &eol)
{
    QStringList lines = content.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
        if (!trimmed.startsWith(QStringLiteral("- **Статус:**")))
            continue;
        lines[i] = QStringLiteral("- **Статус:** %1").arg(status) + eol;
        return lines.join(QLatin1Char('\n'));
    }
    return content;
}

// Rewrites "- **Ревизия:** N" to @p revision, inserting the field after the
// status line when the file does not carry it yet.
QString contentWithRevision(const QString &content, int revision, const QString &eol)
{
    QStringList lines = content.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
        if (!trimmed.startsWith(QStringLiteral("- **Ревизия:**")))
            continue;
        lines[i] = QStringLiteral("- **Ревизия:** %1").arg(revision) + eol;
        return lines.join(QLatin1Char('\n'));
    }

    // No revision field yet: it belongs next to the other header dates — after
    // "Одобрено:" when there is one, else after the status, else before the
    // first section.
    int insertAt = 0;
    bool anchored = false;
    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
        if (trimmed.startsWith(QStringLiteral("- **Одобрено:**"))) {
            insertAt = i + 1;
            anchored = true;
            break;
        }
        if (!anchored && trimmed.startsWith(QStringLiteral("- **Статус:**"))) {
            insertAt = i + 1;
            anchored = true;
        }
    }
    lines.insert(insertAt, QStringLiteral("- **Ревизия:** %1").arg(revision) + eol);
    return lines.join(QLatin1Char('\n'));
}

// Builds the whole "one history row + revision bump" edit on a content string.
QString contentWithHistoryRow(const QString &content, const QString &date,
                              const QString &change, const QString &who,
                              const QString &why)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    const QString eol = lines.first().endsWith(QLatin1Char('\r'))
                            ? QStringLiteral("\r") : QString();
    const QString row = QStringLiteral("| %1 | %2 | %3 | %4 |")
                            .arg(date.trimmed(), change.trimmed(), who.trimmed(),
                                 why.trimmed());
    const QString withRow = appendTableRow(
        content, QStringLiteral("## История"),
        QStringLiteral("| дата | промяна | кой | защо |"),
        QStringLiteral("|---|---|---|---|"), row, eol);
    const int rows = tableDataRows(withRow, QStringLiteral("## История")).size();
    const int current = RequirementsParser::revisionInContent(content);
    const int next = qMax(current + (current > 0 ? 1 : 0), rows);
    return contentWithRevision(withRow, next > 0 ? next : 1, eol);
}

// A rename that never overwrites: "нищо не се трие" (REQ-SW-PL-102).
bool renameStrict(const QString &source, const QString &target, QString *errorOut)
{
    if (QFile::exists(target)) {
        if (errorOut)
            *errorOut = QStringLiteral("в архива вече има файл със същото име: %1")
                            .arg(target);
        return false;
    }
    if (!QFile::rename(source, target)) {
        if (errorOut)
            *errorOut = QStringLiteral("файлът не може да се премести: %1 → %2")
                            .arg(source, target);
        return false;
    }
    return true;
}

bool writeContentBytes(const QString &filePath, const QString &content,
                       QString *errorOut)
{
    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("файлът не може да се запише: %1").arg(filePath);
        return false;
    }
    out.write(content.toUtf8());
    if (!out.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("записът не завърши: %1").arg(filePath);
        return false;
    }
    return true;
}

// The whole file, read as UTF-8 without any line-ending translation.
bool readContentBytes(const QString &filePath, QString *content, QString *errorOut)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("файлът не може да се прочете: %1").arg(filePath);
        return false;
    }
    *content = QString::fromUtf8(file.readAll());
    file.close();
    return true;
}

} // namespace

namespace RequirementTemplate {

QString canonicalVersion()
{
    return QStringLiteral("v2");
}

QString canonicalHeading()
{
    return QStringLiteral("Накратко");
}

QString versionForHeading(const QString &heading)
{
    const QString text = heading.trimmed();
    // v2 first: it is the only one an author should be writing today.
    if (text == canonicalHeading())
        return canonicalVersion();
    if (text == QStringLiteral("Описание"))
        return QStringLiteral("v1");
    // v0 is "## Проблем" plus its optional "## Цел": both belong to the same
    // generation, and Цел alone is still the oldest template, not a fourth one.
    if (text == QStringLiteral("Проблем") || text == QStringLiteral("Цел"))
        return QStringLiteral("v0");
    return QString();
}

QString headingForVersion(const QString &version)
{
    if (version == canonicalVersion())
        return canonicalHeading();
    if (version == QStringLiteral("v1"))
        return QStringLiteral("Описание");
    if (version == QStringLiteral("v0"))
        return QStringLiteral("Проблем");
    return QString();
}

} // namespace RequirementTemplate

QVector<Requirement> RequirementsParser::parseDirectory(const QString &baseDir)
{
    QVector<Requirement> result;
    QVector<QFileInfo> files;
    // Scope = the requirements tree, never the whole repo root (see scanRootOf).
    collectMarkdownFiles(scanRootOf(baseDir), files);

    for (const QFileInfo &fileInfo : files) {
        const QString section = sectionOf(baseDir, fileInfo.absoluteFilePath());
        Requirement req;
        if (parseFile(fileInfo, section, req)) {
            result.append(req);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const Requirement &a, const Requirement &b) {
                  return a.id < b.id;
              });

    return result;
}

QString RequirementsParser::repoForId(const QString &id)
{
    // The typed public scheme is REQ-SW-<TYPE>-<NN>; everything else under
    // REQ-<PREFIX>-<NN> is treated as private (PLG/AI/SEC/DOC...).
    if (id.startsWith(QStringLiteral("REQ-SW-"), Qt::CaseInsensitive))
        return QStringLiteral("public");
    if (id.startsWith(QStringLiteral("REQ-"), Qt::CaseInsensitive))
        return QStringLiteral("private");
    return QStringLiteral("other");
}

QVector<Requirement> RequirementsParser::parseDirectories(const QVector<RequirementRoot> &roots)
{
    QVector<Requirement> result;
    // Roots are deduplicated by canonical path (discoverRepoRoots), but the
    // same physical file can still be reached via multiple roots — e.g. a root
    // at the repo top level plus a root at DevelopmentProcess/requirements, or
    // two sibling roots where one is a symlink. Dedup by canonical FILE path so
    // each file is parsed exactly once.
    QSet<QString> seenFiles;
    for (const RequirementRoot &root : roots) {
        QVector<Requirement> parsed = parseDirectory(root.repoRoot);
        for (Requirement &req : parsed) {
            const QFileInfo fileInfo(req.filePath);
            const QString canonical = fileInfo.canonicalFilePath();
            const QString key = canonical.isEmpty() ? fileInfo.absoluteFilePath()
                                                    : canonical;
            if (seenFiles.contains(key))
                continue;
            seenFiles.insert(key);
            req.repo = repoForId(req.id);
            result.append(req);
        }
    }

    // Stable sort so ties inside one ID family keep their per-root order
    // (parseDirectory already sorts by ID within a root).
    std::stable_sort(result.begin(), result.end(),
                     [](const Requirement &a, const Requirement &b) {
                         const int byId = QString::compare(a.id, b.id, Qt::CaseInsensitive);
                         if (byId != 0)
                             return byId < 0;
                         return QString::compare(a.repo, b.repo, Qt::CaseInsensitive) < 0;
                     });
    return result;
}

QStringList RequirementsParser::discoverRepoRoots()
{
    QStringList roots;

    // 1. Walk up from the application binary to the primary root (the first
    //    directory containing DevelopmentProcess/requirements) — the
    //    historical single-root discovery.
    QDir dir(QCoreApplication::applicationDirPath());
    while (!QDir(dir.filePath(QString::fromUtf8(kRequirementsSubdir))).exists()
           && dir.cdUp()) {
    }
    const QString primary =
        QDir(dir.filePath(QString::fromUtf8(kRequirementsSubdir))).exists()
        ? dir.absolutePath()
        : QCoreApplication::applicationDirPath();
    roots.append(primary);

    // 2. Scan the parent directory of the primary root for SIBLING
    //    directories that also contain DevelopmentProcess/requirements.
    QDir parent(primary);
    if (parent.cdUp()) {
        const QFileInfoList entries =
            parent.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (QDir(entry.absoluteFilePath())
                    .exists(QString::fromUtf8(kRequirementsSubdir))) {
                roots.append(entry.absoluteFilePath());
            }
        }
    }

    // 3. Deduplicate canonical absolute paths and sort.
    QStringList canonical;
    QSet<QString> seen;
    for (const QString &root : roots) {
        const QString canon = QDir(root).canonicalPath();
        if (canon.isEmpty() || seen.contains(canon))
            continue;
        seen.insert(canon);
        canonical.append(canon);
    }
    std::sort(canonical.begin(), canonical.end());

    // 4. Warn if only one root found -- the private/public sibling is missing.
    //    REQ-SW-PL-012 expects both repos side-by-side: ../daqster (public) and
    //    ../DaqsterAiStudio (private). Without both, cross-repo references
    //    (traceability-matrix, RDD-STATUS) cannot be resolved correctly.
    if (canonical.size() == 1) {
        qCWarning(lcFramework) << "RequirementsManager: only ONE requirements root found:"
                               << canonical.first()
                               << "\nExpected TWO sibling roots (public + private):"
                               << "\n  e.g. /path/to/daqster and /path/to/DaqsterAiStudio"
                               << "\nCross-repo traceability will be incomplete.";
    }
    return canonical;
}

bool RequirementsParser::writeRequirement(const Requirement &req)
{
    QFile file(req.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qCWarning(lcFramework) << "RequirementsParser: cannot write" << req.filePath;
        return false;
    }
    QTextStream out(&file);
    out << req.rawContent;
    file.close();
    return true;
}

void RequirementsParser::setCriterionChecked(Requirement &req, int index, bool done)
{
    if (index < 0 || index >= req.acceptanceCriteria.size())
        return;

    QStringList lines = req.rawContent.split(QStringLiteral("\n"));
    const QString marker = QStringLiteral("- [");

    int matched = 0;
    for (QString &line : lines) {
        if (line.startsWith(marker)) {
            if (matched == index) {
                line = QStringLiteral("- [%1] %2")
                           .arg(done ? QStringLiteral("x") : QStringLiteral(" "))
                           .arg(req.acceptanceCriteria.at(index));
                req.criteriaDone[index] = done;
                break;
            }
            ++matched;
        }
    }
    req.rawContent = lines.join(QStringLiteral("\n"));
}

void RequirementsParser::setStatusLine(Requirement &req, const QString &status)
{
    QStringList lines = req.rawContent.split(QStringLiteral("\n"));
    bool found = false;
    QStringList updated;
    for (const QString &line : lines) {
        if (!found && line.trimmed().startsWith(QStringLiteral("- **Статус:**"))) {
            updated.append(QStringLiteral("- **Статус:** %1").arg(status));
            found = true;
        } else {
            updated.append(line);
        }
    }
    if (!found) {
        int insertAt = 0;
        while (insertAt < updated.size()
               && !updated.at(insertAt).trimmed().startsWith(QStringLiteral("## ")))
            ++insertAt;
        updated.insert(insertAt, QStringLiteral("- **Статус:** %1").arg(status));
    }
    req.rawContent = updated.join(QStringLiteral("\n"));
    req.status = status;
}

void RequirementsParser::setDependenciesLine(Requirement &req,
                                             const QStringList &dependencies)
{
    QStringList valueParts;
    for (const QString &dep : dependencies) {
        if (dep.isEmpty() || dep == QStringLiteral("—"))
            continue;
        valueParts.append(dep);
    }
    const QString newLine = valueParts.isEmpty()
        ? QStringLiteral("- **Зависи от:** —")
        : QStringLiteral("- **Зависи от:** %1").arg(valueParts.join(QStringLiteral(", ")));

    QStringList lines = req.rawContent.split(QStringLiteral("\n"));
    bool found = false;
    QStringList updated;
    for (const QString &line : lines) {
        if (!found && line.trimmed().startsWith(QStringLiteral("- **Зависи от:**"))) {
            updated.append(newLine);
            found = true;
        } else {
            updated.append(line);
        }
    }
    if (!found) {
        int insertAt = 0;
        while (insertAt < updated.size()
               && !updated.at(insertAt).trimmed().startsWith(QStringLiteral("## ")))
            ++insertAt;
        updated.insert(insertAt, newLine);
    }
    req.rawContent = updated.join(QStringLiteral("\n"));
    req.dependencies = valueParts;
}

bool RequirementsParser::approveDraft(const QString &filePath, const QString &date,
                                      QString *errorOut)
{
    const auto failWith = [errorOut](const QString &reason) {
        if (errorOut)
            *errorOut = reason;
        return false;
    };

    if (date.trimmed().isEmpty())
        return failWith(QStringLiteral("датата на одобрение е празна"));

    QFile file(filePath);
    // Binary mode on purpose: the promise is that every byte except the status
    // line survives, and a text-mode round-trip would rewrite CRLF on Windows.
    if (!file.open(QIODevice::ReadOnly))
        return failWith(QStringLiteral("файлът не може да се прочете: %1").arg(filePath));
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Split/join on '\n' is byte-exact: every line keeps its own "\r" (if the
    // file is CRLF) and the trailing newline survives as an empty last item.
    QStringList lines = content.split(QLatin1Char('\n'));
    const QString eolSuffix =
        lines.first().endsWith(QLatin1Char('\r')) ? QStringLiteral("\r") : QString();

    const int statusLine = [&lines]() {
        for (int i = 0; i < lines.size(); ++i) {
            const QString trimmed = lines.at(i).trimmed();
            if (trimmed.startsWith(QStringLiteral("- **Статус:**")))
                return i;
            // The metadata block ends at the first heading: a "Статус" word in
            // the body must never be read as the requirement's status.
            if (trimmed.startsWith(QStringLiteral("## ")))
                break;
        }
        return -1;
    }();
    if (statusLine < 0)
        return failWith(QStringLiteral("няма ред \"- **Статус:**\" в %1").arg(filePath));

    const QString statusValue = [](QString line) {
        // The colon sits INSIDE the bold markers (`- **Статус:** DRAFT`), so the
        // value is what follows the first colon once emphasis is dropped.
        line.remove(0, line.indexOf(QLatin1Char(':')) + 1);
        line.remove(QLatin1Char('*')).remove(QLatin1Char('`'));
        return line.simplified();
    }(lines.at(statusLine));
    const QString token = statusValue.section(QLatin1Char(' '), 0, 0);
    // Not a draft: nothing to approve, and nothing to rewrite. A second accept
    // therefore cannot re-date an already accepted requirement.
    if (token != QStringLiteral("DRAFT"))
        return true;

    lines[statusLine] = QStringLiteral("- **Статус:** ACTIVE") + eolSuffix;

    const QString approvalLine =
        QStringLiteral("- **Одобрено:** %1, потребител").arg(date.trimmed()) + eolSuffix;
    int approvalLineIndex = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.startsWith(QStringLiteral("- **Одобрено:**"))) {
            approvalLineIndex = i;
            break;
        }
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
    }
    if (approvalLineIndex >= 0)
        lines[approvalLineIndex] = approvalLine;
    else
        lines.insert(statusLine + 1, approvalLine);

    // The draft banner (" > **Чернова — чака одобрение…**") states the file's
    // status, so an approved file must not keep it: ACTIVE plus "waits for
    // approval" contradicts itself. Only that line goes; the blank line it
    // leaves behind is dropped as well so no doubled empty line appears.
    for (int i = 0; i < lines.size(); ++i) {
        if (!lines.at(i).trimmed().startsWith(QStringLiteral("> **Чернова")))
            continue;
        lines.removeAt(i);
        if (i > 0 && i < lines.size() && lines.at(i - 1).trimmed().isEmpty()
            && lines.at(i).trimmed().isEmpty()) {
            lines.removeAt(i);
        }
        break;
    }

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly))
        return failWith(QStringLiteral("файлът не може да се запише: %1").arg(filePath));
    // The status changed, so the requirement records WHY as well: the history
    // row is what `scripts/check-req-history.sh` demands of the commit that
    // carries this approval (REQ-SW-PL-101), and writing it here is the only
    // place that knows both values.
    QString updated = lines.join(QLatin1Char('\n'));
    updated = contentWithHistoryRow(updated, date.trimmed(),
                                    QStringLiteral("одобрено (DRAFT → ACTIVE)"),
                                    QStringLiteral("потребител"),
                                    QStringLiteral("присъда на потребителя"));
    out.write(updated.toUtf8());
    if (!out.commit())
        return failWith(QStringLiteral("записът не завърши: %1").arg(filePath));
    return true;
}

QString RequirementsParser::draftReviewHeading()
{
    return QStringLiteral("## Ревю на черновата");
}

QString RequirementsParser::changesRequestedVerdict()
{
    return QStringLiteral("за корекция");
}

QString RequirementsParser::rejectVerdict()
{
    return QStringLiteral("откажи");
}

int RequirementsParser::draftReviewRounds(const QString &content)
{
    return tableDataRows(content, draftReviewHeading()).size();
}

QString RequirementsParser::lastDraftVerdict(const QString &content)
{
    const QStringList rows = tableDataRows(content, draftReviewHeading());
    if (rows.isEmpty())
        return QString();
    // "| N | date | verdict | note |" → cells[3] is the verdict.
    const QStringList cells = rows.last().split(QLatin1Char('|'));
    return cells.value(3).trimmed();
}

bool RequirementsParser::requestChangesDraft(const QString &filePath, const QString &note,
                                             const QString &date, QString *errorOut)
{
    const auto failWith = [errorOut](const QString &reason) {
        if (errorOut)
            *errorOut = reason;
        return false;
    };

    if (note.trimmed().isEmpty())
        return failWith(QStringLiteral("бележката е празна — „за корекция“ иска какво липсва"));
    if (date.trimmed().isEmpty())
        return failWith(QStringLiteral("датата на присъдата е празна"));

    QString content;
    if (!readContentBytes(filePath, &content, errorOut))
        return false;

    // Only a draft has a review; anything else is a successful no-op, the same
    // contract approveDraft() follows.
    if (statusTokenInContent(content) != QStringLiteral("DRAFT"))
        return true;

    const QStringList lines = content.split(QLatin1Char('\n'));
    const QString eol = lines.first().endsWith(QLatin1Char('\r'))
                            ? QStringLiteral("\r") : QString();
    const int round = draftReviewRounds(content) + 1;
    const QString row = QStringLiteral("| %1 | %2 | %3 | %4 |")
                            .arg(QString::number(round), date.trimmed(),
                                 changesRequestedVerdict(), note.trimmed());
    // Status stays DRAFT and nothing else in the file is touched: the row is
    // the whole record of the round (REQ-SW-PL-102 AC1).
    const QString updated = appendTableRow(content, draftReviewHeading(),
                                           QStringLiteral("| кръг | дата | присъда | бележка |"),
                                           QStringLiteral("|---|---|---|---|"), row, eol);
    return writeContentBytes(filePath, updated, errorOut);
}

bool RequirementsParser::rejectDraft(const QString &filePath, const QString &reason,
                                     const QString &date, QString *errorOut)
{
    const auto failWith = [errorOut](const QString &reason) {
        if (errorOut)
            *errorOut = reason;
        return false;
    };

    if (reason.trimmed().isEmpty())
        return failWith(QStringLiteral("причината е празна — отказът иска защо"));

    QString content;
    if (!readContentBytes(filePath, &content, errorOut))
        return false;
    if (statusTokenInContent(content) != QStringLiteral("DRAFT"))
        return true;

    const QStringList lines = content.split(QLatin1Char('\n'));
    const QString eol = lines.first().endsWith(QLatin1Char('\r'))
                            ? QStringLiteral("\r") : QString();
    const int round = draftReviewRounds(content) + 1;

    QString updated = contentWithStatus(content, QStringLiteral("CANCELLED"), eol);
    updated = appendTableRow(updated, draftReviewHeading(),
                             QStringLiteral("| кръг | дата | присъда | бележка |"),
                             QStringLiteral("|---|---|---|---|"),
                             QStringLiteral("| %1 | %2 | %3 | %4 |")
                                 .arg(QString::number(round), date.trimmed(),
                                      rejectVerdict(), reason.trimmed()),
                             eol);
    // The status changed, so the history row is not optional: the whole point
    // of the guard is that a later reader sees WHY (REQ-SW-PL-101).
    updated = contentWithHistoryRow(updated, date,
                                    QStringLiteral("CANCELLED — черновата е отказана"),
                                    QStringLiteral("потребител"), reason.trimmed());

    if (!writeContentBytes(filePath, updated, errorOut))
        return false;

    // Same zone rule as "Mark Done & Archive": a CANCELLED file left in active/
    // is an error for scripts/validate-traceability.sh.
    const QFileInfo info(filePath);
    const QString cleanPath =
        QDir::toNativeSeparators(QDir::cleanPath(info.absolutePath()));
    QStringList dirParts = cleanPath.split(QDir::separator());
    const int activeIndex = dirParts.lastIndexOf(QStringLiteral("active"));
    if (activeIndex < 0)
        return failWith(QStringLiteral("файлът не е в active/: %1").arg(filePath));
    dirParts[activeIndex] = QStringLiteral("archive");
    const QString archiveDir = dirParts.join(QDir::separator());
    QDir().mkpath(archiveDir);
    return renameStrict(filePath, QDir(archiveDir).filePath(info.fileName()), errorOut);
}

bool RequirementsParser::startContractChange(const QString &filePath, const QString &reason,
                                             const QString &who, const QString &date,
                                             QString *errorOut)
{
    const auto failWith = [errorOut](const QString &reason) {
        if (errorOut)
            *errorOut = reason;
        return false;
    };

    if (reason.trimmed().isEmpty())
        return failWith(QStringLiteral("причината е празна — промяната в договора иска защо"));

    QString content;
    if (!readContentBytes(filePath, &content, errorOut))
        return false;

    const QString token = statusTokenInContent(content);
    // A contract exists only once it was accepted (REQ-SW-PL-101 → „Промени
    // договора“ is enabled for ACTIVE/IMPLEMENTED).
    if (token != QStringLiteral("ACTIVE") && token != QStringLiteral("IMPLEMENTED"))
        return true;

    const QStringList lines = content.split(QLatin1Char('\n'));
    const QString eol = lines.first().endsWith(QLatin1Char('\r'))
                            ? QStringLiteral("\r") : QString();
    QString updated = contentWithStatus(content, QStringLiteral("DRAFT"), eol);
    updated = contentWithHistoryRow(updated, date,
                                    QStringLiteral("промяна в договора — обратно в DRAFT"),
                                    who.trimmed().isEmpty() ? QStringLiteral("потребител")
                                                            : who.trimmed(),
                                    reason.trimmed());
    return writeContentBytes(filePath, updated, errorOut);
}

void RequirementsParser::appendHistoryRow(Requirement &req, const QString &date,
                                          const QString &change, const QString &who,
                                          const QString &why)
{
    req.rawContent = contentWithHistoryRow(req.rawContent, date, change, who, why);
    req.revision = revisionInContent(req.rawContent);
}

int RequirementsParser::revisionInContent(const QString &content)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
        if (!trimmed.startsWith(QStringLiteral("- **Ревизия:**")))
            continue;
        QString value = trimmed;
        value.remove(0, value.indexOf(QLatin1Char(':')) + 1);
        value.remove(QLatin1Char('*')).remove(QLatin1Char('`'));
        return value.simplified().toInt();
    }
    return 0;
}

QString RequirementsParser::findFileById(const QVector<RequirementRoot> &roots,
                                         const QString &id)
{
    if (id.trimmed().isEmpty())
        return QString();
    // A FRESH walk, not a lookup in a cached vector: the point is that the H1
    // is the identity and the path is not (REQ-SW-PL-102 AC7).
    for (const Requirement &req : parseDirectories(roots)) {
        if (QString::compare(req.id, id, Qt::CaseInsensitive) == 0)
            return req.filePath;
    }
    return QString();
}

QString RequirementsParser::generateNextId(const QString &baseDir, const QString &prefix)
{
    const QString prefixStr = QStringLiteral("REQ-%1-").arg(prefix);
    const QString padded = QStringLiteral("%1").arg(0, 3, 10, QLatin1Char('0'));
    int maxNumber = padded.toInt();

    const QVector<Requirement> requirements = parseDirectory(baseDir);
    for (const Requirement &req : requirements) {
        if (!req.id.startsWith(prefixStr))
            continue;
        const QString numberStr = req.id.mid(prefixStr.length());
        // Exact prefix-family match: the remainder after "REQ-<PREFIX>-" must
        // be a pure number. This keeps typed families isolated — e.g. a query
        // for "SW-PL" counts "REQ-SW-PL-001" but never "REQ-SW-FW-001", and a
        // query for "SW" never counts "REQ-SW-PL-001".
        if (numberStr.isEmpty() || !numberStrIsDigits(numberStr))
            continue;
        bool ok = false;
        const int number = numberStr.toInt(&ok);
        if (ok && number > maxNumber)
            maxNumber = number;
    }
    return QStringLiteral("REQ-%1-%2")
        .arg(prefix, QStringLiteral("%1").arg(maxNumber + 1, 3, 10, QLatin1Char('0')));
}

QString RequirementsParser::activeDirectory(const QString &baseDir)
{
    const QString reqDir =
        QDir(baseDir).absoluteFilePath(QString::fromUtf8(kRequirementsSubdir));
    if (QDir(reqDir).exists())
        return QDir(reqDir).filePath(QStringLiteral("active"));
    return QDir(baseDir).filePath(QStringLiteral("active"));
}

QString RequirementsParser::archiveDirectory(const QString &baseDir)
{
    const QString reqDir =
        QDir(baseDir).absoluteFilePath(QString::fromUtf8(kRequirementsSubdir));
    if (QDir(reqDir).exists())
        return QDir(reqDir).filePath(QStringLiteral("archive"));
    return QDir(baseDir).filePath(QStringLiteral("archive"));
}

bool RequirementsParser::moveToArchive(const QString &filePath)
{
    const QFileInfo info(filePath);
    // Normalize path to native separators before splitting to handle
    // cross-platform inconsistencies (e.g., forward slashes on Windows).
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(info.absolutePath()));
    const QStringList dirParts = cleanPath.split(QDir::separator());
    const int activeIndex = dirParts.lastIndexOf(QStringLiteral("active"));
    if (activeIndex < 0)
        return false; // file is not inside an active/ directory

    // Replace the "active" path segment with "archive", preserving any nested
    // subdirectory: <...>/active/framework/ -> <...>/archive/framework/.
    QStringList targetParts = dirParts;
    targetParts[activeIndex] = QStringLiteral("archive");
    const QString archiveDir = targetParts.join(QDir::separator());
    QDir().mkpath(archiveDir);
    const QString target = QDir(archiveDir).filePath(info.fileName());
    if (target == filePath)
        return false;
    if (QFile::exists(target) && !QFile::remove(target))
        return false;
    return QFile::rename(filePath, target);
}

bool RequirementsParser::moveToActive(const QString &filePath)
{
    const QFileInfo info(filePath);
    // Normalize path to native separators before splitting to handle
    // cross-platform inconsistencies (e.g., forward slashes on Windows).
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(info.absolutePath()));
    const QStringList dirParts = cleanPath.split(QDir::separator());
    const int archiveIndex = dirParts.lastIndexOf(QStringLiteral("archive"));
    if (archiveIndex < 0)
        return false; // file is not inside an archive/ directory

    // Replace the "archive" path segment with "active", preserving any nested
    // subdirectory: <...>/archive/framework/ -> <...>/active/framework/.
    QStringList targetParts = dirParts;
    targetParts[archiveIndex] = QStringLiteral("active");
    const QString activeDir = targetParts.join(QDir::separator());
    QDir().mkpath(activeDir);
    const QString target = QDir(activeDir).filePath(info.fileName());
    if (target == filePath)
        return false;
    if (QFile::exists(target) && !QFile::remove(target))
        return false;
    return QFile::rename(filePath, target);
}

bool RequirementsParser::parseFile(const QFileInfo &fileInfo, const QString &section,
                                   Requirement &out)
{
    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    out.rawContent = in.readAll();
    file.close();

    out.filePath = fileInfo.absoluteFilePath();
    out.fileName = fileInfo.fileName();
    out.section = section;

    const QStringList lines = out.rawContent.split(QStringLiteral("\n"));

    // Title: "# REQ-FW-001: Some Title" (3-segment) or
    //        "# REQ-SW-PL-001: Some Title" (typed, 4-segment)
    // Capture groups: 1 = full ID, 2 = optional type segment, 3 = title.
    const QRegularExpression titleRe(
        QStringLiteral("^#\\s+(REQ-[A-Z]+(-[A-Z]+)?-\\d{3})\\s*:\\s*(.*)$"));
    // Metadata: "- **Статус:** ACTIVE" (bold wraps "Ключ:" together)
    const QRegularExpression metaRe(QStringLiteral("^\\s*-\\s*\\*\\*([^*]+):\\*\\*\\s*(.*)$"));
    // Acceptance criterion: "- [ ] text" / "- [x] text"
    const QRegularExpression criterionRe(QStringLiteral("^\\s*-\\s*\\[([ xX])\\]\\s*(.*)$"));

    bool inDescription = false;
    bool inCriteria = false;
    bool inTraceability = false;
    QStringList descriptionLines;
    QStringList traceabilityLines;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("## "))) {
            const QString heading = trimmed.mid(3).trimmed();
            const QString version = descriptionVersion(heading);
            inDescription = !version.isEmpty();
            if (inDescription && out.templateVersion.isEmpty()) {
                // First description heading wins: a file that starts its
                // description with a legacy heading IS that generation, even
                // when a canonical heading follows later in the file.
                out.templateVersion = version;
                out.templateHeading = heading;
            }
            inCriteria = isCriteriaHeading(heading);
            inTraceability = (trimmed == QStringLiteral("## Проследимост"));
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("### "))) {
            if (inCriteria)
                appendCriterionHeading(line, out);
            continue;
        }

        const QRegularExpressionMatch titleMatch = titleRe.match(line);
        if (titleMatch.hasMatch()) {
            out.id = titleMatch.captured(1);
            out.title = titleMatch.captured(3).trimmed(); // group 2 = optional type
            continue;
        }

        const QRegularExpressionMatch metaMatch = metaRe.match(line);
        if (metaMatch.hasMatch()) {
            const QString key = metaMatch.captured(1).trimmed();
            const QString value = metaMatch.captured(2).trimmed();
            bool handled = true;
            if (key == QStringLiteral("Статус"))
                out.status = value;
            else if (key == QStringLiteral("Приоритет"))
                out.priority = value;
            else if (key.contains(QStringLiteral("Отговорник")))
                out.assignee = value;
            else if (key == QStringLiteral("Дата"))
                out.date = value;
            else if (key == QStringLiteral("Създадено"))
                out.created = value;
            else if (key == QStringLiteral("Ревизия"))
                out.revision = value.toInt();
            else if (key == QStringLiteral("Родител")) {
                // A parent list is possible ("REQ-AI-011, REQ-AI-012"); the tree
                // is single-parent, so parentId is the first requirement ID —
                // all of them stay in parentIds and are validated.
                const QString first = appendReferences(value, out.parentIds,
                                                       out.dependencyHints);
                out.parentId = first.isEmpty() ? out.parentIds.value(0) : first;
            }
            else if (key == QStringLiteral("Зависи от")) {
                appendReferences(value, out.dependencies, out.dependencyHints);
            }
            else if (key == QStringLiteral("Деца")) {
                // Inverse of "Родител:" — parsed and validated so the child
                // links of a requirement cannot rot unobserved.
                appendReferences(value, out.childIds, out.dependencyHints);
            } else {
                handled = false; // unknown metadata (e.g. under Проследимост)
                // Traceability keys are captured into structured fields BUT
                // deliberately left with handled == false: their lines must
                // also stay part of the raw traceability text below.
                if (key == QStringLiteral("Коммити"))
                    out.commits = value;
                else if (key == QStringLiteral("Код"))
                    out.code = value;
                else if (key == QStringLiteral("Тестове"))
                    out.tests = value;
                else if (key == QStringLiteral("Документация"))
                    out.docs = value;
            }
            if (handled)
                continue;
        }

        const QRegularExpressionMatch criterionMatch = criterionRe.match(line);
        if (criterionMatch.hasMatch()) {
            out.acceptanceCriteria.append(criterionMatch.captured(2).trimmed());
            out.criteriaDone.append(criterionMatch.captured(1).toLower() == QStringLiteral("x"));
            continue;
        }

        // Numbered entries and table rows only count inside the criteria
        // section; the same shapes elsewhere are prose.
        if (inCriteria && appendCriterionLine(line, out))
            continue;

        if (inDescription)
            descriptionLines.append(line);
        else if (inTraceability)
            traceabilityLines.append(line);
    }

    out.description = descriptionLines.join(QStringLiteral("\n")).trimmed();
    out.traceability = traceabilityLines.join(QStringLiteral("\n")).trimmed();
    // REQ-SW-PL-101: "Създадено:" is the field, "Дата:" the legacy alias. The
    // alias lives HERE, in the parser, not in a migration: the 135 files that
    // predate the field keep their bytes and still answer "when was this
    // created".
    if (out.created.trimmed().isEmpty())
        out.created = out.date;
    // A parsed file always has a generation: "unknown" is the honest answer for
    // one that follows no template at all, and it stays an Error downstream.
    if (out.templateVersion.isEmpty())
        out.templateVersion = QStringLiteral("unknown");
    return !out.id.isEmpty();
}

PhaseStatus phaseStatus(const Requirement &req)
{
    PhaseStatus result;
    result.architecture = !req.docs.trimmed().isEmpty()
                          && req.docs.trimmed() != QStringLiteral("—");
    result.implementation = !req.code.trimmed().isEmpty()
                            && req.code.trimmed() != QStringLiteral("—");
    result.testing = !req.tests.trimmed().isEmpty()
                     && req.tests.trimmed() != QStringLiteral("—");
    return result;
}

QString createdDate(const Requirement &req)
{
    const QString created = req.created.trimmed();
    if (!created.isEmpty())
        return created;
    return req.date.trimmed();
}

} // namespace Daqster

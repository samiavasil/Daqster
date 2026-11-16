#include "IdeaReader.h"
#include "LogCategories.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <algorithm>

namespace Daqster {
namespace {

//!< How far into the file the `>` header block is looked for.
constexpr int kHeaderScanLines = 15;

QString stripDecoration(QString text)
{
    text = text.trimmed();
    text.remove(QLatin1Char('*'));
    text.remove(QLatin1Char('`'));
    return text.trimmed();
}

//!< Reads the whole file as UTF-8; empty string on failure.
QString readAll(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *ok = false;
        return QString();
    }
    const QString content = QString::fromUtf8(file.readAll());
    *ok = true;
    return content;
}

QStringList fileLines(const QString &content)
{
    QStringList lines = content.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
    }
    return lines;
}

//!< First `# ` heading line, or empty when the file has none.
QString titleLine(const QStringList &lines)
{
    static const QRegularExpression heading(QStringLiteral("^#\\s+(.+)$"));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = heading.match(line);
        if (match.hasMatch())
            return match.captured(1).trimmed();
    }
    return QString();
}

QString idFrom(const QString &text)
{
    static const QRegularExpression ideaId(
        QStringLiteral("(IDEA-\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = ideaId.match(text);
    return match.hasMatch() ? match.captured(1).toUpper() : QString();
}

//!< Splits the `>` header block into "key: value" fields.
void readHeaderFields(const QStringList &lines, const IdeaHeaderKeys &keys, Idea &out)
{
    static const QRegularExpression separators(QStringLiteral("[·|]"));
    const int limit = qMin(lines.size(), kHeaderScanLines);
    for (int i = 0; i < limit; ++i) {
        QString line = lines.at(i).trimmed();
        if (!line.startsWith(QLatin1Char('>')))
            continue;
        while (line.startsWith(QLatin1Char('>')))
            line.remove(0, 1);
        const QStringList fields = line.split(separators);
        for (const QString &field : fields) {
            const int colon = field.indexOf(QLatin1Char(':'));
            if (colon <= 0)
                continue;
            const QString key = stripDecoration(field.left(colon));
            const QString value = stripDecoration(field.mid(colon + 1));
            if (key.compare(keys.status, Qt::CaseInsensitive) == 0 && out.status.isEmpty())
                out.status = value;
            else if (key.compare(keys.added, Qt::CaseInsensitive) == 0 && out.added.isEmpty())
                out.added = value;
            else if (key.compare(keys.nextReview, Qt::CaseInsensitive) == 0
                     && out.nextReview.isEmpty())
                out.nextReview = value;
        }
    }
}

//!< Strips a leading "IDEA-NNN — " / "IDEA-NNN: " prefix from the title.
QString titleWithoutId(const QString &title)
{
    static const QRegularExpression prefix(
        QStringLiteral("^IDEA-\\d+\\s*(?:[—–:-]+)?\\s*"));
    QString cleaned = title;
    cleaned.remove(prefix);
    return cleaned.trimmed();
}

} // namespace

QStringList IdeaReader::candidateDirs(const QStringList &roots)
{
    QStringList dirs;
    const QStringList suffixes = {QStringLiteral("DevelopmentProcess/plans/ideas"),
                                  QStringLiteral("DevelopmentProcess/ideas")};
    for (const QString &root : roots) {
        const QString trimmed = root.trimmed();
        if (trimmed.isEmpty())
            continue;
        QStringList candidates;
        // Tolerate being handed the ideas directory itself (tests, callers).
        if (QFileInfo(trimmed).fileName() == QStringLiteral("ideas"))
            candidates.append(trimmed);
        for (const QString &suffix : suffixes)
            candidates.append(QDir(trimmed).filePath(suffix));

        for (const QString &candidate : candidates) {
            const QFileInfo info(candidate);
            if (!info.isDir())
                continue;
            const QString canonical = info.canonicalFilePath();
            if (!canonical.isEmpty() && !dirs.contains(canonical))
                dirs.append(canonical);
        }
    }
    return dirs;
}

Idea IdeaReader::parseFile(const QString &filePath, const IdeaHeaderKeys &keys, bool *ok)
{
    if (ok)
        *ok = false;

    bool readable = false;
    const QString content = readAll(filePath, &readable);
    if (!readable) {
        qCWarning(lcFramework) << "IdeaReader: cannot read idea file" << filePath;
        return Idea();
    }

    const QStringList lines = fileLines(content);
    Idea idea;
    idea.path = QFileInfo(filePath).absoluteFilePath();
    idea.rawContent = content;

    const QString heading = titleLine(lines);
    const QString fileId = idFrom(QFileInfo(filePath).fileName());
    const QString headerId = idFrom(heading);

    idea.id = fileId.isEmpty() ? headerId : fileId;
    if (idea.id.isEmpty())
        idea.id = idFrom(content.left(2048)); // header block, not the body

    if (heading.isEmpty() || idea.id.isEmpty()) {
        // A file that advertises itself as an idea (IDEA-… in the name) but has
        // no title is a defect worth one warning; the register's README and
        // templates are not ideas at all and stay quiet at debug level.
        if (!fileId.isEmpty())
            qCWarning(lcFramework) << "IdeaReader: skipping malformed idea file"
                                   << filePath << "(no '# ' title or no IDEA id)";
        else
            qCDebug(lcFramework) << "IdeaReader: skipping non-idea markdown"
                                 << filePath;
        return Idea();
    }

    idea.title = titleWithoutId(heading);
    readHeaderFields(lines, keys, idea);
    idea.attachmentCount = 0;

    const QString attachments = attachmentsDir(idea.path, idea.id);
    if (!attachments.isEmpty()) {
        QDirIterator it(attachments, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++idea.attachmentCount;
        }
    }

    if (ok)
        *ok = true;
    return idea;
}

QVector<Idea> IdeaReader::readDirectory(const QString &ideasDir, const IdeaHeaderKeys &keys)
{
    QVector<Idea> ideas;
    QDir dir(ideasDir);
    if (ideasDir.trimmed().isEmpty() || !dir.exists())
        return ideas;

    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.md")}, QDir::Files,
                                                 QDir::Name);
    for (const QFileInfo &info : files) {
        bool ok = false;
        const Idea idea = parseFile(info.absoluteFilePath(), keys, &ok);
        if (ok)
            ideas.append(idea);
    }
    return ideas;
}

QVector<Idea> IdeaReader::readFromRoots(const QStringList &roots, const IdeaHeaderKeys &keys)
{
    QVector<Idea> ideas;
    QSet<QString> seen;
    for (const QString &dir : candidateDirs(roots)) {
        for (const Idea &idea : readDirectory(dir, keys)) {
            if (seen.contains(idea.path))
                continue;
            seen.insert(idea.path);
            ideas.append(idea);
        }
    }
    std::sort(ideas.begin(), ideas.end(), [](const Idea &left, const Idea &right) {
        return QString::compare(left.id, right.id, Qt::CaseInsensitive) < 0;
    });
    return ideas;
}

QString IdeaReader::attachmentsDir(const QString &ideaPath, const QString &ideaId)
{
    if (ideaPath.trimmed().isEmpty() || ideaId.trimmed().isEmpty())
        return QString();
    const QDir parent = QFileInfo(ideaPath).dir();
    const QString candidate = parent.filePath(
        ideaId + QStringLiteral("/attachments"));
    return QFileInfo(candidate).isDir() ? candidate : QString();
}

bool IdeaReader::appendPromotionLine(const QString &ideaPath, const QString &requirementId)
{
    if (ideaPath.trimmed().isEmpty() || requirementId.trimmed().isEmpty())
        return false;

    bool readable = false;
    const QString content = readAll(ideaPath, &readable);
    if (!readable)
        return false;

    const QString eol = content.contains(QStringLiteral("\r\n"))
                            ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = fileLines(content);
    const QString promotionLine =
        QStringLiteral("> **Промотирана:** %1").arg(requirementId.trimmed());

    static const QRegularExpression existing(
        QStringLiteral("^\\s*>\\s*\\**\\s*Промотирана\\s*\\**\\s*:"));
    int existingLine = -1;
    int firstHeaderLine = -1;
    const int limit = qMin(lines.size(), kHeaderScanLines * 2);
    for (int i = 0; i < limit; ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (firstHeaderLine < 0 && trimmed.startsWith(QLatin1Char('>')))
            firstHeaderLine = i;
        if (existingLine < 0 && existing.match(trimmed).hasMatch())
            existingLine = i;
    }
    // The header block is the contiguous run of `>` lines starting at the first one.
    int lastHeaderLine = firstHeaderLine;
    if (firstHeaderLine >= 0) {
        while (lastHeaderLine + 1 < lines.size()
               && lines.at(lastHeaderLine + 1).trimmed().startsWith(QLatin1Char('>'))) {
            ++lastHeaderLine;
        }
    }

    if (existingLine >= 0) {
        lines[existingLine] = promotionLine;
    } else if (firstHeaderLine >= 0) {
        lines.insert(lastHeaderLine + 1, promotionLine);
    } else {
        // No header block at all: keep the line directly under the title.
        int insertAt = 0;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).startsWith(QLatin1Char('#'))) {
                insertAt = i + 1;
                break;
            }
        }
        lines.insert(insertAt, promotionLine);
    }

    QSaveFile file(ideaPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(lines.join(eol).toUtf8());
    return file.commit();
}

} // namespace Daqster

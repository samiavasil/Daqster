#include "RequirementsParser.h"

#include <QDir>
#include <QRegularExpression>
#include <QTextStream>
#include <QFile>

#include "LogCategories.h"

namespace Daqster {

namespace {

QString sectionOf(const QString &baseDir, const QString &absolutePath)
{
    const QString dir = QDir(baseDir).absolutePath() + QDir::separator();
    if (absolutePath.startsWith(dir + QStringLiteral("active")))
        return QStringLiteral("active");
    if (absolutePath.startsWith(dir + QStringLiteral("archive")))
        return QStringLiteral("archive");
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

} // namespace

QVector<Requirement> RequirementsParser::parseDirectory(const QString &baseDir)
{
    QVector<Requirement> result;
    QVector<QFileInfo> files;
    collectMarkdownFiles(baseDir, files);

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

    // Title: "# REQ-FW-001: Some Title"
    const QRegularExpression titleRe(QStringLiteral("^#\\s+(REQ-[A-Z]+-\\d+)\\s*:\\s*(.*)$"));
    // Metadata: "- **Статус:** ACTIVE"
    const QRegularExpression metaRe(QStringLiteral("^\\s*-\\s*\\*\\*([^*]+)\\*\\*\\s*:\\s*(.*)$"));

    bool inDescription = false;
    QStringList descriptionLines;

    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("## "))) {
            inDescription = false;
            continue;
        }
        if (line.trimmed().startsWith(QStringLiteral("### ")))
            continue;

        const QRegularExpressionMatch titleMatch = titleRe.match(line);
        if (titleMatch.hasMatch()) {
            out.id = titleMatch.captured(1);
            out.title = titleMatch.captured(2).trimmed();
            continue;
        }

        const QRegularExpressionMatch metaMatch = metaRe.match(line);
        if (metaMatch.hasMatch()) {
            const QString key = metaMatch.captured(1).trimmed();
            const QString value = metaMatch.captured(2).trimmed();
            if (key == QStringLiteral("Статус"))
                out.status = value;
            else if (key == QStringLiteral("Приоритет"))
                out.priority = value;
            else if (key.contains(QStringLiteral("Отговорник")))
                out.assignee = value;
            continue;
        }

        const QRegularExpressionMatch criterionMatch =
            QRegularExpression(QStringLiteral("^\\s*-\\s*\\[([ xX])\\]\\s*(.*)$")).match(line);
        if (criterionMatch.hasMatch()) {
            out.acceptanceCriteria.append(criterionMatch.captured(2).trimmed());
            out.criteriaDone.append(criterionMatch.captured(1).toLower() == QStringLiteral("x"));
            continue;
        }

        if (line.trimmed() == QStringLiteral("## Описание"))
            inDescription = true;
        else if (inDescription)
            descriptionLines.append(line);
    }

    out.description = descriptionLines.join(QStringLiteral("\n")).trimmed();
    return !out.id.isEmpty();
}

} // namespace Daqster

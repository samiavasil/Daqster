#include "JackDetectEngine.h"

#include <QDir>
#include <QFile>
#include <QTimer>

#include <algorithm>

JackDetectEngine::JackDetectEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &JackDetectEngine::poll);
}

JackDetectEngine::~JackDetectEngine()
{
    stop();
}

void JackDetectEngine::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qBound(100, ms, 5000);
    if (m_timer->isActive())
        m_timer->start(m_pollIntervalMs);
}

void JackDetectEngine::start()
{
    poll(); // initial read + emit (event-driven first state)
    m_timer->start(m_pollIntervalMs);
    emit statusChanged(QStringLiteral("Polling every %1 ms").arg(m_pollIntervalMs));
}

void JackDetectEngine::stop()
{
    m_timer->stop();
}

void JackDetectEngine::poll()
{
    const QVector<JackState> current = readJacks();
    if (current == m_lastJacks)
        return;
    m_lastJacks = current;
    emit jacksChanged(current);
}

QVector<JackDetectEngine::JackState> JackDetectEngine::readJacks() const
{
    QVector<JackState> jacks;

    const QDir asoundDir(QStringLiteral("/proc/asound"));
    const QStringList cardDirs =
        asoundDir.entryList({QStringLiteral("card*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &card : cardDirs) {
        const QDir cardDir(asoundDir.filePath(card));
        const QStringList codecEntries = cardDir.entryList(
            {QStringLiteral("codec#*")}, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QString &codec : codecEntries) {
            const QDir codecDir(cardDir.filePath(codec));
            const QStringList jackFiles = codecDir.entryList(
                {QStringLiteral("jack*")}, QDir::Files | QDir::NoDotAndDotDot);
            for (const QString &jackFile : jackFiles) {
                QFile file(codecDir.filePath(jackFile));
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;
                const QString content = QString::fromUtf8(file.readAll());
                file.close();

                // Parse lines like "Pin 0x21 (Headphone): present = No".
                const QStringList lines =
                    content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    const int openParen = line.indexOf(QLatin1Char('('));
                    const int closeParen = line.indexOf(QLatin1Char(')'), openParen + 1);
                    if (openParen < 0 || closeParen < 0)
                        continue;
                    const QString name =
                        line.mid(openParen + 1, closeParen - openParen - 1).trimmed();
                    if (name.isEmpty())
                        continue;

                    const int presentIdx = line.indexOf(QStringLiteral("present ="));
                    if (presentIdx < 0)
                        continue;
                    const QString state = line.mid(presentIdx + 9).trimmed();
                    const bool present =
                        state.compare(QStringLiteral("Yes"), Qt::CaseInsensitive) == 0;
                    jacks.append({name, present});
                }
            }
        }
    }

    // Deterministic order so poll() change detection is stable across reads.
    std::sort(jacks.begin(), jacks.end(), [](const JackState &a, const JackState &b) {
        return a.name < b.name;
    });
    return jacks;
}
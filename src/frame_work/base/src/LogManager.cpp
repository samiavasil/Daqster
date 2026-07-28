#include "include/LogManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Daqster {

// ── Global state for message handler ────────────────────
static QFile *s_logFile = nullptr;
static bool s_consoleEnabled = false;  // Legacy compatibility, replaced by logLevel threshold
static QString s_instanceId;
static QMutex s_logMutex;
LogManager *LogManager::s_instance = nullptr;

// File-scope alias for use in the free message handler
static LogManager *s_logManagerInstance = nullptr;

// ── Custom message handler ──────────────────────────────
static void daqsterMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    QMutexLocker locker(&s_logMutex);

    // ── Early exit: skip if no consumer exists ────────────
    // Always allow Fatal (will abort below)
    bool hasConsumer = (type == QtFatalMsg)
        || (s_logFile && s_logFile->isOpen())
        || s_consoleEnabled
        || s_logManagerInstance;  // DebugConsoleWidget might be connected

    if (!hasConsumer) {
        return;  // No formatting, no allocation, no waste
    }

    // ── Build formatted line ──────────────────────────────
    // Format: [YYYY-MM-DD HH:mm:ss.zzz] [LEVEL] [source] message
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

#ifdef Q_OS_WIN
    qint64 pid = GetCurrentProcessId();
#else
    qint64 pid = getpid();
#endif

    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DBG"; break;
    case QtInfoMsg:     level = "INF"; break;
    case QtWarningMsg:  level = "WRN"; break;
    case QtCriticalMsg: level = "CRT"; break;
    case QtFatalMsg:    level = "FTL"; break;
    }

    QString source = QString("PID:%1").arg(pid);
    if (!s_instanceId.isEmpty()) {
        source += ":" + s_instanceId;
    }

    QString line = QString("[%1] [%2] [%3] %4")
        .arg(timestamp)
        .arg(level)
        .arg(source)
        .arg(msg);

    // ── File output ───────────────────────────────────────
    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->write(line.toUtf8() + "\n");
        s_logFile->flush();
    }

    // ── Console output ────────────────────────────────────
    // Only when console is explicitly enabled AND message meets level threshold
    if (s_consoleEnabled) {
        LogLevel msgLevel = LogManager::qtMsgTypeToLevel(type);
        LogLevel threshold = s_logManagerInstance ? s_logManagerInstance->consoleLogLevel() : LogLevel::Warning;
        if (msgLevel >= threshold) {
            FILE *stream = (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) ? stderr : stdout;
            fprintf(stream, "%s\n", line.toUtf8().constData());
            fflush(stream);
        }
    } else if (!s_logManagerInstance && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)) {
        // Fallback: if LogManager is destroyed, still show warnings+ to stderr
        fprintf(stderr, "%s\n", line.toUtf8().constData());
        fflush(stderr);
    }

    // ── Widget signal ─────────────────────────────────────
    if (s_logManagerInstance) {
        emit s_logManagerInstance->messageLogged(line, static_cast<int>(type));
    }

    // ── Fatal: abort ──────────────────────────────────────
    if (type == QtFatalMsg) {
        abort();
    }
}

// ── LogManager implementation ───────────────────────────

LogManager *LogManager::instance()
{
    if (!s_instance) {
        s_instance = new LogManager();
        s_logManagerInstance = s_instance;
    }
    return s_instance;
}

struct LogManager::Private {
    QStringList enabledCategories;
    QString logFilePath;
    LogLevel logLevel = LogLevel::Warning;  // Default: WRN and above
};

LogManager::LogManager(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

LogManager::~LogManager()
{
    shutdown();
    delete d;
}

void LogManager::initialize()
{
    qInstallMessageHandler(daqsterMessageHandler);

    // All categories OFF by default
    // User enables via DebugConsoleWidget or programmatically
}

void LogManager::shutdown()
{
    if (s_logFile) {
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }
    s_consoleEnabled = false;
    // Keep message handler active for destructor messages
    s_logManagerInstance = nullptr;
}

// ── Category control ────────────────────────────────────

void LogManager::enableCategory(const QString &category)
{
    if (!d->enabledCategories.contains(category)) {
        d->enabledCategories.append(category);
    }
    // Apply to Qt logging system
    QStringList rules;
    for (const QString &cat : d->enabledCategories) {
        rules << cat + "=true";
    }
    QLoggingCategory::setFilterRules(rules.join("\n"));
    emit filterRulesChanged(filterRules());
}

void LogManager::disableCategory(const QString &category)
{
    d->enabledCategories.removeAll(category);
    QStringList rules;
    for (const QString &cat : d->enabledCategories) {
        rules << cat + "=true";
    }
    QLoggingCategory::setFilterRules(rules.join("\n"));
    emit filterRulesChanged(filterRules());
}

void LogManager::enableAll()
{
    d->enabledCategories.clear();
    for (const auto &info : Log::allCategories()) {
        d->enabledCategories.append(info.name);
    }
    QLoggingCategory::setFilterRules("*.debug=true");
    emit filterRulesChanged(filterRules());
}

void LogManager::disableAll()
{
    d->enabledCategories.clear();
    QLoggingCategory::setFilterRules(QString());
    emit filterRulesChanged(filterRules());
}

bool LogManager::isCategoryEnabled(const QString &category) const
{
    return d->enabledCategories.contains(category);
}

QString LogManager::filterRules() const
{
    QStringList rules;
    for (const QString &cat : d->enabledCategories) {
        rules << cat + "=true";
    }
    return rules.join("\n");
}

// ── Output targets ──────────────────────────────────────

bool LogManager::setLogFile(const QString &filePath)
{
    // Close existing file
    if (s_logFile) {
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }

    if (filePath.isEmpty()) {
        d->logFilePath.clear();
        emit logFileChanged(QString());
        return true;
    }

    // Ensure directory exists
    QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    s_logFile = new QFile(filePath);
    if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        d->logFilePath = filePath;
        emit logFileChanged(filePath);
        return true;
    } else {
        delete s_logFile;
        s_logFile = nullptr;
        qWarning() << "LogManager: Failed to open log file:" << filePath;
        return false;
    }
}

QString LogManager::logFilePath() const
{
    return d->logFilePath;
}

void LogManager::setConsoleEnabled(bool enabled)
{
    if (s_consoleEnabled != enabled) {
        s_consoleEnabled = enabled;
        emit consoleEnabledChanged(enabled);
    }
}

bool LogManager::isConsoleEnabled() const
{
    return s_consoleEnabled;
}

LogLevel LogManager::consoleLogLevel() const
{
    return d->logLevel;
}

void LogManager::setConsoleLogLevel(LogLevel level)
{
    if (d->logLevel != level) {
        d->logLevel = level;
        emit consoleLogLevelChanged(level);
    }
}

QString LogManager::consoleLogLevelName() const
{
    switch (d->logLevel) {
    case LogLevel::Debug:    return QStringLiteral("Debug");
    case LogLevel::Info:     return QStringLiteral("Info");
    case LogLevel::Warning:  return QStringLiteral("Warning");
    case LogLevel::Critical: return QStringLiteral("Critical");
    case LogLevel::Fatal:    return QStringLiteral("Fatal");
    }
    return QStringLiteral("Warning");
}

LogLevel LogManager::qtMsgTypeToLevel(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return LogLevel::Debug;
    case QtInfoMsg:     return LogLevel::Info;
    case QtWarningMsg:  return LogLevel::Warning;
    case QtCriticalMsg: return LogLevel::Critical;
    case QtFatalMsg:    return LogLevel::Fatal;
    }
    return LogLevel::Warning;
}

// ── Instance identification ─────────────────────────────

void LogManager::setInstanceId(const QString &id)
{
    s_instanceId = id;
}

QString LogManager::instanceId() const
{
    return s_instanceId;
}

} // namespace Daqster

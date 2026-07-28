#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include "LogCategories.h"

namespace Daqster {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Critical = 3,
    Fatal = 4
};

/**
 * @brief Central logging manager for the Daqster framework.
 *
 * Controls:
 * - Which categories are enabled (runtime filtering)
 * - Output targets (console, file, or both)
 * - PID and instance ID tagging
 * - Log file path
 *
 * Debug is OFF by default. User enables via DebugConsoleWidget.
 */
class LogManager : public QObject
{
    Q_OBJECT

public:
    static LogManager *instance();

    // ── Category control ──────────────────────────────
    void enableCategory(const QString &category);
    void disableCategory(const QString &category);
    void enableAll();
    void disableAll();
    bool isCategoryEnabled(const QString &category) const;
    QString filterRules() const;

    // ── Output targets ────────────────────────────────
    bool setLogFile(const QString &filePath);
    QString logFilePath() const;
    Q_PROPERTY(bool consoleEnabled READ isConsoleEnabled WRITE setConsoleEnabled NOTIFY consoleEnabledChanged)
    Q_PROPERTY(LogLevel consoleLogLevel READ consoleLogLevel WRITE setConsoleLogLevel NOTIFY consoleLogLevelChanged)
    LogLevel consoleLogLevel() const;
    void setConsoleLogLevel(LogLevel level);
    QString consoleLogLevelName() const;
    static LogLevel qtMsgTypeToLevel(QtMsgType type);
    void setConsoleEnabled(bool enabled);
    bool isConsoleEnabled() const;

    // ── Instance identification ───────────────────────
    void setInstanceId(const QString &id);
    QString instanceId() const;

    // ── Lifecycle ─────────────────────────────────────
    void initialize();
    void shutdown();

signals:
    void filterRulesChanged(const QString &rules);
    void logFileChanged(const QString &path);
    void messageLogged(const QString &formatted, int msgType);
    void consoleEnabledChanged(bool enabled);
    void consoleLogLevelChanged(LogLevel level);

private:
    LogManager(QObject *parent = nullptr);
    ~LogManager();

    void applyFilterRules();

    static LogManager *s_instance;

    struct Private;
    Private *d;
};

} // namespace Daqster

#endif // LOGMANAGER_H

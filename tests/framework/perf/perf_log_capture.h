#pragma once

#include <QLoggingCategory>
#include <QStringList>
#include <QtMessageHandler>

/// RAII helper that enables the daqster.perf logging category and captures any
/// debug messages emitted while it is alive. Used to observe the aggregates
/// that Daqster::Perf::Domain::flush() writes via qCDebug(lcPerf).
///
/// The message handler and filter rules are always restored on destruction, so
/// an early-returning QVERIFY cannot leak state into subsequent test slots.
class PerfLogCapture
{
public:
    PerfLogCapture()
    {
        m_previous = qInstallMessageHandler(&PerfLogCapture::handler);
        s_active = this;
        QLoggingCategory::setFilterRules(QStringLiteral("daqster.perf.debug=true"));
    }

    ~PerfLogCapture()
    {
        s_active = nullptr;
        qInstallMessageHandler(m_previous);
        QLoggingCategory::setFilterRules(QString());
    }

    PerfLogCapture(const PerfLogCapture &) = delete;
    PerfLogCapture &operator=(const PerfLogCapture &) = delete;

    const QStringList &lines() const { return m_lines; }

    /// True if any captured line contains all of the given substrings.
    bool contains(const QStringList &needles) const
    {
        for (const QString &line : m_lines) {
            bool all = true;
            for (const QString &needle : needles) {
                if (!line.contains(needle)) {
                    all = false;
                    break;
                }
            }
            if (all)
                return true;
        }
        return false;
    }

private:
    static void handler(QtMsgType type, const QMessageLogContext &context,
                        const QString &msg)
    {
        Q_UNUSED(type);
        Q_UNUSED(context);
        if (s_active)
            s_active->m_lines.append(msg);
    }

    inline static PerfLogCapture *s_active = nullptr;
    QStringList m_lines;
    QtMessageHandler m_previous = nullptr;
};

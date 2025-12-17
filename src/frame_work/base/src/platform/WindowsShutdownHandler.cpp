#include "WindowsShutdownHandler.h"
#include <QDebug>
#include <QFile>
#include <iostream>

#ifdef Q_OS_WIN
WindowsShutdownHandler* WindowsShutdownHandler::s_instance = nullptr;
#endif

WindowsShutdownHandler::WindowsShutdownHandler(QObject *parent)
    : ShutdownHandler(parent)
    , m_notifier(nullptr)
{
#ifdef Q_OS_WIN
    s_instance = this;
#endif
}

WindowsShutdownHandler::~WindowsShutdownHandler()
{
    m_thread.quit();
    m_thread.wait();
    
#ifdef Q_OS_WIN
    s_instance = nullptr;
#endif
}

bool WindowsShutdownHandler::initialize()
{
#ifdef Q_OS_WIN
    // Setup Windows console control handler
    if (!SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)) {
        qWarning() << "Failed to set console control handler";
        return false;
    }
    qDebug() << "Windows shutdown handler initialized (Console Ctrl events)";
#else
    qDebug() << "Stdin shutdown handler initialized (quit/exit commands)";
#endif

    // Setup stdin listener for "quit"/"exit" commands (works on both platforms)
    QObject::connect(
        this, &WindowsShutdownHandler::finishedGetLine,
        this, &WindowsShutdownHandler::on_finishedGetLine,
        Qt::QueuedConnection
    );

#ifdef Q_OS_WIN
    m_notifier = new QWinEventNotifier(GetStdHandle(STD_INPUT_HANDLE));
#else
    m_notifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read);
#endif

    m_notifier->setEnabled(true);
    m_notifier->moveToThread(&m_thread);
    
    QObject::connect(
        &m_thread, &QThread::finished,
        m_notifier, &QObject::deleteLater
    );

#ifdef Q_OS_WIN
    QObject::connect(m_notifier, &QWinEventNotifier::activated,
                     [this]() {
                         std::string line;
                         std::getline(std::cin, line);
                         QString strLine = QString::fromStdString(line);
                         Q_EMIT this->finishedGetLine(strLine);
                     });
#else
    QObject::connect(m_notifier, &QSocketNotifier::activated,
                     [this](int) {
                         std::string line;
                         std::getline(std::cin, line);
                         QString strLine = QString::fromStdString(line);
                         Q_EMIT this->finishedGetLine(strLine);
                     });
#endif

    m_thread.start();
    return true;
}

void WindowsShutdownHandler::on_finishedGetLine(const QString &strNewLine)
{
    QString cmd = strNewLine.trimmed();
    
    if (cmd.compare("quit", Qt::CaseInsensitive) == 0 ||
        cmd.compare("exit", Qt::CaseInsensitive) == 0) {
        qDebug() << "Received shutdown command:" << cmd;
        Q_EMIT shutdownRequested();
    }
}

#ifdef Q_OS_WIN
BOOL WINAPI WindowsShutdownHandler::consoleCtrlHandler(DWORD signal)
{
    if (s_instance) {
        QString signalName;
        switch (signal) {
            case CTRL_C_EVENT:
                signalName = "Ctrl+C";
                break;
            case CTRL_BREAK_EVENT:
                signalName = "Ctrl+Break";
                break;
            case CTRL_CLOSE_EVENT:
                signalName = "Console Close";
                break;
            case CTRL_LOGOFF_EVENT:
                signalName = "Logoff";
                break;
            case CTRL_SHUTDOWN_EVENT:
                signalName = "Shutdown";
                break;
            default:
                signalName = QString("Unknown(%1)").arg(signal);
        }
        
        qDebug() << "\nReceived Windows console event:" << signalName;
        QMetaObject::invokeMethod(s_instance, "shutdownRequested", Qt::QueuedConnection);
        return TRUE;
    }
    return FALSE;
}
#endif

#include <iostream>
#include "QConsoleListener.h"
#include<QDebug>
#include<QMessageBox>
#include<QFile>

QConsoleListener::QConsoleListener()
{
    QObject::connect(
        this, &QConsoleListener::finishedGetLine,
        this, &QConsoleListener::on_finishedGetLine,
        Qt::QueuedConnection
        );
#ifdef Q_OS_WIN
    m_notifier = new QWinEventNotifier(GetStdHandle(STD_INPUT_HANDLE));
#else
    m_notifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read);
#endif \
    // NOTE : move to thread because std::getline blocks,
    //        then we sync with main thread using a QueuedConnection with finishedGetLine
    m_notifier->setEnabled(true);
    m_notifier->moveToThread(&m_thread);
    QObject::connect(
        &m_thread , &QThread::finished,
        m_notifier, &QObject::deleteLater
        );
#ifdef Q_OS_WIN
    QObject::connect(m_notifier, &QWinEventNotifier::activated,
                     [this]() {
                         std::string line;
                         std::getline(std::cin, line);
                         if (line.empty() && std::cin.eof()) {
                             // stdin reached EOF: the console handle stays
                             // permanently signaled, which would make the
                             // notifier fire endlessly (busy-spin). Stop it.
                             m_notifier->setEnabled(false);
                             return;
                         }
                         QString strLine = QString::fromStdString(line);
                         Q_EMIT this->finishedGetLine(strLine);
                     });
#else
    QObject::connect(m_notifier, &QSocketNotifier::activated,
                     [this](QSocketDescriptor socket, QSocketNotifier::Type /*activationEvent*/) {
                         std::string line;
                         QFile file;
                         if (file.open(socket, QIODevice::ReadOnly)) {
                             //std::getline(std::cin, line);
                             line = file.readLine().toStdString();
                             if (line.empty()) {
                                 // readLine() == 0 means the underlying read()
                                 // returned 0: stdin reached EOF. EOF keeps the
                                 // descriptor permanently "readable", so the
                                 // notifier fires endlessly (busy-spin, ~180%
                                 // idle CPU, REQ-SW-APP-002). Disable it after
                                 // the first empty read. A live terminal/pipe
                                 // with no input never reaches this branch: its
                                 // fd is not readable, so the notifier does not
                                 // fire at all.
                                 m_notifier->setEnabled(false);
                                 file.close();
                                 return;
                             }
                             QString strLine = QString::fromStdString(line);
                             Q_EMIT this->finishedGetLine(strLine);
                         }
                     });
#endif
    m_thread.start();
}

void QConsoleListener::on_finishedGetLine(const QString &strNewLine)
{
    Q_EMIT this->newLine(strNewLine);
}

QConsoleListener::~QConsoleListener()
{
    m_thread.quit();
    m_thread.wait();
}

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
                         QString strLine = QString::fromStdString(line);
                         qDebug() << "Emit: " << strLine;
                         Q_EMIT this->finishedGetLine(strLine);
                     });
#else
    QObject::connect(m_notifier, &QSocketNotifier::activated,
                     [this](QSocketDescriptor socket, QSocketNotifier::Type activationEvent) {
                         std::string line;
                         QFile file;
                         if(file.open(socket, QIODevice::ReadOnly))
                         {
                             //std::getline(std::cin, line);
                             line = file.readLine().toStdString();
                             QString strLine = QString::fromStdString(line);

                             qDebug() << "Emit: " << strLine;
                             Q_EMIT this->finishedGetLine(strLine);
                         }
                     });
#endif
    m_thread.start();
}

void QConsoleListener::on_finishedGetLine(const QString &strNewLine)
{
    Q_EMIT this->newLine(strNewLine);
    qDebug() << "Written: " << strNewLine;
}

QConsoleListener::~QConsoleListener()
{
    m_thread.quit();
    m_thread.wait();
}

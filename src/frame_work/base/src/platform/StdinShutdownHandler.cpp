#include "StdinShutdownHandler.h"

#include <QDebug>

#include <iostream>

StdinShutdownHandler::StdinShutdownHandler(QObject *parent)
    : ShutdownHandler(parent)
{
}

StdinShutdownHandler::~StdinShutdownHandler() = default;

bool StdinShutdownHandler::initialize()
{
#ifdef Q_OS_WIN
    auto *notifier = new QWinEventNotifier(GetStdHandle(STD_INPUT_HANDLE), this);
    QObject::connect(notifier, &QWinEventNotifier::activated,
                     this, &StdinShutdownHandler::onActivated);
#else
    auto *notifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this);
    QObject::connect(notifier, &QSocketNotifier::activated,
                     this, &StdinShutdownHandler::onActivated);
#endif

    qDebug() << "Stdin shutdown handler initialized (quit/exit commands)";
    return true;
}

void StdinShutdownHandler::onActivated()
{
    std::string line;
    if (!std::getline(std::cin, line)) {
        return; // EOF or error
    }

    QString cmd = QString::fromStdString(line).trimmed();

    if (cmd.compare("quit", Qt::CaseInsensitive) == 0 ||
        cmd.compare("exit", Qt::CaseInsensitive) == 0) {
        qDebug() << "Received stdin shutdown command:" << cmd;
        Q_EMIT shutdownRequested();
    }
}

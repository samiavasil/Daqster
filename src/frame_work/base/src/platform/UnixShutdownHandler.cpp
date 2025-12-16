#include "UnixShutdownHandler.h"
#include <QDebug>
#include <QCoreApplication>

UnixShutdownHandler* UnixShutdownHandler::s_instance = nullptr;

UnixShutdownHandler::UnixShutdownHandler(QObject *parent)
    : ShutdownHandler(parent)
{
    s_instance = this;
}

UnixShutdownHandler::~UnixShutdownHandler()
{
    s_instance = nullptr;
}

bool UnixShutdownHandler::initialize()
{
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // kill command
    
    qDebug() << "Unix shutdown handler initialized (SIGINT, SIGTERM)";
    return true;
}

void UnixShutdownHandler::signalHandler(int signal)
{
    if (s_instance && (signal == SIGINT || signal == SIGTERM)) {
        qDebug() << "\nReceived signal" << signal << "(" 
                 << (signal == SIGINT ? "Ctrl+C" : "SIGTERM") 
                 << "), shutting down gracefully...";
        
        // Emit shutdown signal through Qt's event system
        QMetaObject::invokeMethod(s_instance, "shutdownRequested", Qt::QueuedConnection);
    }
}

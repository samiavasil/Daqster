#ifndef TESTPLUGINTHREADCREATION_H
#define TESTPLUGINTHREADCREATION_H

#include <QThread>

class TestPluginCreation
{

public:
    TestPluginCreation();
public slots:
    void stopRunning();
virtual void run(QObject* Parent);

signals:

private:
    bool isRunning;
};

#endif // TESTPLUGINTHREADCREATION_H

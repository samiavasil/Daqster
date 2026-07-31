#pragma once

#include "QBasePluginObject.h"

class QMainWindow;

class RequirementsManagerObject : public Daqster::QBasePluginObject
{
    Q_OBJECT
public:
    RequirementsManagerObject(QObject* Parent = nullptr);
    virtual ~RequirementsManagerObject();
    void SetName(const QString& name);
    virtual bool Initialize();

protected:
    virtual void DeInitialize();

public slots:
    void MainWinDestroyed(QObject* obj);

private:
    QMainWindow* m_Win;
};

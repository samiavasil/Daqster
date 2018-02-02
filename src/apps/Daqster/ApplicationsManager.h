#ifndef APPLICATIONSMANAGER_H
#define APPLICATIONSMANAGER_H
#include<QObject>
#include<QList>
#include<QString>
#include<QProcess>

class ApplicationsManager:public QObject
{
    Q_OBJECT
public:
    static ApplicationsManager& Instance();

public slots:
    void StartApplication(const QString& Name, const QStringList & Arguments, QProcess::OpenMode Mode = QProcess::ReadWrite );

private:
    ApplicationsManager();
    QList<QProcess*>  m_ProcessList;
    static ApplicationsManager* m_Manager;
};

#endif // APPLICATIONSMANAGER_H

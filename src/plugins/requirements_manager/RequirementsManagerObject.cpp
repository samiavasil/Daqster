#include "RequirementsManagerObject.h"
#include "RequirementsWidget.h"
#include "RequirementsParser.h"
#include "debug.h"
#include "LogCategories.h"

#include <QMainWindow>
#include <QVBoxLayout>

RequirementsManagerObject::RequirementsManagerObject(QObject* Parent)
    : Daqster::QBasePluginObject(Parent)
    , m_Win(nullptr)
{
}

RequirementsManagerObject::~RequirementsManagerObject()
{
    DeInitialize();
}

void RequirementsManagerObject::SetName(const QString& name)
{
    if (nullptr != m_Win) {
        m_Win->setWindowTitle(name);
    }
}

bool RequirementsManagerObject::Initialize()
{
    m_Win = new QMainWindow();
    m_Win->setWindowTitle("Requirements Manager");

    auto* widget = new Daqster::RequirementsWidget(m_Win);
    m_Win->setCentralWidget(widget);

    // Dev builds put the binary inside the build tree (e.g. build_qt5/bin);
    // discoverRepoRoots() walks up to the first repo root that contains the
    // requirements tree and then merges any sibling repo roots found next to
    // it (public + private requirement trees, REQ-SW-PL-012).
    const QStringList roots = Daqster::RequirementsParser::discoverRepoRoots();
    widget->openDirectories(roots);

    m_Win->resize(1100, 700);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)));
    return true;
}

void RequirementsManagerObject::DeInitialize()
{
    if (nullptr != m_Win) {
        m_Win->deleteLater();
    }
    DEBUG_V << "RequirementsManagerObject destroyed";
}

void RequirementsManagerObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}

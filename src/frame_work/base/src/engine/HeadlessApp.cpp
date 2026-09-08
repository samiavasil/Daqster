#include "HeadlessApp.h"

#include <QByteArray>
#include <QtGlobal>

namespace Daqster {

void configureHeadlessPlatform()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
}

} // namespace Daqster

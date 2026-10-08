#include "StreamUrlValidator.h"

#include <QObject>
#include <QUrl>

namespace StreamUrlValidator {

bool isValidStreamUrl(const QString &urlString, QString *errorOut)
{
    const QString trimmed = urlString.trimmed();
    if (trimmed.isEmpty()) {
        if (errorOut != nullptr)
            *errorOut = QObject::tr("Enter a stream URL first");
        return false;
    }

    const QUrl url(trimmed);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || scheme.isEmpty()) {
        if (errorOut != nullptr)
            *errorOut = QObject::tr("Invalid stream URL");
        return false;
    }
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")
        && scheme != QStringLiteral("rtsp")) {
        if (errorOut != nullptr)
            *errorOut = QObject::tr("Unsupported stream scheme: %1").arg(url.scheme());
        return false;
    }

    return true;
}

} // namespace StreamUrlValidator

#pragma once

#include <QString>
#include <QtGlobal>

#include "GL/VideoGLContextManager.h"

enum class VideoBackend { Gl, Software };

/// Select the display backend. Env override DAQSTER_VIDEO_BACKEND=gl|software
/// wins; otherwise auto-detect via VideoGLContextManager::hasHardwareGL()
/// (cached for the process lifetime).
inline VideoBackend detectVideoBackend()
{
    if (qEnvironmentVariableIsSet("DAQSTER_VIDEO_BACKEND")) {
        const QString v = qEnvironmentVariable("DAQSTER_VIDEO_BACKEND").toLower();
        if (v == QStringLiteral("software") || v == QStringLiteral("cpu"))
            return VideoBackend::Software;
        if (v == QStringLiteral("gl") || v == QStringLiteral("gpu")
            || v == QStringLiteral("opengl"))
            return VideoBackend::Gl;
    }
    return VideoGLContextManager::hasHardwareGL() ? VideoBackend::Gl
                                                  : VideoBackend::Software;
}
// SPDX-License-Identifier: MIT

#include "Scene/SceneLayout.h"

#include "Scene/SceneVariables.h"

namespace Daqster {
namespace {

/// The token that carries the root (REQ-SW-PL-091).
const char* const kDepsToken = "DAQSTER_DEPS";

/// Subpaths of the contract. One place, because `SceneVariables` derives the
/// two legacy tokens from **these** strings — a second copy would drift.
const char* const kLlamaBuild = "llama.cpp/build";
const char* const kModels = "models";
const char* const kPiper = "piper";
const char* const kPiperVoices = "piper-voices";
const char* const kWhisperBuild = "whisper.cpp/build";
const char* const kSdServer = "sd-server";

} // namespace

SceneLayout::SceneLayout(const QString& depsRoot)
    : m_depsRoot(depsRoot.trimmed())
{
}

SceneLayout SceneLayout::fromMachine(QString* reason)
{
    if (reason != nullptr)
        reason->clear();

    const QString root = SceneVariables::instance().tokenValue(QLatin1String(kDepsToken));
    if (root.isEmpty()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("DAQSTER_DEPS is not set (machine config "
                                     "'paths/DAQSTER_DEPS' or the process "
                                     "environment) — the dependency layout has "
                                     "no root");
        }
        return SceneLayout();
    }

    return SceneLayout(root);
}

QString SceneLayout::join(const QString& relative) const
{
    if (m_depsRoot.isEmpty())
        return QString();

    // QDir::filePath keeps the native separator on Windows and does not touch
    // the filesystem, so a root that does not exist yet still yields the path
    // the installer (REQ-SW-PL-089) is going to create.
    return QDir(m_depsRoot).filePath(relative);
}

QString SceneLayout::llamaBuildRoot() const
{
    return join(QLatin1String(kLlamaBuild));
}

QString SceneLayout::llamaBinDir() const
{
    return llamaBuildRoot().isEmpty()
        ? QString()
        : QDir(llamaBuildRoot()).filePath(QStringLiteral("bin"));
}

QString SceneLayout::modelsDir() const
{
    return join(QLatin1String(kModels));
}

QString SceneLayout::modelsDirFor(const QString& kind) const
{
    const QString name = kind.trimmed();
    if (name.isEmpty() || !kinds().contains(name))
        return QString();

    const QString models = modelsDir();
    if (models.isEmpty())
        return QString();   // no root → no kind directory, not "llm" in the cwd

    return QDir(models).filePath(name);
}

QString SceneLayout::piperBin() const
{
    return join(QStringLiteral("%1/piper").arg(QLatin1String(kPiper)));
}

QString SceneLayout::piperVoicesDir() const
{
    return join(QLatin1String(kPiperVoices));
}

QString SceneLayout::whisperBin() const
{
    return join(QStringLiteral("%1/bin/whisper-server").arg(QLatin1String(kWhisperBuild)));
}

QString SceneLayout::sdServer() const
{
    return join(QLatin1String(kSdServer));
}

QStringList SceneLayout::kinds()
{
    return QStringList{
        QStringLiteral("llm"),
        QStringLiteral("asr"),
        QStringLiteral("vlm"),
        QStringLiteral("embedding"),
        QStringLiteral("tts"),
        QStringLiteral("sd"),
    };
}

} // namespace Daqster

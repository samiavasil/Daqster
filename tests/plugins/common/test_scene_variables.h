#pragma once

#include <QByteArray>
#include <QVector>
#include <QtTest>

#include <memory>

class QTemporaryDir;

/// Tests for `Daqster::SceneVariables` (REQ-SW-PL-091) — the `${VAR}` resolver a
/// node calls while loading a scene.
///
/// Every test runs against a temporary `XDG_CONFIG_HOME` **and** a temporary
/// `HOME` (the resolver's launcher-independent candidate lives under `HOME`),
/// so the real `~/.config/Daqster/scene-variables.json` is never read and never
/// written. The known token names are unset for the duration as well: a test
/// that accidentally inherits `DAQSTER_DEPS` from the shell would pass for the
/// wrong reason.
class TestSceneVariables : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /// The four syntaxes plus the "leave it visible" rule.
    void expandsTheFourSyntaxes();
    /// override > machine config > process environment > empty.
    void precedenceIsOverrideConfigEnvironment();
    /// `${X:?msg}` reports the token **and** the message, and yields "".
    void requiredTokenReportsTokenAndMessage();
    /// `$$` is the only literal `$`; `${}` and an unclosed `${` stay as they are.
    void dollarEscapeAndMalformedExpressions();
    /// The out-param wrapper reports the same token as the detailed call.
    void expandWrapperReportsToken();
    /// Tokens are persisted in `paths/…` and survive a cache reset.
    void tokensRoundTripThroughTheDocument();
    /// Names that look like syntax are rejected instead of silently breaking.
    void rejectsNamesThatLookLikeSyntax();
    /// `seedFromEnvironment()` never overwrites an existing value.
    void seedFromEnvironmentKeepsExistingValue();
    /// The default path honours the env override and stays under the temp roots.
    void defaultPathIsOneRootPerMachine();

private:
    /// One environment variable as it was before `init()` touched it.
    struct EnvSnapshot
    {
        QByteArray name;
        bool       wasSet = false;
        QByteArray value;
    };

    void       overrideEnvironment(const QByteArray& name, const QByteArray& value);
    void       unsetEnvironment(const QByteArray& name);
    void       restoreEnvironment();

    std::unique_ptr<QTemporaryDir> m_configDir;
    std::unique_ptr<QTemporaryDir> m_homeDir;
    QVector<EnvSnapshot>           m_environment;
    QString                        m_document;
};

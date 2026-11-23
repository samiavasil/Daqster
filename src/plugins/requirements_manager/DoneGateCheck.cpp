#include "DoneGateCheck.h"

#include <QDir>
#include <QFileInfo>

namespace Daqster {

namespace {

constexpr int kGitTimeoutMs = 10000;

} // namespace

DoneGateCheck::DoneGateCheck(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &DoneGateCheck::onTimeout);
}

DoneGateCheck::~DoneGateCheck()
{
    // A child still running would be destroyed under a live pipe; kill it and
    // drop the continuation, the answer is nobody's business any more.
    abort();
}

void DoneGateCheck::abort()
{
    m_running = false;
    m_delivered = true;
    m_continuation = nullptr;
    m_timeout.stop();
    // Bounded: terminate, then kill. A child left running here would be
    // destroyed under a live pipe.
    m_process.stop(200, 200);
}

void DoneGateCheck::start(const QVector<RequirementRoot> &roots,
                          const QVector<Requirement> &requirements)
{
    abort();

    m_roots = roots;
    m_requirements = requirements;
    m_develop.fill(Develop::RepoMissing, m_roots.size());
    m_branch.clear();
    for (int i = 0; i < m_roots.size(); ++i)
        m_branch.append(QString());
    m_found.clear();
    m_remaining.clear();
    m_answers.clear();
    m_rootIndex = 0;
    m_running = true;

    // One question per distinct path: two requirements citing the same file cost
    // one line in the batch, not two processes.
    for (const Requirement &req : m_requirements) {
        for (const QString &path : DoneGate::citedPaths(req)) {
            if (!m_remaining.contains(path))
                m_remaining.append(path);
        }
    }

    if (m_remaining.isEmpty()) {
        // Nothing cited anywhere: no git to ask, but the answer still arrives
        // from the event loop (a caller may start the check inside its load).
        QTimer::singleShot(0, this, &DoneGateCheck::finish);
        return;
    }
    checkNextRoot();
}

void DoneGateCheck::runGit(const QString &root, const QStringList &arguments,
                           const QByteArray &input,
                           std::function<void(bool, const QByteArray &out)> continuation)
{
    if (!m_running) {
        // aborted between two commands: no continuation is owed
        return;
    }
    if (m_process.isRunning()) {
        // The previous child is still winding down (it was killed on the
        // deadline). Starting a second one on the same owner is not allowed, and
        // "no answer" is a refusal in every step below — the safe reading.
        continuation(false, QByteArray());
        return;
    }

    m_continuation = std::move(continuation);
    m_delivered = false;
    m_timeout.start(kGitTimeoutMs);
    if (!m_process.start(QStringLiteral("git"), arguments, root)) {
        deliver(false, QByteArray());
        return;
    }

    QProcess *git = m_process.process();
    if (!git) {
        deliver(false, QByteArray());
        return;
    }
    // Borrowed: the owner deletes it when it finishes. Connected right after
    // start(), so no `finished` of this child can be missed.
    connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &DoneGateCheck::onProcessFinished);
    connect(git, &QProcess::errorOccurred, this, &DoneGateCheck::onProcessError);
    if (!input.isEmpty()) {
        // Buffered by QProcess until the child is running.
        git->write(input);
        git->closeWriteChannel();
    }
}

void DoneGateCheck::deliver(bool ok, const QByteArray &out)
{
    if (m_delivered)
        return;
    m_delivered = true;
    m_timeout.stop();
    auto continuation = std::move(m_continuation);
    m_continuation = nullptr;
    if (continuation)
        continuation(ok, out);
}

void DoneGateCheck::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_delivered)
        return;   // a killed child (abort, deadline) is not an answer
    QProcess *git = m_process.process();
    const bool ok = status == QProcess::NormalExit && exitCode == 0;
    // Read here, in the child's own `finished`: the owner deletes it afterwards.
    deliver(ok, git ? git->readAllStandardOutput() : QByteArray());
}

void DoneGateCheck::onProcessError(QProcess::ProcessError error)
{
    // FailedToStart is the only error with no `finished` behind it; a crash
    // reports both, and the guard in deliver() drops the second one.
    if (error == QProcess::FailedToStart)
        deliver(false, QByteArray());
}

void DoneGateCheck::onTimeout()
{
    // The child is killed through its owner (bounded), and the step gets "no
    // answer" — a refusal, never a pass.
    m_process.stop(100, 100);
    deliver(false, QByteArray());
}

// ── The pass: develop per repo, every cited path, then the branches ──────────

void DoneGateCheck::checkNextRoot()
{
    if (m_rootIndex >= m_roots.size()) {
        afterRoots();
        return;
    }

    const int index = m_rootIndex;
    const QString root = m_roots.at(index).repoRoot;
    runGit(root,
           {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
            QStringLiteral("--quiet"), QStringLiteral("develop^{commit}")},
           QByteArray(),
           [this, index](bool ok, const QByteArray &) {
               if (!m_running)
                   return;
               if (ok) {
                   m_develop[index] = Develop::Present;
                   ++m_rootIndex;
                   checkNextRoot();
                   return;
               }
               // A repo without the branch is a different failure from a
               // directory that is not a repo at all: only the second one is
               // "nothing is checked here" (REQ-SW-PL-107 R1-1).
               const QString repoRoot = m_roots.at(index).repoRoot;
               runGit(repoRoot,
                      {QStringLiteral("rev-parse"), QStringLiteral("--git-dir")},
                      QByteArray(),
                      [this, index](bool isRepo, const QByteArray &) {
                          if (!m_running)
                              return;
                          m_develop[index] = isRepo ? Develop::Branchless
                                                    : Develop::RepoMissing;
                          ++m_rootIndex;
                          checkNextRoot();
                      });
           });
}

void DoneGateCheck::afterRoots()
{
    if (m_remaining.isEmpty() || !anyDevelop()) {
        // Nothing left to ask, or no repo can answer at all: the answers are
        // the refusal (whyNoDevelop()) and the branches are not needed.
        if (m_remaining.isEmpty()) {
            m_rootIndex = 0;   // the branch pass has its own walk over the roots
            askNextBranch();
        } else {
            finish();
        }
        return;
    }
    m_rootIndex = 0;
    askNextPathsRoot();
}

void DoneGateCheck::askNextPathsRoot()
{
    while (m_rootIndex < m_roots.size()
           && m_develop.at(m_rootIndex) != Develop::Present) {
        ++m_rootIndex;
    }
    if (m_rootIndex >= m_roots.size() || m_remaining.isEmpty()) {
        m_rootIndex = 0;   // the branch pass has its own walk over the roots
        askNextBranch();
        return;
    }

    const int index = m_rootIndex;
    const QStringList asked = m_remaining;
    QByteArray input;
    for (const QString &path : asked)
        input += QByteArray("develop:") + path.toUtf8() + '\n';

    runGit(m_roots.at(index).repoRoot,
           {QStringLiteral("cat-file"), QStringLiteral("--batch-check")},
           input,
           [this, index, asked](bool ok, const QByteArray &out) {
               if (!m_running)
                   return;
               // --batch-check answers "<id> <type> <size>" per line and
               // "<rev>:<path> missing" for the absent ones; the answers are one
               // per asked path, in order.
               const QStringList lines = ok
                                             ? QString::fromUtf8(out).split(QLatin1Char('\n'),
                                                                           Qt::SkipEmptyParts)
                                             : QStringList();
               QStringList stillMissing;
               for (int i = 0; i < asked.size(); ++i) {
                   // A line git did not even produce is missing, not resolved:
                   // silence must never read as a pass.
                   const bool missing = i >= lines.size()
                                        || lines.at(i).trimmed().endsWith(
                                            QStringLiteral(" missing"));
                   if (missing)
                       stillMissing.append(asked.at(i));
                   else
                       m_found.insert(asked.at(i));
               }
               m_remaining = stillMissing;
               m_rootIndex = index + 1;
               askNextPathsRoot();
           });
}

void DoneGateCheck::askNextBranch()
{
    while (m_rootIndex < m_roots.size()
           && m_develop.at(m_rootIndex) == Develop::RepoMissing) {
        ++m_rootIndex;
    }
    if (m_rootIndex >= m_roots.size()) {
        finish();
        return;
    }

    const int index = m_rootIndex;
    runGit(m_roots.at(index).repoRoot,
           {QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
            QStringLiteral("HEAD")},
           QByteArray(),
           [this, index](bool ok, const QByteArray &out) {
               if (!m_running)
                   return;
               if (ok)
                   m_branch[index] = QString::fromUtf8(out).trimmed();
               m_rootIndex = index + 1;
               askNextBranch();
           });
}

void DoneGateCheck::finish()
{
    m_running = false;
    m_answers.clear();

    const bool answered = anyDevelop();
    for (const Requirement &req : m_requirements) {
        DoneGate::CodeAnswer answer;
        answer.paths = DoneGate::citedPaths(req);
        answer.answered = true;

        if (answer.paths.isEmpty()) {
            answer.inDevelop = true;   // nothing cited: nothing to verify
        } else if (!answered) {
            answer.error = whyNoDevelop();
        } else {
            QStringList missing;
            for (const QString &path : answer.paths) {
                if (!m_found.contains(path))
                    missing.append(path);
            }
            if (missing.isEmpty()) {
                answer.inDevelop = true;
            } else {
                const int carrier = worktreeRootWith(missing.first());
                answer.inDevelop = false;
                answer.missingPaths = missing;
                answer.branch = carrier >= 0 ? m_branch.at(carrier) : QString();
            }
        }
        m_answers.insert(req.filePath, answer);
    }

    emit finished();
}

bool DoneGateCheck::anyDevelop() const
{
    for (const Develop state : m_develop) {
        if (state == Develop::Present)
            return true;
    }
    return false;
}

QString DoneGateCheck::whyNoDevelop() const
{
    if (m_roots.isEmpty())
        return QStringLiteral("няма заредено репо");
    QStringList reasons;
    for (int i = 0; i < m_roots.size(); ++i) {
        switch (m_develop.at(i)) {
        case Develop::Present:
            continue;
        case Develop::Branchless:
            reasons.append(QStringLiteral("няма клон `develop` в %1")
                               .arg(m_roots.at(i).repoRoot));
            break;
        case Develop::RepoMissing:
            reasons.append(QStringLiteral("%1 не е git репо").arg(m_roots.at(i).repoRoot));
            break;
        }
    }
    return reasons.join(QStringLiteral("; "));
}

int DoneGateCheck::worktreeRootWith(const QString &path) const
{
    for (int i = 0; i < m_roots.size(); ++i) {
        if (QFileInfo::exists(QDir(m_roots.at(i).repoRoot).filePath(path)))
            return i;
    }
    return -1;
}

} // namespace Daqster

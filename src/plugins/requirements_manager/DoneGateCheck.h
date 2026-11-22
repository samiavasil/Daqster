#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <functional>

#include "DoneGate.h"
#include "RequirementsParser.h"
#include "Threading/ProcessOwner.h"

namespace Daqster {

/**
 * @brief Asks git whether the cited code is in `develop` — WITHOUT blocking.
 *
 * A wait inside a plugin is forbidden (`check_no_event_loops.cmake`: a nested
 * loop re-enters the graph, the measured case enrolled one pair sixteen times
 * and ran out of file descriptors). So this class never waits: it starts a
 * `git` child, and continues in the child's `finished` signal. The caller keeps
 * its thread — the tree paints, the dialog opens, the UI never freezes for a
 * `git rev-parse`.
 *
 * @par One pass per tree, not per row
 * `start()` asks each repo's `develop` once, then reads every cited path of the
 * whole tree in ONE `git cat-file --batch-check` per repo, then each repo's
 * current branch. The answers are handed out per requirement in `answers()`.
 *
 * @par The child is owned, not hand-rolled
 * `Daqster::ProcessOwner` (REQ-SW-PL-050) holds the `git` child:
 * `check_no_bare_threads.cmake` forbids a bare `new QProcess` outside the
 * primitives' directory, and the owner is the one place that knows how to stop
 * a process in bounded time (terminate → grace → kill).
 *
 * @par A reply that never comes is a refusal
 * `git` gets a deadline; a child that does not answer, a repo without `develop`
 * and a directory that is not a repository all end as "cannot check" with the
 * reason — never as "the code is there". `DoneGate::applyCodeAnswer()` is the
 * only place that reads an answer, so a stale or missing one cannot pass.
 *
 * @par Usage
 * @code
 * auto *check = new DoneGateCheck(this);
 * connect(check, &DoneGateCheck::finished, this, [this, check] {
 *     for (Requirement &req : m_requirements) {
 *         const auto it = check->answers().constFind(req.filePath);
 *         if (it != check->answers().constEnd())
 *             DoneGate::applyCodeAnswer(req, *it);
 *     }
 * });
 * check->start(roots, requirements);
 * @endcode
 *
 * `finished()` is always emitted asynchronously (a tree that cites nothing
 * still goes through the event loop), so a caller may start the check from
 * inside the load that builds its model.
 */
class DoneGateCheck : public QObject
{
    Q_OBJECT

public:
    explicit DoneGateCheck(QObject *parent = nullptr);
    ~DoneGateCheck() override;

    /**
     * @brief Starts the check; a running pass is aborted first.
     *
     * The abort is a bounded stop of our own child (`ProcessOwner`: terminate
     * → grace → kill), so a reload that interrupts a check costs at most a few
     * hundred milliseconds — it never waits for git to answer.
     *
     * @param roots The loaded requirement roots (candidate repos).
     * @param requirements The tree to resolve; `rawContent` is read for the
     *        cited paths, nothing is written back.
     */
    void start(const QVector<RequirementRoot> &roots,
               const QVector<Requirement> &requirements);

    /// Stops the pass in flight: the child is killed, no `finished()` follows.
    void abort();

    /// True between start() and the `finished()` it causes.
    bool isRunning() const { return m_running; }

    /// The answers of the LAST finished pass, keyed by `Requirement::filePath`.
    const QHash<QString, DoneGate::CodeAnswer> &answers() const { return m_answers; }

signals:
    /// Emitted once per start(), always from the event loop, never re-entrant.
    void finished();

private:
    /// What one repo said about `develop`.
    enum class Develop { Present, Branchless, RepoMissing };

    void checkNextRoot();
    void afterRoots();
    void askNextPathsRoot();
    void askNextBranch();
    void finish();

    /**
     * @brief Runs one `git` and hands its outcome to @p continuation.
     *
     * The continuation is invoked exactly once: on the child's `finished`
     * signal, on its failure to start, or on the deadline. It may start the
     * next command.
     */
    void runGit(const QString &root, const QStringList &arguments,
                const QByteArray &input,
                std::function<void(bool ok, const QByteArray &out)> continuation);

    void deliver(bool ok, const QByteArray &out);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();

    /// Why no repo could answer — the sentence the owner reads instead of a pass.
    QString whyNoDevelop() const;
    /// Index of the root whose working tree carries @p path, -1 when none does.
    int worktreeRootWith(const QString &path) const;
    /// True when at least one repo has a `develop` to ask.
    bool anyDevelop() const;

    ProcessOwner m_process;          //!< the `git` child, owned (bounded stop)
    QTimer m_timeout;
    std::function<void(bool, const QByteArray &)> m_continuation;
    QVector<RequirementRoot> m_roots;
    QVector<Requirement> m_requirements;
    QVector<Develop> m_develop;      //!< one per root
    QStringList m_branch;            //!< current branch per root (empty = unknown)
    QStringList m_remaining;         //!< cited paths still missing from every develop
    QSet<QString> m_found;           //!< cited paths some `develop` carries
    QHash<QString, DoneGate::CodeAnswer> m_answers; //!< keyed by filePath
    int m_rootIndex = 0;
    bool m_running = false;
    bool m_delivered = true;         //!< one continuation per command, never two
};

} // namespace Daqster

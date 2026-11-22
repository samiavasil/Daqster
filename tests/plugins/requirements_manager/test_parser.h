#pragma once

#include <QtTest>

// Test class for RequirementsParser. Declared in a header (instead of relying
// on QTEST_GUILESS_MAIN inside the .cpp) so the three Requirements Manager
// test classes can share a single test binary - see test_main.cpp.
class TestParser : public QObject
{
    Q_OBJECT

private slots:
    void parseDirectory_fullMetadata();
    void parseDirectory_minimalAndDashFields();
    void parseDirectory_typedId();
    void parseDirectory_descriptionDialects();
    void parseDirectory_criteriaDialects();
    void parseDirectory_criteriaHeadingBulgarian();
    void parseDirectory_nestedSubdirSection();
    void parseDirectory_traceabilityFields();
    void parseReferences_annotationWithComma();
    void parseReferences_multipleParents();
    void parseReferences_childrenAndNonRequirementId();
    void phaseStatus_allEmpty_false();
    void phaseStatus_dashAndWhitespace_false();
    void phaseStatus_allFilled_true();
    void phaseStatus_partialCode_only();
    void phaseStatus_trimmedNonEmpty_true();
    void parseDirectory_emptyDir();
    void generateNextId_scansActiveAndArchive();
    void generateNextId_typedIsolation();
    void moveToArchive_movesFile();
    void moveToActive_movesFile();
    void moveToArchive_neverOverwrites();
    void markDone_rollsBackWhenArchiveTargetExists();
    void moveToArchive_nestedSubdir();
    void moveRejectsWrongSection();
    void writeRequirement_roundTrip();
    void approveDraft_rewritesStatusAndApproval();
    void approveDraft_secondAcceptIsNoOp();
    void approveDraft_nonDraftNoOp();
    void approveDraft_missingStatus_error();
    void approveDraft_keepsCrlfBytes();
    void parseDirectory_createdAndRevision();
    void parseDirectory_legacyDateIsCreationAlias();
    void requestChangesDraft_appendsOneRowOnly();
    void requestChangesDraft_rejectsEmptyNoteAndNonDraft();
    void rejectDraft_cancelsAndArchives();
    void startContractChange_backToDraftWithHistory();
    void startContractChange_sealsApproval();
    void approveDraft_namesTheApprovedRevision();
    void parseDirectory_approvedRevisionAndBaseline();
    void appendHistoryRow_bumpsRevision();
    void findFileById_survivesRename();
    void createdAliasDoesNotRewriteTheRealTree();
};

#pragma once

#include <QtTest>

// Test class for RequirementsValidator. Declared in a header (instead of
// relying on QTEST_GUILESS_MAIN inside the .cpp) so the three Requirements
// Manager test classes can share a single test binary - see test_main.cpp.
class TestValidator : public QObject
{
    Q_OBJECT

private slots:
    void cleanFixture_noIssues();
    void danglingParent_error();
    void danglingDependency_error();
    void cycle_detection();
    void missingFields_warnings();
    void selfReference_error();
    void archivedDependency_warning();
    void doneDependency_noWarning();
    void typedIdFormat_validAndMalformed();
    void twoDigitId_warning();
    void duplicateId_acrossRepos_error();
    void crossRepoParent_notDangling();
    void hintMismatch_warning();
    void hintMatch_noWarning();
    void scope_onlyRequirementsTreeIsValidated();
    void statusVocabulary_hasOneEntryPoint();
    void statusVocabulary_invalidStatus_error();
    void statusVocabulary_doneNaKlonaMapsToImplemented();
    void statusZone_activeInArchive_error();
    void statusVocabulary_draftOnlyInActive();
    void statusZone_doneInActive_error();
    void statusZone_implementedInArchive_noIssue();
    void doneWithOpenCriterion_error();
    void doneWithDeferredCriterion_warning();
    void nonRequirementParent_warning();
    void multipleParents_secondDangling_error();
    void templateV2_canonical_noIssue();
    void templateV1_deviationWarning();
    void templateV0_deviationWarning();
    void templateMissing_descriptionError();
    void dependencyOnImplemented_silent();
    void dependencyOnArchivedUnknownStatus_warning();
    void childDangling_error();
    void dateNotIso_warning();
    void dateMissing_warningStays();
    void createdField_isTheCreationDate();
    void citedPath_activeMissingPath_warning();
    void citedPath_implementedMissingPath_error();
    void citedPath_draftAndExisting_silent();
    // REQ-SW-PL-107: the DONE gate's one decision.
    void doneGate_candidateWhenAllFourHold();
    void doneGate_notCandidateWhenCriterionOpen();
    void doneGate_notCandidateWhenNoReviewRecord();
    void doneGate_notCandidateWhenVerdictReturned();
    void doneGate_notCandidateWhenCodeOnlyOnBranch();
    void doneGate_refusesWhenNoDevelopBranch();
    void doneGate_publicPathResolvedByPublicDevelop();
    void doneGate_doneOutsideArchiveReported();
    // REQ-SW-PL-106: ревизия, по-голяма от одобрената.
    void revisionAboveApproved_error();
    void revisionWithoutNumber_legacySilent();
    // REQ-SW-PL-098: връзката идея ↔ изискване, в двете посоки.
    void ideaLink_citedIdeaWithoutRecord_error();
    void ideaLink_promotionToMissingRequirement_error();
    void ideaLink_promotedPairAndUnpromotedIdea_silent();
    void ideaLink_conflictingPromotionKeys_error();
    // REQ-SW-PL-098, решение „А“: една връзка, две страни.
    void ideaLink_promotionWithoutFromIdea_error();
    void ideaLink_fromIdeaWithoutPromotion_error();
    void ideaLink_cleanPairInBothDirections_silent();
};

# Anti-rot за прозата: публичното дърво не бива да съдържа частни специфики в
# документацията (REQ-SW-PL-094). Само bash + grep — без Qt, без билд, под секунда.
#
# `scripts/check-public-docs.sh` сканира `docs/` и root `*.md` и сравнява с
# `scripts/public-docs-baseline.txt` (замразени срещания, само за намаляване).

find_program(BASH_PROGRAM bash)

if(BASH_PROGRAM)
    add_test(NAME docs_no_private_leaks
             COMMAND ${BASH_PROGRAM} ${DAQSTER_SOURCE_DIR}/scripts/check-public-docs.sh)
    set_tests_properties(docs_no_private_leaks PROPERTIES
        TIMEOUT 60
        WORKING_DIRECTORY ${DAQSTER_SOURCE_DIR})
else()
    message(STATUS "docs_no_private_leaks: пропуснат (няма bash)")
endif()

# Anti-rot за сцените: комитирана `.flow` не носи машинен път (REQ-SW-PL-091).
#
# `scripts/check-scene-paths.sh` обхожда `git ls-files '*.flow'` и иска всяко
# поле-път (`*Path`/`*File`/`*Binary`) да е празно, относително име до сцената
# или `${DAQSTER_VIDEO_FILE:-<име>}`. Абсолютен път (`/mnt/...`) значи, че
# сцената работи на точно една машина. Няма baseline — дългът е нула.
#
# Само bash + python3 (чете JSON), без Qt и без билд — под секунда, затова върви
# дори в build без тежести.

find_program(BASH_PROGRAM bash)
find_program(PYTHON3_PROGRAM python3)

if(BASH_PROGRAM AND PYTHON3_PROGRAM)
    add_test(NAME scene_paths
             COMMAND ${BASH_PROGRAM} ${DAQSTER_SOURCE_DIR}/scripts/check-scene-paths.sh)
    set_tests_properties(scene_paths PROPERTIES
        TIMEOUT 60
        WORKING_DIRECTORY ${DAQSTER_SOURCE_DIR})
else()
    message(STATUS "scene_paths: пропуснат (няма bash или python3)")
endif()

# From https://www.mattkeeter.com/blog/2018-01-06-versioning/

message("Update script_version.hpp")
execute_process(COMMAND ${BASH} ./scripts/update-versionfile.sh)

message("Writing version.cpp")

find_program(GIT "git")

if(GIT)
    message("Found git at:")
    message(${GIT})
else()
    message("Did not find git!")
endif()

find_program(BASH "bash")
if(BASH)
    message("Found bash at:")
    message(${BASH})
else()
    message("Did not find bash!")
endif()

# Write version.cpp

execute_process(COMMAND ${GIT} log --pretty=format:'%h' -n 1
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
else()
    execute_process(
        COMMAND ${BASH} -c "git diff --quiet --exit-code || echo +"
        OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
        COMMAND ${GIT} describe --exact-match --tags
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
        COMMAND ${GIT} rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH)

    execute_process(
        COMMAND ${GIT} describe --tags --abbrev=0 --match v*
        OUTPUT_VARIABLE GIT_LAST_VERSION
    )

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
    string(STRIP "${GIT_LAST_VERSION}" GIT_LAST_VERSION)
    string(REGEX REPLACE "^v" "" GIT_LAST_VERSION "${GIT_LAST_VERSION}")
endif()

set(VERSION "#include \"version.h\"

const std::string GRAD_MEH_GIT_REV = \"${GIT_REV}${GIT_DIFF}\";
const std::string GRAD_MEH_GIT_TAG = \"${GIT_TAG}\";
const std::string GRAD_MEH_GIT_BRANCH = \"${GIT_BRANCH}\";
const std::string GRAD_MEH_GIT_LAST_VERSION = \"${GIT_LAST_VERSION}\";
")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/version.cpp)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.cpp VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version.cpp "${VERSION}")
endif()

# Write script_version.hpp
if ("${GIT_REV}" STREQUAL "")
    set(VERSION_MAJOR "0")
    set(VERSION_MINOR "0")
    set(VERSION_PATCH "0")
    set(VERSION_BUILD "0")
else()
    execute_process(
        COMMAND ${GIT} describe --tag --always
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    string(REGEX REPLACE "^v" "" GIT_TAG "${GIT_TAG}")
    message("Using git tag ${GIT_TAG}")

    string(REPLACE "-" ";" GIT_TAG_LIST ${GIT_TAG})
    list(LENGTH GIT_TAG_LIST GIT_TAG_LIST_LENGTH)

    list(GET GIT_TAG_LIST 0 VERSION_TAG)
    if(GIT_TAG_LIST_LENGTH GREATER 1)
        list(GET GIT_TAG_LIST 1 VERSION_BUILD)
    else()
        set(VERSION_BUILD "0")
    endif()
    message("Tag: ${VERSION_TAG}")
    message("Build: ${VERSION_BUILD}")
    message("")
    string(REPLACE "." ";" GIT_VERSION_LIST ${VERSION_TAG})
    list(GET GIT_VERSION_LIST 0 VERSION_MAJOR)
    list(GET GIT_VERSION_LIST 1 VERSION_MINOR)
    list(GET GIT_VERSION_LIST 2 VERSION_PATCH)
endif()

set(VERSION "#define MAJOR ${VERSION_MAJOR}
#define MINOR ${VERSION_MINOR}
#define PATCHLVL ${VERSION_PATCH}
#define BUILD ${VERSION_BUILD}
")

if(EXISTS ${SCRIPTVERSION_PATH})
    file(READ ${SCRIPTVERSION_PATH} VERSION_)
else()
    set(VERSION_ "")
endif()

message("Version: " ${VERSION})
message("script_version.hpp Path: " ${SCRIPTVERSION_PATH})

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${SCRIPTVERSION_PATH} "${VERSION}")
endif()

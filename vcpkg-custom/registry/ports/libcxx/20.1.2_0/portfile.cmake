vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO llvm/llvm-project
    REF 58df0ef89dd64126512e4ee27b4ac3fd8ddf6247
    HEAD_REF llvmorg-20.1.2
    SHA512 31041939e9bdfaf2103fa6facae3ec43c3fd27de680ea5edaf780ea46a5202c047b31875740273d575aa023f7a2e7f1db0ccf66dbc9447faa96f8a30ef20777f
)

file(REMOVE_RECURSE "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg" "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel")

set(BUILD_DIR "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel")

set(INSTALL_PREFIX "${CURRENT_PACKAGES_DIR}")

configure_file("${CMAKE_CURRENT_LIST_DIR}/build.sh.in" "${BUILD_DIR}/build.sh" @ONLY)

vcpkg_execute_required_process(
    COMMAND "${SHELL}" ./build.sh
    WORKING_DIRECTORY "${BUILD_DIR}"
    LOGNAME "build-${TARGET_TRIPLET}-rel"
)

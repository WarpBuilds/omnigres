include(CPM)
include(${CMAKE_CURRENT_LIST_DIR}/dependencies/versions.cmake)
include(CheckCSourceCompiles)

CPMAddPackage(NAME uriparser GIT_REPOSITORY https://github.com/uriparser/uriparser GIT_TAG ${GIT_TAG_uriparser} VERSION ${VERSION_uriparser}
              OPTIONS "BUILD_SHARED_LIBS OFF" "URIPARSER_BUILD_DOCS OFF" "URIPARSER_BUILD_TESTS OFF" "URIPARSER_BUILD_TOOLS OFF"
                      "URIPARSER_ENABLE_INSTALL OFF" "URIPARSER_WARNINGS_AS_ERRORS ON")
set_property(TARGET uriparser PROPERTY POSITION_INDEPENDENT_CODE ON)

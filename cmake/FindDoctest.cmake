include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY  https://github.com/doctest/doctest.git
    GIT_TAG         v2.4.11
)

FetchContent_MakeAvailable(doctest)

# Expose required variable (DOCTEST_INCLUDE_DIR) to parent scope
message("doctest dir : ${doctest_SOURCE_DIR}")
set(DOCTEST_INCLUDE_DIR ${doctest_SOURCE_DIR}/doctest CACHE INTERNAL "Path to include folder for doctest")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${doctest_SOURCE_DIR}/scripts/cmake/")

cmake_minimum_required(VERSION 3.4)
project(cmake_test VERSION 0.0.1 LANGUAGES CXX)

# Prepare doctest for other targets to use
find_package(Doctest REQUIRED)
# add it globally
include_directories(${DOCTEST_INCLUDE_DIR})

find_package(LibUSB)
include_directories(${LibUSB_INCLUDE_DIRS})

# Make test executable
add_executable(tests
    test.cpp
    test_init.cpp
    test_parser.cpp
    )

target_compile_features(tests PRIVATE cxx_std_17)
target_link_libraries(tests
    PRIVATE
    uac
    ${SDL2_LIBRARIES}
)

# add tests to CTest
include(doctest)
doctest_discover_tests(tests)

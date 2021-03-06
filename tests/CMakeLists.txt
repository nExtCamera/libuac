cmake_minimum_required(VERSION 3.4)
project(cmake_test VERSION 0.0.1 LANGUAGES CXX)

# Prepare doctest for other targets to use
find_package(Doctest REQUIRED)
# add it globally
include_directories(${DOCTEST_INCLUDE_DIR})

include_directories(../src)

# Make test executable
add_executable(tests
    test.cpp
    test_context.cpp
    test_parser.cpp
    test_usb_device.cpp
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

cmake_minimum_required(VERSION 3.14)

project(libuac
        VERSION 0.1
        LANGUAGES CXX
        )

option(THROW_ON_ERROR "Throw exception on error log" ON)

# Include cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


# Dependencies
find_package(LibUSB REQUIRED)
find_package(Threads REQUIRED)

# configure C++-17 requirement
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

include(GenerateExportHeader)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(SOURCES
        src/uac_context.cpp
        src/uac_device.cpp
        src/uac_parser.cpp
        src/uac_streaming.cpp
        src/uac_exceptions.cpp
)
configure_file(src/config.h.in config.h @ONLY)

add_library(uac ${SOURCES})


generate_export_header(uac)

# Specify directories which the compiler should look for headers
target_include_directories(uac
        PRIVATE src
        PUBLIC include ${LibUSB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(uac PRIVATE
        LibUSB::LibUSB
        Threads::Threads)

include(Dart)
add_subdirectory(tests)
enable_testing()

add_subdirectory(sample)

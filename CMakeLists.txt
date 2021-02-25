cmake_minimum_required(VERSION 3.4)

project(libuac
        VERSION 0.1
        LANGUAGES CXX
        )

# Include cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


# Dependencies
find_package(LibUSB)

# params
set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})


set(SOURCES
        src/libuac.cpp
        src/uac_device.cpp
        src/uac_parser.cpp
)

add_library(uac ${SOURCES})

# Specify directories which the compiler should look for headers
target_include_directories(uac
        PRIVATE src
        PUBLIC include)

target_link_libraries(uac PRIVATE LibUSB::LibUSB)


add_subdirectory(tests)

cmake_minimum_required(VERSION 3.4)
project(examples LANGUAGES CXX)

option(BUILD_EXAMPLE "Build example program" OFF)
option(BUILD_EXAMPLE_SDL "Build SDL example program" ON)

# Build non-audible, but simple example
if(BUILD_EXAMPLE)
    add_executable(example example.c)
    find_package(Threads)
    target_link_libraries(example 
        PRIVATE 
        uac 
        Threads::Threads
    )
endif()

# Build audible example
if(BUILD_EXAMPLE_SDL)
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})

    add_executable(example_sdl example_sdl.cpp)
    target_compile_features(example_sdl PRIVATE cxx_std_17)
    target_link_libraries(example_sdl
        PRIVATE
        uac
        ${SDL2_LIBRARIES}
    )
endif()


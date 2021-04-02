# Copyright (C) 2021  Dhruv Chawla
# See LICENSE at project root for license details
cmake_minimum_required(VERSION 3.10)

project(wis VERSION 0.0.7)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(wis src/wis.cpp src/ErrorLogger/ErrorLogger.cpp src/Parser/TypeResolver.cpp src/VisitorTypes.cpp
                   src/Parser/Parser.cpp src/Scanner/Scanner.cpp src/Scanner/Trie.cpp src/AST.cpp src/VM/Chunk.cpp
                   src/CodeGen/CodeGen.cpp src/VM/VM.cpp src/VM/Value.cpp src/CodeGen/Disassembler.cpp src/VM/Natives.cpp
                   src/VM/List.cpp src/ASTPrinter.cpp)

if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(wis PUBLIC /W4)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(wis PUBLIC -Wall -Wextra -pedantic)
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if (MSVC)
        target_compile_options(wis PUBLIC /Zi)
    else()
        target_compile_options(wis PUBLIC -g)
        target_link_options(wis PUBLIC -fsanitize=address -fsanitize=undefined)
        set(LD_PRELOAD "/usr/lib/libasan.so")
    endif()
else(${CMAKE_BUILD_TYPE} MATCHES "Release")
    if (MSVC)
        target_compile_options(wis PUBLIC /O2 /Ot /GL)
    else()
        target_compile_options(wis PUBLIC -O2 -flto)
    endif()
endif()

include(FetchContent)

FetchContent_Declare(
        CXXOPTS
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG        v2.2.1
)
FetchContent_MakeAvailable(CXXOPTS)
FetchContent_GetProperties(CXXOPTS)

target_link_libraries(wis PUBLIC cxxopts)
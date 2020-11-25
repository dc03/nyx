# Copyright (C) 2020  Dhruv Chawla
# See LICENSE at project root for license details
cmake_minimum_required(VERSION 3.10)

project(wis VERSION 0.0.7)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -pedantic)
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_compile_options(-g)
else(${CMAKE_BUILD_TYPE} MATCHES "Release")
    add_compile_options(-O2 -flto)
endif()

add_executable(wis src/wis.cpp src/ErrorLogger/ErrorLogger.cpp src/Parser/TypeResolver.cpp src/Parser/VisitorTypes.cpp
                   src/Parser/Parser.cpp src/Scanner/Scanner.cpp src/Scanner/Trie.cpp src/AST.cpp src/VM/Chunk.cpp
                   src/CodeGen/CodeGen.cpp src/VM/VM.cpp src/VM/Value.cpp)

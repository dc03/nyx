# Copyright (C) 2021  Dhruv Chawla
# See LICENSE at project root for license details
cmake_minimum_required(VERSION 3.10)

project(nyx VERSION 0.0.7)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(nyx src/nyx.cpp src/ErrorLogger/ErrorLogger.cpp src/Frontend/Parser/TypeResolver.cpp src/AST/VisitorTypes.cpp
        src/Frontend/Parser/Parser.cpp src/Frontend/Scanner/Scanner.cpp src/Frontend/Scanner/Trie.cpp src/AST/AST.cpp
        src/Backend/VirtualMachine/Chunk.cpp src/Backend/CodeGenerators/ByteCodeGenerator.cpp src/Backend/VirtualMachine/VirtualMachine.cpp
        src/Backend/VirtualMachine/Disassembler.cpp src/Backend/VirtualMachine/Natives.cpp src/AST/ASTPrinter.cpp
        src/Backend/VirtualMachine/Value.cpp src/Backend/VirtualMachine/StringCacher.cpp src/Frontend/CompileManager.cpp
        src/Backend/RuntimeManager.cpp src/Frontend/CompileContext.cpp src/Backend/RuntimeContext.cpp)

add_executable(nyxVM src/Backend/VirtualMachine/Chunk.cpp src/Backend/VirtualMachine/Value.cpp src/Backend/VirtualMachine/VirtualMachine.cpp
        src/Backend/VirtualMachine/VMMain.cpp src/ErrorLogger/ErrorLogger.cpp src/Backend/VirtualMachine/Disassembler.cpp
        src/Backend/VirtualMachine/Natives.cpp src/Backend/VirtualMachine/StringCacher.cpp src/Backend/RuntimeContext.cpp)

target_include_directories(nyx PUBLIC include)
target_include_directories(nyxVM PUBLIC include)

if (MSVC)
    target_compile_options(nyx PRIVATE /W4)
    target_compile_options(nyxVM PRIVATE /W4)
else()
    target_compile_options(nyx PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(nyxVM PRIVATE -Wall -Wextra -pedantic)
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if(NOT MSVC)
        # Enable sanitizers
        target_link_options(nyx PUBLIC -fsanitize=address -fsanitize=leak -fsanitize=undefined)
        set(LD_PRELOAD "/usr/lib/libasan.so")
    endif()
else(${CMAKE_BUILD_TYPE} MATCHES "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT supported OUTPUT error)

    if(supported)
        message(STATUS "IPO / LTO enabled")
        set_property(TARGET nyx PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        set_property(TARGET nyxVM PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO / LTO not supported: <${error}>")
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

FetchContent_Declare(
        TERMCOLOR
        GIT_REPOSITORY https://github.com/ikalnytskyi/termcolor.git
        GIT_TAG        v2.0.0
)

FetchContent_MakeAvailable(TERMCOLOR)
FetchContent_GetProperties(TERMCOLOR)

target_link_libraries(nyx PRIVATE cxxopts termcolor)
target_link_libraries(nyxVM PRIVATE cxxopts termcolor)

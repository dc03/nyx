# Copyright (C) 2020-2022  Dhruv Chawla
# See LICENSE at project root for license details
cmake_minimum_required(VERSION 3.10)

project(nyx VERSION 0.0.7)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES src/ErrorLogger/ErrorLogger.cpp src/Frontend/Parser/TypeResolver.cpp src/AST/VisitorTypes.cpp
        src/Frontend/Parser/Parser.cpp src/Frontend/Scanner/Scanner.cpp src/Frontend/Scanner/Trie.cpp src/AST/AST.cpp
        src/Backend/VirtualMachine/Chunk.cpp src/Backend/CodeGenerators/ByteCodeGenerator.cpp src/Backend/VirtualMachine/VirtualMachine.cpp
        src/Backend/VirtualMachine/Disassembler.cpp src/Backend/VirtualMachine/Natives.cpp src/AST/ASTPrinter.cpp
        src/Backend/VirtualMachine/Value.cpp src/Backend/VirtualMachine/StringCacher.cpp src/Frontend/FrontendManager.cpp
        src/Backend/BackendManager.cpp src/Frontend/FrontendContext.cpp src/Backend/BackendContext.cpp src/CLIConfigParser.cpp
        src/Frontend/Parser/Optimization/ConstantFolding.cpp)

add_executable(nyx-bin ${SOURCES} src/nyx.cpp)
add_executable(nyx-fmt ${SOURCES} src/nyx-fmt.cpp src/NyxFormatter.cpp)

target_include_directories(nyx-bin PUBLIC include)
target_include_directories(nyx-fmt PUBLIC include)

if (MSVC)
    target_compile_options(nyx-bin PRIVATE /W4)
    target_compile_options(nyx-fmt PRIVATE /W4)
else()
    target_compile_options(nyx-bin PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(nyx-fmt PRIVATE -Wall -Wextra -pedantic)
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if(NOT MSVC)
        # Enable sanitizers
        target_link_options(nyx-bin PUBLIC -fsanitize=address -fsanitize=leak -fsanitize=undefined)
        set(LD_PRELOAD "/usr/lib/libasan.so")
    endif()
else(${CMAKE_BUILD_TYPE} MATCHES "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT supported OUTPUT error)

    target_compile_definitions(nyx-bin PRIVATE NO_TRACE_VM)
    target_compile_definitions(nyx-fmt PRIVATE NO_TRACE_VM)

    if(supported)
        message(STATUS "IPO / LTO enabled")
        set_property(TARGET nyx-bin PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        set_property(TARGET nyx-fmt PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
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

target_link_libraries(nyx-bin PRIVATE cxxopts termcolor)
target_link_libraries(nyx-fmt PRIVATE cxxopts termcolor)

# Set up nyx library for including in other projects

add_library(${PROJECT_NAME} INTERFACE ${SOURCES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_11)

include(CMakePackageConfigHelpers)

include(GNUInstallDirs)

configure_package_config_file(
        cmake/config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(
        DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
        EXPORT ${PROJECT_NAME}-targets
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

target_link_libraries(nyx INTERFACE cxxopts termcolor)

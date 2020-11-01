cmake_minimum_required(VERSION 3.10)

project(eis VERSION 0.0.6)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -pedantic -Werror -g)
endif()

add_executable(eis src/eis.cpp src/ErrorLogger/ErrorLogger.cpp src/Parser/TypeResolver.cpp src/Parser/VisitorTypes.cpp
                   src/Parser/Parser.cpp src/Scanner/Scanner.cpp src/Scanner/Trie.cpp)

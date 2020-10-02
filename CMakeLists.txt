cmake_minimum_required(VERSION 3.10)

project(eis VERSION 0.0.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(eis src/eis.cpp src/ErrorLogger/ErrorLogger.cpp
                   src/Scanner/Scanner.cpp src/Scanner/Trie.cpp)

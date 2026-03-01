cmake_minimum_required(VERSION 3.16)
project(java_minifier CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(java_minifier
    src/main.cpp
    src/minifier.cpp
)

target_include_directories(java_minifier PRIVATE src)
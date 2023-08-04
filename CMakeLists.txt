cmake_minimum_required(VERSION 3.18)

# Setup vcpkg script with CMake (note: should be placed before project() call)
set(CMAKE_TOOLCHAIN_FILE ../vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(R3
  VERSION 1.0
  DESCRIPTION "C++/Lua engine V3"
  LANGUAGES CXX)
  
set(CMAKE_CXX_STANDARD 17)

# vcpkg dependencies
find_package(fmt CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)

# external libs not from vcpkg
find_package(Optick CONFIG REQUIRED)

add_subdirectory(source/core)
add_subdirectory(source/engine)
add_subdirectory(source/main)
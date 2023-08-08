cmake_minimum_required(VERSION 3.7)

# Setup vcpkg script with CMake (note: should be placed before project() call)
set(CMAKE_TOOLCHAIN_FILE ../vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(R3
  VERSION 1.0
  DESCRIPTION "C++/Lua engine V3"
  LANGUAGES CXX)
  
set(CMAKE_CXX_STANDARD 17)

# vcpkg dependencies
find_package(fmt REQUIRED)
find_package(SDL2 REQUIRED)
find_package(sol2 REQUIRED)
find_package(glm REQUIRED)
find_package(unofficial-vulkan-memory-allocator REQUIRED)

# external libs not from vcpkg
find_package(Optick REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(source/core)
add_subdirectory(source/engine)
add_subdirectory(source/render)
add_subdirectory(source/main)
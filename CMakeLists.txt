cmake_minimum_required(VERSION 3.8)

# Setup vcpkg script with CMake (note: should be placed before project() call)
set(CMAKE_TOOLCHAIN_FILE ../vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(R3
  VERSION 1.0
  DESCRIPTION "C++/Lua engine V3"
  LANGUAGES CXX)
  
set(CMAKE_CXX_STANDARD 20)

#Disable implicit conversions when using json lib
set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)

# Enable all safety checks in sol 
add_compile_definitions(SOL_ALL_SAFETIES_ON=1)

if (MSVC)
  add_compile_options(/bigobj)	# Enable /bigobj for msvc builds (sol adds a load of templates)
  add_compile_options(/MP)		# Multi-processor compilation (off by default)
endif ()

# vcpkg dependencies
find_package(SDL2 REQUIRED)
find_package(sol2 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
find_package(imgui REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(unofficial-nativefiledialog REQUIRED)
find_package(unofficial-concurrentqueue REQUIRED)
find_package(Lua REQUIRED)

# external libs not from vcpkg
find_package(Optick REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)	# must be found after Vulkan

add_subdirectory(source/core)
add_subdirectory(source/engine)
add_subdirectory(source/entities)
add_subdirectory(source/render)
add_subdirectory(source/editor)

add_subdirectory(source/dungeons_of_arrrgh)

add_subdirectory(source/main)

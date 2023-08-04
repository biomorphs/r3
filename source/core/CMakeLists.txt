set(CoreLib_SourceFiles
	thread_pool.h
	thread_pool.cpp
	mutex.h
	mutex.cpp
    platform.h
	platform.cpp
	profiler.h
	random.h
	random.cpp
	semaphore.h
	semaphore.cpp
)

add_library(Core ${CoreLib_SourceFiles})
target_include_directories(Core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(Core PRIVATE fmt::fmt Optick::OptickCore SDL2::SDL2) 
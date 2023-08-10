set(CoreLib_SourceFiles
	file_io.h
	file_io.cpp
	glm_headers.h
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
	time.h
	time.cpp
)

add_library(Core ${CoreLib_SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../ FILES ${CoreLib_SourceFiles})	# adds directory structure filters in VS
target_include_directories(Core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(Core PRIVATE fmt::fmt Optick::OptickCore SDL2::SDL2 glm::glm) 
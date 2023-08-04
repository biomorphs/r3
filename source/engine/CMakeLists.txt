set(EngineLib_SourceFiles
	thread_masks.h
	engine_startup.h
	engine_startup.cpp
	systems.h
	systems.cpp
)

add_library(Engine ${EngineLib_SourceFiles})
target_include_directories(Engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(Engine PRIVATE Core fmt::fmt Optick::OptickCore SDL2::SDL2) 
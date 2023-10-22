add_executable(R3 main.cpp)

# Setup default working directory
set(TARGET R3 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")

# High dpi awareness
set_target_properties(R3 PROPERTIES VS_DPI_AWARE "ON")

target_include_directories(R3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(R3 PRIVATE 
	Core
	Engine
	Entities
	Render
	Editor
	Optick::OptickCore
	sol2
	SDL2::SDL2 
	SDL2::SDL2main
	glm::glm
)


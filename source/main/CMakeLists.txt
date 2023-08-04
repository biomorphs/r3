add_executable(R3 main.cpp)

target_include_directories(R3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(R3 PRIVATE 
	Core
	Engine
	fmt::fmt
	Optick::OptickCore
	SDL2::SDL2 
	SDL2::SDL2main
)
# r3
 R3		

1. Install cmake

2. Install vcpkg
	git clone https://github.com/Microsoft/vcpkg.git
	.\vcpkg\bootstrap-vcpkg.bat
	
3.  Clone + install non-vcpkg dependencies
	git clone https://github.com/bombomby/optick.git
		optick\tools\GenerateProjects_cmake.bat
		optick\build\cmake\Optick.sln
			build + run install 
	
4. Run setup-msvc.bat
	Open build/R3.sln
	
 Goals
	most/all middleware should be hooked up with vcpkg
		glm, SDL2, Lua, Sol, Vulkan, Imgui, Json, Assimp, etc
	job system/thread pool with per-thread masks
	frame described using frame graph
	simplified systems API
	asset loading/baking
		json metadata + binary data
	vulkan renderer
		tiled light culling
		deferred PBR lighting
		multiple shadow maps
		SSAO
		debug renderer (lines)
		text rendering
# r3
 R3		
 
 ![Screenshot of the editor with a basic scene loaded](screenshots/materialoverrides.jpg)

# Setup
1. Install cmake v3.8 or higher
2. Install vcpkg
	- git clone https://github.com/Microsoft/vcpkg.git
	- run .\vcpkg\bootstrap-vcpkg.bat
3. Install Vulkan SDK from https://vulkan.lunarg.com/sdk/home (+ restart afterwards)
4. Clone + install non-vcpkg dependencies
	- git clone https://github.com/bombomby/optick.git
	- run cmake -H"." -B"build\cmake" from the optick root directory
	- open optick\build\cmake\Optick.sln (as admin!)
		- build + run install (release)
5. Run setup-msvc.bat
	Open build/R3.sln
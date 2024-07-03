#pragma once

#define R3_USE_OPTICK

// Note there is a known bug with optick gpu profiling
// the queries are not reset correctly, resulting in some warnings from validation layers
// #define R3_ENABLE_GPU_PROFILING

// Assume any macros in here are active for the current scope
#ifdef R3_USE_OPTICK
	#include <optick.h>
	#define R3_PROF_FRAME(...)		OPTICK_FRAME(__VA_ARGS__)
	#define R3_PROF_EVENT(...)		OPTICK_EVENT(__VA_ARGS__)
	#define R3_PROF_EVENT_DYN(str)	OPTICK_EVENT_DYNAMIC(str)
	#define R3_PROF_STALL(...)		OPTICK_CATEGORY(__VA_ARGS__, Optick::Category::Wait)
	#define R3_PROF_THREAD(name)	OPTICK_THREAD(name)
	#define R3_PROF_IS_ACTIVE()		Optick::IsActive()
	#define R3_PROF_SHUTDOWN()		OPTICK_SHUTDOWN()
#ifdef R3_ENABLE_GPU_PROFILING
	#define R3_PROF_GPU_COMMANDS(cmdBuffer)		OPTICK_GPU_CONTEXT(cmdBuffer)	// call after begincmdbuffer
	#define R3_PROF_GPU_EVENT(...)	OPTICK_GPU_EVENT(__VA_ARGS__)				// call any time while writing cmds
	#define R3_PROF_GPU_INIT(device,physDevice,queue,qfi,queuecount)	OPTICK_GPU_INIT_VULKAN(device,physDevice,queue,qfi,1,nullptr)
	#define R3_PROF_GPU_FLIP(swapchain)	OPTICK_GPU_FLIP(swapchain)
#else
	#define R3_PROF_GPU_COMMANDS(...)
	#define R3_PROF_GPU_EVENT(...)
	#define R3_PROF_GPU_INIT(...)
	#define R3_PROF_GPU_FLIP(...)
#endif
#else
	#define R3_PROF_FRAME(...)
	#define R3_PROF_EVENT(...)
	#define R3_PROF_EVENT_DYN(str)
	#define R3_PROF_STALL(...)
	#define R3_PROF_THREAD(name)
	#define R3_PROF_IS_ACTIVE()	false
	#define R3_PROF_SHUTDOWN()
	#define R3_PROF_GPU_COMMANDS(...)
	#define R3_PROF_GPU_EVENT(...)
	#define R3_PROF_GPU_INIT(...)
	#define R3_PROF_GPU_FLIP(...)
#endif
#pragma once

 #define R3_USE_OPTICK

// Assume any macros in here are active for the current scope
#ifdef R3_USE_OPTICK
	#include <optick.h>
	#define R3_PROF_FRAME(...)		OPTICK_FRAME(__VA_ARGS__)
	#define R3_PROF_EVENT(...)		OPTICK_EVENT(__VA_ARGS__)
	#define R3_PROF_EVENT_DYN(str) OPTICK_EVENT_DYNAMIC(str)
	#define R3_PROF_STALL(...)		OPTICK_CATEGORY(__VA_ARGS__, Optick::Category::Wait)
	#define R3_PROF_THREAD(name)	OPTICK_THREAD(name)
	#define R3_PROF_IS_ACTIVE()	Optick::IsActive()
#else
	#define R3_PROF_FRAME(...)
	#define R3_PROF_EVENT(...)
	#define R3_PROF_EVENT_DYN(str)
	#define R3_PROF_STALL(...)
	#define R3_PROF_THREAD(name)
	#define R3_PROF_IS_ACTIVE()	false
#endif
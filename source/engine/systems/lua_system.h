#pragma once

#include "engine/systems.h"
#include <memory>

namespace R3
{
	// Main entry point for any lua scripting needs
	// There is a single shared state protected by a global mutex
	class LuaSystem : public System
	{
	public:
		LuaSystem();
		virtual ~LuaSystem();
		static std::string_view GetName() { return "LuaSystem"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();

	private:
		bool RunFixedUpdateScripts();
		bool RunVariableUpdateScripts();
		struct Internals;
		std::unique_ptr<Internals> m_internals;
		bool InitialiseGlobalState();
	};
}
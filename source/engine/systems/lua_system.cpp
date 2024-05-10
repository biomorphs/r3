#include "lua_system.h"
#include "engine/components/lua_script.h"
#include "entities/systems/entity_system.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "core/mutex.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include <cassert>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace R3
{
	struct LuaSystem::Internals
	{
		Mutex m_globalStateMutex;
		sol::state m_globalState;
	};

	LuaSystem::LuaSystem()
	{
	}

	LuaSystem::~LuaSystem()
	{
	}

	void LuaSystem::RegisterTickFns()
	{
		RegisterTick("LuaSystem::RunFixedUpdateScripts", [this]() {
			return RunFixedUpdateScripts();
		});
		RegisterTick("LuaSystem::RunVariableUpdateScripts", [this]() {
			return RunVariableUpdateScripts();
		});
		RegisterTick("LuaSystem::RunGC", [this]() {
			return RunGC();
		});
	}

	bool LuaSystem::Init()
	{
		R3_PROF_EVENT();
		m_internals = std::make_unique<Internals>();
		InitialiseGlobalState();
		return true;
	}

	void LuaSystem::Shutdown()
	{
	}

	sol::protected_function LoadScriptAndGetEntrypoint(sol::state& g, std::string_view scriptPath, std::string_view entryPoint)
	{
		R3_PROF_EVENT();
		if (scriptPath.length() == 0 || entryPoint.length() == 0)
		{
			return {};
		}
		std::string scriptText;
		if (!FileIO::LoadTextFromFile(scriptPath, scriptText))
		{
			LogError("Failed to load file '{}'", scriptPath);
			return {};
		}
		else
		{
			try
			{
				g.script(scriptText, [](lua_State*, sol::protected_function_result pfr) {
					sol::error err = pfr;
					LogError("Error compiling script: \n{}", err.what());
					return pfr;	// something went wrong!
				});
				return g[entryPoint];
			}
			catch (const sol::error& err)
			{
				std::string errorText = err.what();
				LogError("Error compiling script: \n{}", errorText);
			}
		}
		return {};
	}

	bool LuaSystem::RunGC()
	{
		R3_PROF_EVENT();
		m_internals->m_globalState.collect_garbage();
		return true;
	}

	bool LuaSystem::RunFixedUpdateScripts()
	{
		R3_PROF_EVENT();
		auto forEachScriptCmp = [this](const Entities::EntityHandle& e, LuaScriptComponent& lc) {
			if (lc.m_onFixedUpdate.m_fn.valid() && lc.m_isActive)
			{
				R3_PROF_EVENT_DYN(lc.m_onFixedUpdate.m_entryPointName.c_str());
				try
				{
					sol::protected_function_result result = lc.m_onFixedUpdate.m_fn();	// todo, pass entity handle
					if (!result.valid())
					{
						sol::error err = result;
						LogError("Script error: {}", err.what());
					}
				}
				catch (const sol::error& err)
				{
					std::string errorText = err.what();
					LogError("Script Error: %s", errorText);
				}
			}
			return true;
		};
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities->GetActiveWorld())
		{
			Entities::Queries::ForEach<LuaScriptComponent>(entities->GetActiveWorld(), forEachScriptCmp);
		}
		return true;
	}

	bool LuaSystem::RunVariableUpdateScripts()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto forEachScriptCmp = [this](const Entities::EntityHandle& e, LuaScriptComponent& lc) {
			if (lc.m_needsRecompile)
			{
				lc.m_needsRecompile = false;
				lc.m_onFixedUpdate.m_fn = LoadScriptAndGetEntrypoint(m_internals->m_globalState, lc.m_onFixedUpdate.m_sourcePath, lc.m_onFixedUpdate.m_entryPointName);
				lc.m_onVariableUpdate.m_fn = LoadScriptAndGetEntrypoint(m_internals->m_globalState, lc.m_onVariableUpdate.m_sourcePath, lc.m_onVariableUpdate.m_entryPointName);
			}
			if (lc.m_onVariableUpdate.m_fn.valid() && lc.m_isActive)
			{
				R3_PROF_EVENT_DYN(lc.m_onVariableUpdate.m_entryPointName.c_str());
				try
				{
					sol::protected_function_result result = lc.m_onVariableUpdate.m_fn();	// todo, pass entity handle
					if (!result.valid())
					{
						sol::error err = result;
						LogError("Script error: {}", err.what());
					}
				}
				catch (const sol::error& err)
				{
					std::string errorText = err.what();
					LogError("Script Error: %s", errorText);
				}
			}
			return true;
		};
		if (entities->GetActiveWorld())
		{
			Entities::Queries::ForEach<LuaScriptComponent>(entities->GetActiveWorld(), forEachScriptCmp);
		}
		return true;
	}

	bool LuaSystem::InitialiseGlobalState()
	{
		R3_PROF_EVENT();
		assert(m_internals != nullptr);
		ScopedLock lock(m_internals->m_globalStateMutex);
		m_internals->m_globalState.open_libraries(sol::lib::base,
			sol::lib::math,
			sol::lib::package,
			sol::lib::os,
			sol::lib::coroutine,
			sol::lib::bit32,
			sol::lib::io,
			sol::lib::debug,
			sol::lib::table,
			sol::lib::string);
		
		return true;
	}

}
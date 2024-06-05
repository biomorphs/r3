#include "lua_system.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/components/lua_script.h"
#include "entities/systems/entity_system.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include <cassert>
#include <imgui.h>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace R3
{
	LuaSystem::LuaSystem()
	{
		m_globalState = std::make_unique<sol::state>();
		InitialiseGlobalState();
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
		RegisterTick("LuaSystem::ShowGui", [this]() {
			return ShowGui();
		});
	}

	std::string LuaToStr(const sol::object& o)
	{
		std::string result = "";
		switch (o.get_type())
		{
		case sol::type::none:
			result = "none";
			break;
		case sol::type::lua_nil:
			result = "lua_nil";
			break;
		case sol::type::string:
			result = std::format("{}", o.as<std::string>());
			break;
		case sol::type::number:
			result = std::format("{}", o.as<double>());
			break;
		case sol::type::thread:
			result = "thread";
			break;
		case sol::type::boolean:
			result = std::format("{}", o.as<bool>());
			break;
		case sol::type::function:
			result = "function";
			break;
		case sol::type::userdata:
			result = "userdata";
			break;
		case sol::type::lightuserdata:
			result = "lightuserdata";
			break;
		case sol::type::table:
			result = "table";
			break;
		case sol::type::poly:
			result = "poly";
			break;
		default:
			break;
		}
		return result;
	}

	bool LuaSystem::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Lua", [this]() {
			m_showGui = !m_showGui;
		});
		if (m_showGui)
		{
			ImGui::Begin("Lua");
			ScopedTryLock lock(m_globalStateMutex);
			if (lock.IsLocked())
			{
				for (const auto& g : *m_globalState)
				{
					const sol::object& key = g.first;
					const sol::object& val = g.second;
					std::string txt = std::format("{} - {}", LuaToStr(key), LuaToStr(val));
					ImGui::Text(txt.c_str());
				}
			}
			ImGui::End();
		}
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
		m_globalState->collect_garbage();
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
					sol::protected_function_result result = lc.m_onFixedUpdate.m_fn(e);
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

	void LuaSystem::RegisterBuiltinTypes()
	{
		m_globalState->new_usertype<glm::vec2>("vec2",
			sol::constructors<glm::vec2(), glm::vec2(float), glm::vec2(float, float), glm::vec2(glm::vec2)>()
		);
		m_globalState->new_usertype<glm::vec3>("vec3",
			sol::constructors<glm::vec3(), glm::vec3(float), glm::vec3(float, float, float), glm::vec3(glm::vec3)>()
		);
		m_globalState->new_usertype<glm::vec4>("vec4",
			sol::constructors<glm::vec4(), glm::vec4(float), glm::vec4(float, float, float, float), glm::vec4(glm::vec4)>()
		);
		m_globalState->new_usertype<glm::mat3>("mat3",
			sol::constructors<glm::mat3(), glm::mat3(glm::mat3)>()
		);
		m_globalState->new_usertype<glm::mat4>("mat4",
			sol::constructors<glm::mat4(), glm::mat4(glm::mat4)>()
		);
		m_globalState->new_usertype<glm::quat>("quat",
			sol::constructors<glm::quat(), glm::quat(glm::quat)>()
		);
		RegisterFunction("RotateQuat", [](const glm::quat& q, float angleDegrees, glm::vec3 axis) -> glm::quat {
			return glm::rotate(q, angleDegrees, axis);
		});
	}

	bool LuaSystem::RunVariableUpdateScripts()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto forEachScriptCmp = [this](const Entities::EntityHandle& e, LuaScriptComponent& lc) {
			if (lc.m_needsRecompile)
			{
				lc.m_needsRecompile = false;
				lc.m_onFixedUpdate.m_fn = LoadScriptAndGetEntrypoint(*m_globalState, lc.m_onFixedUpdate.m_sourcePath, lc.m_onFixedUpdate.m_entryPointName);
				lc.m_onVariableUpdate.m_fn = LoadScriptAndGetEntrypoint(*m_globalState, lc.m_onVariableUpdate.m_sourcePath, lc.m_onVariableUpdate.m_entryPointName);
			}
			if (lc.m_onVariableUpdate.m_fn.valid() && lc.m_isActive)
			{
				R3_PROF_EVENT_DYN(lc.m_onVariableUpdate.m_entryPointName.c_str());
				try
				{
					sol::protected_function_result result = lc.m_onVariableUpdate.m_fn(e);	// todo, pass entity handle
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
		ScopedLock lock(m_globalStateMutex);
		m_globalState->open_libraries(sol::lib::base,
			sol::lib::math,
			sol::lib::package,
			sol::lib::os,
			sol::lib::coroutine,
			sol::lib::bit32,
			sol::lib::io,
			sol::lib::debug,
			sol::lib::table,
			sol::lib::string);

		RegisterBuiltinTypes();

		return true;
	}

}
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
#include <sol/sol.hpp>

namespace R3
{
	// Override the default lua print function with our own to capture any prints
	static int myLuaPrintOverride(lua_State* ls)
	{
		int argCount = lua_gettop(ls);	// num parameters
		std::string finalString;		// append all params to one line
		for (int i = 1; i <= argCount; i++) // need to use lua style indexing
		{
			size_t strLength = 0;
			const char* paramStr = luaL_tolstring(ls, i, &strLength);
			finalString += paramStr;
			lua_pop(ls, 1); // remove the string
		}
		LogInfo("Lua: {}", finalString);
		return 0;
	}

	// Global table used for function overrides
	static const struct luaL_Reg c_myLuaOverrideFns[] = {
		 {"print", myLuaPrintOverride},
		{NULL, NULL} /* end of array */
	};

	LuaSystem::LuaSystem()
	{
		m_globalState = std::make_unique<sol::state>();
		InitialiseGlobalState();
	}

	LuaSystem::~LuaSystem()
	{
	}

	void LuaSystem::SetWorldScriptsActive(bool v)
	{
		m_runActiveWorldScripts = v;
	}

	bool LuaSystem::GetWorldScriptsActive()
	{
		return m_runActiveWorldScripts;
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

	bool IsSystemTable(std::string_view key)
	{
		std::array<const char*, 11> systemTables = {
			"_G",
			"math",
			"package",
			"os",
			"coroutine",
			"bit32",
			"io",
			"debug",
			"table",
			"string",
			"base"
		};
		for (int i = 0; i < systemTables.size(); ++i)
		{
			if (key == systemTables[i])
			{
				return true;
			}
		}
		return false;
	}

	void ShowTableContents(const sol::table& table, std::string_view name)
	{
		std::vector<std::string> functions;
		std::vector<std::string> userDatas;
		std::vector<std::string> systemTables;
		std::vector<std::string> userTables;
		for (const auto& g : table)
		{
			const sol::object& key = g.first;
			const sol::object& val = g.second;
			auto keyStr = LuaToStr(key);
			auto valStr = LuaToStr(val);
			if (valStr == "function")
			{
				functions.push_back(keyStr);
			}
			else if (valStr == "userdata")
			{
				userDatas.push_back(keyStr);
			}
			else if (valStr == "table")
			{
				if (IsSystemTable(keyStr))
				{
					systemTables.push_back(keyStr);
				}
				else
				{
					userTables.push_back(keyStr);
				}
			}
			else
			{
				std::string txt = std::format("{} - {}", keyStr, valStr);
				ImGui::Text(txt.c_str());
			}
		}
		if (userTables.size() > 0 && ImGui::TreeNode("User Tables"))
		{
			std::sort(userTables.begin(), userTables.end());
			for (const auto& it : userTables)
			{
				if (ImGui::TreeNode(it.c_str()))
				{
					ShowTableContents(table.get<sol::table>(it), it);
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (systemTables.size() > 0 && ImGui::TreeNode("System Tables"))
		{
			std::sort(systemTables.begin(), systemTables.end());
			for (const auto& it : systemTables)
			{
				if (ImGui::TreeNode(it.c_str()))
				{
					ShowTableContents(table.get<sol::table>(it), it);
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (functions.size() > 0 && ImGui::TreeNode("Functions"))
		{
			std::sort(functions.begin(), functions.end());
			for (const auto& it : functions)
			{
				ImGui::Text(it.data());
			}
			ImGui::TreePop();
		}
		if (userDatas.size() > 0 && ImGui::TreeNode("User Datas"))
		{
			std::sort(userDatas.begin(), userDatas.end());
			for (const auto& it : userDatas)
			{
				ImGui::Text(it.data());
			}
			ImGui::TreePop();
		}
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
			ImGui::Text(m_runActiveWorldScripts ? "World Scripts Active" : "World Scripts Deactivated");
			ScopedTryLock lock(m_globalStateMutex);
			if (lock.IsLocked())
			{
				ShowTableContents(m_globalState->globals(), "Globals");
			}
			ImGui::End();
		}
		return true;
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
		ScopedLock lockState(m_globalStateMutex);
		m_globalState->collect_garbage();
		return true;
	}

	bool LuaSystem::RunFixedUpdateScripts()
	{
		R3_PROF_EVENT();
		if (m_runActiveWorldScripts)
		{
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
				ScopedLock lockState(m_globalStateMutex);
				Entities::Queries::ForEach<LuaScriptComponent>(entities->GetActiveWorld(), forEachScriptCmp);
			}
		}
		return true;
	}

	void LuaSystem::RegisterBuiltinTypes()
	{
		m_globalState->new_usertype<glm::ivec2>("ivec2",
			sol::constructors<glm::ivec2(), glm::ivec2(int32_t), glm::ivec2(int32_t, int32_t), glm::ivec2(glm::ivec2)>(),
			"x", &glm::ivec2::x,
			"y", &glm::ivec2::y
		);
		m_globalState->new_usertype<glm::uvec2>("uvec2",
			sol::constructors<glm::uvec2(), glm::uvec2(uint32_t), glm::uvec2(uint32_t, uint32_t), glm::uvec2(glm::uvec2)>(),
			"x", &glm::uvec2::x,
			"y", &glm::uvec2::y
		);
		m_globalState->new_usertype<glm::vec2>("vec2",
			sol::constructors<glm::vec2(), glm::vec2(float), glm::vec2(float, float), glm::vec2(glm::vec2)>(),
			"x", &glm::vec2::x,
			"y", &glm::vec2::y
		);
		m_globalState->new_usertype<glm::vec3>("vec3",
			sol::constructors<glm::vec3(), glm::vec3(float), glm::vec3(float, float, float), glm::vec3(glm::vec3)>(),
			"x", &glm::vec3::x,
			"y", &glm::vec3::y,
			"z", &glm::vec3::z
		);
		m_globalState->new_usertype<glm::vec4>("vec4",
			sol::constructors<glm::vec4(), glm::vec4(float), glm::vec4(float, float, float, float), glm::vec4(glm::vec4)>(),
			"x", &glm::vec4::x,
			"y", &glm::vec4::y,
			"z", &glm::vec4::z,
			"w", &glm::vec4::w
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
		Blackboard::RegisterScripts(*this);
		RegisterFunction("RotateQuat", [](const glm::quat& q, float angleDegrees, glm::vec3 axis) -> glm::quat {
			return glm::rotate(q, angleDegrees, axis);
		});
		RegisterFunction("Vec3Length", [](const glm::vec3& v) -> float {
			return glm::length(v);
		});
	}

	bool LuaSystem::RunVariableUpdateScripts()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto forEachScriptCmp = [this](const Entities::EntityHandle& e, LuaScriptComponent& lc) {
			if (lc.m_needsInputPopulate)
			{
				lc.m_populateInputs.m_fn = LoadScriptAndGetEntrypoint(*m_globalState, lc.m_populateInputs.m_sourcePath, lc.m_populateInputs.m_entryPointName);
				if (lc.m_populateInputs.m_fn)
				{
					R3_PROF_EVENT_DYN(lc.m_populateInputs.m_entryPointName.c_str());
					try
					{
						lc.m_inputParams.Reset();
						sol::protected_function_result result = lc.m_populateInputs.m_fn(&lc);
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
				lc.m_needsInputPopulate = false;
			}
			if (m_runActiveWorldScripts)
			{
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
						sol::protected_function_result result = lc.m_onVariableUpdate.m_fn(e);
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
			}
			return true;
		};
		if (entities->GetActiveWorld())
		{
			ScopedLock lockState(m_globalStateMutex);
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
		
		// Override any globals in _G
		lua_getglobal(m_globalState->lua_state(), "_G");
		luaL_setfuncs(m_globalState->lua_state(), c_myLuaOverrideFns, 0);  // for Lua versions 5.2 or greater
		lua_pop(m_globalState->lua_state(), 1);

		return true;
	}

}
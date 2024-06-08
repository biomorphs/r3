#pragma once

#include "engine/systems.h"
#include "core/mutex.h"
#include "core/log.h"
#include <memory>
#include <sol/sol.hpp>		// if you are using this system, you are using sol, no point hiding it

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
		virtual void Shutdown();

		void SetWorldScriptsActive(bool v);

		// Register a type with internals states using sol::new_usertype<>
		// args can contain constructors (via sol::constructors), member functions, member variables, etc
		// Typical use
		// RegisterType<vec2>("vec2", sol::constructors<vec2(), vec2(float), vec2(float,float)>>, "x", &vec2::x, "y", &vec2::y, "Length", &vec2::Length)
		template<class UserType, class... SolArgs>
		void RegisterType(std::string_view typeName, SolArgs&&... args);

		// Add a member/member function to an existing type
		template<class UserType, class FnOrMember>
		bool AddTypeMember(std::string_view typeName, std::string_view memberName, FnOrMember fn);

		// Register a global function in a namespace
		template<class Fn>
		void RegisterFunction(std::string_view name, Fn fn, std::string_view nameSpace = "R3");

	private:
		bool ShowGui();
		void RegisterBuiltinTypes();
		bool RunGC();
		bool RunFixedUpdateScripts();
		bool RunVariableUpdateScripts();
		bool InitialiseGlobalState();
		bool m_runActiveWorldScripts = false;
		bool m_showGui = false;
		Mutex m_globalStateMutex;
		std::unique_ptr<sol::state> m_globalState;
	};

	template<class UserType, class... SolArgs>
	inline void LuaSystem::RegisterType(std::string_view typeName, SolArgs&&... args)
	{
		ScopedLock lockState(m_globalStateMutex);
		try
		{
			m_globalState->new_usertype<UserType>(typeName, args...);
		}
		catch (const sol::error& err)
		{
			LogError("Error while registering scripted type '{}'!\n\t{}", typeName, err.what());
		}
	}

	template<class UserType, class FnOrMember>
	inline bool LuaSystem::AddTypeMember(std::string_view typeName, std::string_view memberName, FnOrMember fn)
	{
		ScopedLock lockState(m_globalStateMutex);
		try
		{
			sol::usertype<UserType> userTable = m_globalState->get<sol::usertype<UserType>>(typeName);
			userTable[memberName] = fn;
		}
		catch (const sol::error& err)
		{
			LogError("Error while adding script member '{}' to type '{}'!\n\t{}", memberName, typeName, err.what());
			return false;
		}

		return true;
	}

	template<class Fn>
	inline void LuaSystem::RegisterFunction(std::string_view name, Fn fn, std::string_view nameSpace)
	{
		ScopedLock lockState(m_globalStateMutex);
		try
		{
			auto tbl = (*m_globalState)[nameSpace].get_or_create<sol::table>();
			tbl[name] = fn;
		}
		catch (const sol::error& err)
		{
			LogError("Error while registering function '{}' in namespace '{}'!\n\t{}", name, nameSpace, err.what());
		}
	}
}
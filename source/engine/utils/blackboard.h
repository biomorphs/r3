#pragma once
#include "entities/entity_handle.h"
#include "core/glm_headers.h"
#include <string>
#include <unordered_map>

namespace R3
{
	class Blackboard
	{
	public:
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(class JsonSerialiser&);
		void Inspect(class ValueInspector&);

		void Reset();

		using IntVec2s = std::unordered_map<std::string, glm::ivec2>;
		inline bool AddIntVec2(std::string key, glm::ivec2 value);	// only adds if key doesnt exist already
		inline glm::ivec2 GetIntVec2(std::string key, glm::ivec2 defaultValue);

		using Ints = std::unordered_map<std::string, int>;
		inline bool AddInt(std::string key, int value);	// only adds if key doesnt exist already
		inline int GetInt(std::string key, int defaultValue);

		using Floats = std::unordered_map<std::string, float>;
		inline bool AddFloat(std::string key, float value);	// only adds if key doesnt exist already
		inline float GetFloat(std::string key, float defaultValue);

	private:
		IntVec2s m_intVec2s;
		Ints m_ints;
		Floats m_floats;
	};

	inline bool R3::Blackboard::AddIntVec2(std::string key, glm::ivec2 value)
	{
		auto insertResult = m_intVec2s.insert({ key,value });
		return insertResult.second;	// was the value inserted?
	}

	inline glm::ivec2 R3::Blackboard::GetIntVec2(std::string key, glm::ivec2 defaultValue)
	{
		auto foundVal = m_intVec2s.find(key);
		if (foundVal != m_intVec2s.end())
		{
			return foundVal->second;
		}
		return defaultValue;
	}

	inline bool R3::Blackboard::AddInt(std::string key, int value)
	{
		auto insertResult = m_ints.insert({ key,value });
		return insertResult.second;	// was the value inserted?
	}

	inline int R3::Blackboard::GetInt(std::string key, int defaultValue)
	{
		auto foundVal = m_ints.find(key);
		if (foundVal != m_ints.end())
		{
			return foundVal->second;
		}
		return defaultValue;
	}

	inline bool R3::Blackboard::AddFloat(std::string key, float value)
	{
		auto insertResult = m_floats.insert({ key,value });
		return insertResult.second;	// was the value inserted?
	}

	inline float R3::Blackboard::GetFloat(std::string key, float defaultValue)
	{
		auto foundVal = m_floats.find(key);
		if (foundVal != m_floats.end())
		{
			return foundVal->second;
		}
		return defaultValue;
	}
}
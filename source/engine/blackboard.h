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
		inline bool TryAddIntVec2(std::string key, glm::ivec2 value);	// only adds if no value exists already
		inline glm::ivec2 GetIntVec2(std::string key, glm::ivec2 defaultValue);

	private:
		IntVec2s m_intVec2s;
	};

	inline bool R3::Blackboard::TryAddIntVec2(std::string key, glm::ivec2 value)
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
}
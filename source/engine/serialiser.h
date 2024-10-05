#pragma once
#include "core/log.h"
#include "core/glm_headers.h"
#include <nlohmann/json.hpp>
#include <string_view>
#include <type_traits>
#include <memory>
#include <vector>
#include <unordered_map>

namespace R3
{
	// serialiser knows about 
	// ints, floats, bools, strings, vec2, vec3, vec4, quat
	// objects (that have a Serialise function)
	// vectors of the above
	// unordered_maps of the above (within reason)

	// To serialise a custom type, specialise the SerialiseJson template in the R3 namespace
	// Or you can add a SerialiseJson(JsonSerialiser&) member function to your objects
	// call TypeName() in serialiser to append a type name to data that will be tested on load

	template<class T> void SerialiseJson(T&, class JsonSerialiser&)
	{
		assert(!"You need to define a SerialiseJson function!");
	}

	template <typename T>		// SFINAE trick to detect Serialise member fn
	class HasSerialiser
	{
		typedef char one;
		typedef long two;
		template <typename C> static one test(decltype(&C::SerialiseJson));
		template <typename C> static two test(...);
	public:
		enum { value = sizeof(test<T>(0)) == sizeof(char) };
	};

	class JsonSerialiser
	{
	public:
		enum Mode {
			Read, Write
		};
		std::string c_str() { return m_json.dump(2); }
		void LoadFromString(std::string_view jsonData);
		nlohmann::json& GetJson() { return m_json; }
		const nlohmann::json& GetJson() const { return m_json; }
		Mode GetMode() { return m_mode; }

		JsonSerialiser(Mode m) : m_mode(m) { }
		JsonSerialiser(Mode m, const nlohmann::json& j) : m_mode(m), m_json(j) {}
		JsonSerialiser(Mode m, nlohmann::json&& j) : m_mode(m), m_json(std::move(j)) {}

		// (optional-ish) call this at the start of a custom serialiser
		// the name will be written along with the class data + validated when reading
		void TypeName(std::string_view name)
		{
			const char* typeNameId = "_tnid";
			if (m_mode == Mode::Read)
			{
				std::string_view readName = m_json[typeNameId];
				if (readName != name)
				{
					LogError("Type mismatch in data (expected type {}, actual type {})!", name, readName);
					assert(!"Type mismatch!");
				}
			}
			else
			{
				m_json[typeNameId] = name;
			}
		}

		// Main serialisation operator
		// this is getting ridiculous, refactor asap
		template<class ValueType>
		void operator()(std::string_view name, ValueType& t)
		{
			constexpr bool isInt = std::is_integral<ValueType>::value;
			constexpr bool isFloat = std::is_floating_point<ValueType>::value;
			constexpr bool isString = (std::is_same<ValueType, std::string>::value);
			constexpr bool hasSerialiserMember = HasSerialiser<ValueType>::value;
			try
			{
				if (m_mode == Mode::Read)
				{
					if constexpr (isInt || isFloat || isString)
					{
						t = m_json[name];
					}
					else
					{
						JsonSerialiser js(m_mode, m_json[name]);
						if constexpr (hasSerialiserMember)
						{
							t.SerialiseJson(js);
						}
						else
						{
							SerialiseJson(t, js);
						}
					}
				}
				else
				{
					if constexpr (isInt || isFloat || isString)
					{
						m_json[name] = t;
					}
					else
					{
						JsonSerialiser js(m_mode);
						if constexpr (hasSerialiserMember)
						{
							t.SerialiseJson(js);
						}
						else
						{
							SerialiseJson(t, js);
						}
						m_json[name] = std::move(js.m_json);
					}
				}
			}
			catch (std::exception e)
			{
				LogError("Failed to serialise {} - ", name, e.what());
			}
		}

		// handles vectors of things
		template<class ValueType>
		void operator()(std::string_view name, std::vector<ValueType>& v)
		{
			constexpr bool isInt = std::is_integral<ValueType>::value;
			constexpr bool isFloat = std::is_floating_point<ValueType>::value;
			constexpr bool isString = (std::is_same<ValueType, std::string>::value);
			constexpr bool hasSerialiserMember = HasSerialiser<ValueType>::value;
			try
			{
				if (m_mode == Mode::Read)
				{
					std::vector<json> listJson = m_json[name];
					for (int i = 0; i < listJson.size(); ++i)
					{
						if constexpr (isInt || isFloat || isString)
						{
							v.emplace_back(listJson[i]);
						}
						else
						{
							JsonSerialiser js(m_mode, std::move(listJson[i]));
							ValueType newVal;
							if constexpr (hasSerialiserMember)
							{
								newVal.SerialiseJson(js);
							}
							else
							{
								SerialiseJson(newVal, js);
							}
							v.emplace_back(std::move(newVal));
						}
					}
				}
				else
				{
					std::vector<json> listJson;
					for (int i = 0; i < v.size(); ++i)
					{
						if constexpr (isInt || isFloat || isString)
						{
							listJson.push_back(v[i]);
						}
						else
						{
							JsonSerialiser js(m_mode);
							if constexpr (hasSerialiserMember)
							{
								v[i].SerialiseJson(js);
							}
							else
							{
								SerialiseJson(v[i], js);
							}
							listJson.push_back(std::move(js.m_json));
						}
					}
					m_json[name] = listJson;
				}
			}
			catch (std::exception e)
			{
				LogError("Failed to serialise {} - {}", name, e.what());
			}
		}

		// handles maps of things
		template<class KeyType, class ValueType>
		void operator()(std::string_view name, std::unordered_map<KeyType, ValueType>& v)
		{
			constexpr bool keyIsInt = std::is_integral<KeyType>::value;
			constexpr bool keyIsFloat = std::is_floating_point<KeyType>::value;
			constexpr bool keyIsString = (std::is_same<KeyType, std::string>::value);
			constexpr bool keyHasSerialiserMember = HasSerialiser<KeyType>::value;
			constexpr bool valueIsInt = std::is_integral<ValueType>::value;
			constexpr bool valueIsFloat = std::is_floating_point<ValueType>::value;
			constexpr bool valueIsString = (std::is_same<ValueType, std::string>::value);
			constexpr bool valueHasSerialiserMember = HasSerialiser<ValueType>::value;
			if (m_mode == Mode::Write)
			{
				std::vector<json> listJson;
				for (auto& it : v)
				{
					json itJson;
					if constexpr (keyIsInt || keyIsFloat || keyIsString)
					{
						itJson["Key"] = it.first;
					}
					else
					{
						KeyType keyVal = it.first;	// we need a non-const ref to the key, so make a temp copy
						JsonSerialiser keyJs(m_mode);
						if constexpr (keyHasSerialiserMember)
						{
							keyVal.SerialiseJson(keyJs);
						}
						else
						{
							SerialiseJson(keyVal, keyJs);
						}
						itJson["Key"] = keyJs.m_json;
					}
					if constexpr (valueIsInt || valueIsFloat || valueIsString)
					{
						itJson["Value"] = it.second;
					}
					else
					{
						JsonSerialiser valueJs(m_mode);
						if constexpr (valueHasSerialiserMember)
						{
							it.second.SerialiseJson(valueJs);
						}
						else
						{
							SerialiseJson(it.second, valueJs);
						}
						itJson["Value"] = valueJs.m_json;
					}
					listJson.push_back(itJson);
				}
				m_json[name] = listJson;
			}
			else
			{
				std::vector<json> listJson = m_json[name];
				for (int i = 0; i < listJson.size(); ++i)
				{
					KeyType key;
					if constexpr (keyIsInt || keyIsFloat || keyIsString)
					{
						key = listJson[i]["Key"];
					}
					else
					{
						JsonSerialiser js(m_mode, listJson[i]["Key"]);
						if constexpr (keyHasSerialiserMember)
						{
							key.SerialiseJson(js);
						}
						else
						{
							SerialiseJson(key, js);
						}
					}
					ValueType value;
					if constexpr (valueIsInt || valueIsFloat || valueIsString)
					{
						value = listJson[i]["Value"];
					}
					else
					{
						JsonSerialiser js(m_mode, listJson[i]["Value"]);
						if constexpr (valueHasSerialiserMember)
						{
							value.SerialiseJson(js);
						}
						else
						{
							SerialiseJson(value, js);
						}
					}
					v[key] = value;
				}
			}
		}
	private:
		using json = nlohmann::json;
		Mode m_mode = Write;
		json m_json;
	};

	template<> inline void SerialiseJson(glm::vec2& t, JsonSerialiser& s)
	{
		s("X", t.x);
		s("Y", t.y);
	}

	template<> inline void SerialiseJson(glm::uvec2& t, JsonSerialiser& s)
	{
		s("X", t.x);
		s("Y", t.y);
	}

	template<> inline void SerialiseJson(glm::ivec2& t, JsonSerialiser& s)
	{
		s("X", t.x);
		s("Y", t.y);
	}

	template<> inline void SerialiseJson(glm::vec3& t, JsonSerialiser& s)
	{
		s("X", t.x);
		s("Y", t.y);
		s("Z", t.z);
	}

	template<> inline void SerialiseJson(glm::vec4& t, JsonSerialiser& s)
	{
		s("X", t.x);
		s("Y", t.y);
		s("Z", t.z);
		s("W", t.w);
	}

	template<> inline void SerialiseJson(glm::quat& t, JsonSerialiser& s)
	{
		s("X", t.x);
		s("Y", t.y);
		s("Z", t.z);
		s("W", t.w);
	}
}
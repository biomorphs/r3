#pragma once
#include "core/log.h"
#include <nlohmann/json.hpp>
#include <string_view>
#include <type_traits>
#include <memory>
#include <vector>

namespace R3
{
	// serialiser knows about 
	// ints, floats, bools, strings
	// objects (that have a Serialise function)
	// vectors of the above

	// To serialise a custom type, specialise the SerialiseJson template in the R3 namespace
	// call TypeName() in serialiser to append a type name to data that will be tested on load
	template<class T> void SerialiseJson(T&, class JsonSerialiser&)
	{
		assert(!"You need to define a SerialiseJson function!");
	}

	class JsonSerialiser
	{
	public:
		enum Mode {
			Read, Write
		};
		std::string c_str() { return m_json.dump(2); }
		void LoadFromString(std::string_view jsonData);

		JsonSerialiser(Mode m) : m_mode(m)
		{
		}

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
		template<class ValueType>
		void operator()(std::string_view name, ValueType& t)
		{
			constexpr bool isInt = std::is_integral<ValueType>::value;
			constexpr bool isFloat = std::is_floating_point<ValueType>::value;
			constexpr bool isString = (std::is_same<ValueType, std::string>::value);
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
						json objectJson = m_json[name];
						JsonSerialiser js(m_mode, std::move(objectJson));
						SerialiseJson(t, js);
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
						SerialiseJson(t, js);
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
							SerialiseJson(newVal,js);
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
							SerialiseJson(v[i], js);
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
	private:
		using json = nlohmann::json;
		JsonSerialiser(Mode m, json&& j) : m_mode(m), m_json(std::move(j)) {}
		Mode m_mode = Write;
		json m_json;
	};
}
#pragma once
#include "core/log.h"
#include <nlohmann/json.hpp>
#include <string_view>
#include <type_traits>
#include <memory>
#include <vector>

namespace R3
{
	class JsonSerialiser;
}


namespace R3
{
	// serialiser knows about 
	// ints, floats, bools, strings
	// objects (that have a Serialise function)
	// arrays of the above
	// key->value containers of the above (within reason, strings/ints as keys only)

	// To serialise a custom type, specialise the SerialiseJson template in the R3 namespace
	template<class T> void SerialiseJson(T&, R3::JsonSerialiser&)
	{
		assert(!"You need to define a SerialiseJson function!");
	}

	class JsonSerialiser;
	class JsonSerialiser
	{
	public:
		enum Mode {
			Read, Write
		};
		std::string c_str() { return m_json.dump(2); }

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

		template<class ValueType>
		void operator()(std::string_view name, ValueType& t)
		{
			constexpr bool isInt = std::is_integral<ValueType>::value;
			constexpr bool isFloat = std::is_floating_point<ValueType>::value;
			constexpr bool isString = (std::is_same<ValueType, std::string>::value);
			if (m_mode == Mode::Read)
			{
				if constexpr (isInt || isFloat || isString)
				{
					t = m_json[name];
				}
				else
				{
					SerialiseJson(t, *this);
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
					// this is gross
					json currentJson = std::move(m_json);
					m_json = {};
					SerialiseJson(t, *this);
					currentJson[name] = m_json;
					m_json = std::move(currentJson);
				}
			}
		}

		// handles vectors of things
		template<class ValueType>
		void operator()(std::string_view name, std::vector<ValueType>& v)
		{
			LogInfo("Printing a vector of ValueType");
		}
	private:
		using json = nlohmann::json;
		Mode m_mode = Write;
		json m_json;
	};
}
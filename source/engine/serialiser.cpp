#include "serialiser.h"

namespace R3
{
	void JsonSerialiser::LoadFromString(std::string_view jsonData)
	{
		m_json = json::parse(jsonData);
	}
}
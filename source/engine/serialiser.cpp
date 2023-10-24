#include "serialiser.h"
#include "core/profiler.h"

namespace R3
{
	void JsonSerialiser::LoadFromString(std::string_view jsonData)
	{
		R3_PROF_EVENT();
		m_json = json::parse(jsonData);
	}
}
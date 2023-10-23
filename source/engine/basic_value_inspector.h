#pragma once

#include "value_inspector.h"

namespace R3
{
	class BasicValueInspector : public ValueInspector
	{
	public:
		virtual bool Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv);
		virtual bool InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn);
	};
}
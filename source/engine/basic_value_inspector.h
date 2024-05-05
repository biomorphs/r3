#pragma once

#include "value_inspector.h"

namespace R3
{
	class BasicValueInspector : public ValueInspector
	{
	public:
		virtual bool Inspect(std::string label, bool currentValue, std::function<void(bool)> setFn);
		virtual bool Inspect(std::string_view label, std::string_view currentValue, std::function<void(std::string)> setFn);
		virtual bool Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv);
		virtual bool Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv);
		virtual bool Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv);
		virtual bool InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn);
		virtual bool InspectColour(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn);
		virtual bool InspectFile(std::string_view label, std::string_view path, std::string_view filter, std::function<void(std::string_view)> setFn);
		virtual bool InspectEntity(std::string_view label, Entities::EntityHandle current, std::function<void(Entities::EntityHandle)> setFn);
	};
}
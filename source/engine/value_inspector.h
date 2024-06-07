#pragma once

#include "core/glm_headers.h"
#include <string_view>
#include <functional>

namespace R3
{
	namespace Entities
	{
		class EntityHandle;
		class World;
	}
	struct TextureHandle;

	// Base interface used for inspecting/modifying values in the ui
	// Try to use one of these wherever possible to ensure a unified look/feel
	// This is also the easiest way to add undo/redo
	// All follow same basic pattern of inspect(label, currentVal, setValueFunction, optional params)
	class ValueInspector
	{
	public:
		virtual ~ValueInspector() {}
		virtual bool Inspect(std::string label, bool currentValue, std::function<void(bool)> newVal) = 0;
		virtual bool Inspect(std::string_view label, std::string_view currentValue, std::function<void(std::string)> newValue) = 0;
		virtual bool Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv) = 0;
		virtual bool Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv) = 0;
		virtual bool Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv) = 0;
		virtual bool InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn) = 0;
		virtual bool InspectColour(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn) = 0;
		virtual bool InspectFile(std::string_view label, std::string_view path, std::string_view filter, std::function<void(std::string_view)> setFn) = 0;
		virtual bool InspectEntity(std::string_view label, Entities::EntityHandle current, Entities::World* w, std::function<void(Entities::EntityHandle)> setFn) = 0;
		virtual bool InspectTexture(std::string_view label, TextureHandle current, std::function<void(TextureHandle)> setFn, glm::ivec2 dims = glm::ivec2(128,128)) = 0;
	};
}
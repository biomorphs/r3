#pragma once 

#include "value_inspector.h"
#include <memory>
#include <functional>

namespace R3
{
	// A 'proxy' inspector that owns another inspector + fires callbacks if any values are modified
	class ReactiveValueInspector : public ValueInspector
	{
	public:
		ReactiveValueInspector(std::unique_ptr<ValueInspector>&& child) :
			m_child(std::move(child))
		{
		}
		virtual ~ReactiveValueInspector() = default;
		ReactiveValueInspector(ReactiveValueInspector&&) = default;
		ReactiveValueInspector(const ReactiveValueInspector&) = delete;

		using OnValueChangedFn = std::function<void()>;
		void SetOnValueChange(OnValueChangedFn fn);

		virtual void SetModified();
		virtual bool Inspect(std::string label, bool currentValue, std::function<void(bool)> setFn);
		virtual bool Inspect(std::string label, Tag currentValue, std::function<void(Tag)> newVal);
		virtual bool Inspect(std::string_view label, std::string_view currentValue, std::function<void(std::string)> newValue);
		virtual bool Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv);
		virtual bool Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv);
		virtual bool Inspect(std::string_view label, glm::uvec2 currentValue, std::function<void(glm::uvec2)> setFn, glm::uvec2 minv, glm::uvec2 maxv);
		virtual bool Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv);
		virtual bool Inspect(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn, glm::vec4 minv, glm::vec4 maxv);
		virtual bool InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn);
		virtual bool InspectColour(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn);
		virtual bool InspectFile(std::string_view label, std::string_view path, std::function<void(std::string_view)> setFn, const FileDialogFilter* filters, size_t filterCount);
		virtual bool InspectEntity(std::string_view label, Entities::EntityHandle current, Entities::World* w, std::function<void(Entities::EntityHandle)> setFn);
		virtual bool InspectTexture(std::string_view label, TextureHandle current, std::function<void(TextureHandle)> setFn, glm::ivec2 dims);
		virtual bool InspectEnum(std::string_view label, int currentValue, std::function<void(int)> setFn, const std::string_view options[], int optionCount);

	private:
		std::unique_ptr<ValueInspector> m_child;
		OnValueChangedFn m_onChanged;
	};
}
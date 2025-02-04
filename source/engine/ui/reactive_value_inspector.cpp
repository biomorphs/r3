#include "reactive_value_inspector.h"
#include "engine/utils/tag.h"
#include "engine/systems/texture_system.h"
#include "entities/entity_handle.h"

namespace R3
{
	void ReactiveValueInspector::SetOnValueChange(OnValueChangedFn fn)
	{
		m_onChanged = fn;
	}

	void ReactiveValueInspector::SetModified()
	{
		m_onChanged();
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, bool currentValue, std::function<void(bool)> setFn)
	{
		bool modified = m_child->Inspect(label, currentValue, setFn);
		if(modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, Tag currentValue, std::function<void(Tag)> newVal)
	{
		bool modified = m_child->Inspect(label, currentValue, newVal);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, std::string_view currentValue, std::function<void(std::string)> newValue)
	{
		bool modified = m_child->Inspect(label, currentValue, newValue);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv)
	{
		bool modified = m_child->Inspect(label, currentValue, setFn, step, minv, maxv);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv)
	{
		bool modified = m_child->Inspect(label, currentValue, setFn, step, minv, maxv);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, glm::uvec2 currentValue, std::function<void(glm::uvec2)> setFn, glm::uvec2 minv, glm::uvec2 maxv)
	{
		bool modified = m_child->Inspect(label, currentValue, setFn, minv, maxv);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv)
	{
		bool modified = m_child->Inspect(label, currentValue, setFn, minv, maxv);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::Inspect(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn, glm::vec4 minv, glm::vec4 maxv)
	{
		bool modified = m_child->Inspect(label, currentValue, setFn, minv, maxv);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn)
	{
		bool modified = m_child->InspectColour(label, currentValue, setFn);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::InspectColour(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn)
	{
		bool modified = m_child->InspectColour(label, currentValue, setFn);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::InspectFile(std::string_view label, std::string_view path, std::function<void(std::string_view)> setFn, const FileDialogFilter* filters, size_t filterCount)
	{
		bool modified = m_child->InspectFile(label, path, setFn, filters, filterCount);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::InspectEntity(std::string_view label, Entities::EntityHandle current, Entities::World* w, std::function<void(Entities::EntityHandle)> setFn)
	{
		bool modified = m_child->InspectEntity(label, current, w, setFn);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::InspectTexture(std::string_view label, TextureHandle current, std::function<void(TextureHandle)> setFn, glm::ivec2 dims)
	{
		bool modified = m_child->InspectTexture(label, current, setFn, dims);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}

	bool ReactiveValueInspector::InspectEnum(std::string_view label, int currentValue, std::function<void(int)> setFn, const std::string_view options[], int optionCount)
	{
		bool modified = m_child->InspectEnum(label, currentValue, setFn, options, optionCount);
		if (modified)
		{
			m_onChanged();
		}
		return modified;
	}
}
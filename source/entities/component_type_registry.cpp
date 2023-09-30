#include "component_type_registry.h"
#include <cassert>

namespace R3
{
namespace Entities
{
	ComponentTypeRegistry& ComponentTypeRegistry::GetInstance()
	{
		static ComponentTypeRegistry s_instance;
		return s_instance;
	}

	uint32_t ComponentTypeRegistry::Register(std::string_view typeName)
	{
		auto found = std::find_if(m_allTypes.begin(), m_allTypes.end(), [typeName](const ComponentTypeRecord& r) {
			return r.m_name == typeName;
		});
		assert(found == m_allTypes.end());
		if (found == m_allTypes.end())
		{
			ComponentTypeRecord newTypeRecord;
			newTypeRecord.m_name = typeName;
			newTypeRecord.m_dynamicIndex = static_cast<uint32_t>(m_allTypes.size());
			m_allTypes.emplace_back(std::move(newTypeRecord));
			return static_cast<uint32_t>(m_allTypes.size() - 1);
		}
		return -1;
	}

	uint32_t ComponentTypeRegistry::GetTypeIndex(std::string_view typeName) const
	{
		auto found = std::find_if(m_allTypes.begin(), m_allTypes.end(), [typeName](const ComponentTypeRecord& r) {
			return r.m_name == typeName;
		});
		if (found != m_allTypes.end())
		{
			return static_cast<uint32_t>(std::distance(m_allTypes.begin(), found));
		}
		return -1;
	}

	void ComponentTypeRegistry::SetInspector(std::string_view typeName, InspectorFn fn)
	{
		auto found = std::find_if(m_allTypes.begin(), m_allTypes.end(), [typeName](const ComponentTypeRecord& r) {
			return r.m_name == typeName;
			});
		assert(found != m_allTypes.end());
		if (found != m_allTypes.end())
		{
			found->m_inspectFn = std::move(fn);
		}
	}
	void ComponentTypeRegistry::SetStorageFactory(std::string_view typeName, StorageFn fn)
	{
		auto found = std::find_if(m_allTypes.begin(), m_allTypes.end(), [typeName](const ComponentTypeRecord& r) {
			return r.m_name == typeName;
			});
		assert(found != m_allTypes.end());
		if (found != m_allTypes.end())
		{
			found->m_storageFactory = std::move(fn);
		}
	}
}
}

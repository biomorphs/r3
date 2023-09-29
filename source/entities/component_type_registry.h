#pragma once
#include <string_view>
#include <vector>
#include <functional>
#include <memory>

namespace R3
{
namespace Entities
{
	// Singleton that tracks all component types + assigns them a runtime unique index
	class ComponentStorage;
	class World;
	class ComponentTypeRegistry
	{
	public:
		static ComponentTypeRegistry& GetInstance();

		void Register(std::string_view typeName);
		uint32_t GetTypeIndex(std::string_view typeName) const;	// returns -1 if no type found

		using InspectorFn = std::function<void()>;
		void SetInspector(std::string_view typeName, InspectorFn fn);

		using StorageFn = std::function<std::unique_ptr<ComponentStorage>(World*)>;
		void SetStorageFactory(std::string_view typeName, StorageFn fn);

		struct ComponentTypeRecord {
			std::string m_name;
			uint32_t m_dynamicIndex = -1;	// used to index various arrays related to components
			InspectorFn m_inspectFn;
			StorageFn m_storageFactory;
		};
		const std::vector<ComponentTypeRecord>& AllTypes() const { return m_allTypes; }

	private:
		std::vector<ComponentTypeRecord> m_allTypes;
	};
}
}
#pragma once
#include <string_view>
#include <vector>
#include <functional>
#include <memory>
#include <cassert>

namespace R3
{
namespace Entities
{
	// Singleton that tracks all component types + assigns them a runtime unique index
	// Tracks type IDs via name string or templated by component type
	class ComponentStorage;
	class World;
	class ComponentTypeRegistry
	{
	public:
		static ComponentTypeRegistry& GetInstance();

		template<class ComponentType> uint32_t Register();	// returns new type index
		uint32_t GetTypeIndex(std::string_view typeName) const;	// (slowpath) returns -1 if no type found
		template<class ComponentType> static uint32_t GetTypeIndex();	// fastpath

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
		template<class ComponentType>
		static uint32_t& GetStaticTypeIndex();	
		uint32_t Register(std::string_view typeName);	
		std::vector<ComponentTypeRecord> m_allTypes;
	};

	template<class ComponentType> 
	static uint32_t ComponentTypeRegistry::GetTypeIndex()
	{
		return GetStaticTypeIndex<ComponentType>();
	}

	// tricksy. for each component type a static variable is initialised that we can set/get later
	template<class ComponentType>
	static uint32_t& ComponentTypeRegistry::GetStaticTypeIndex()
	{
		static uint32_t s_typeIndex = -1;
		return s_typeIndex;
	}

	template<class ComponentType> 
	uint32_t ComponentTypeRegistry::Register()
	{
		// register by name + get the new type index
		uint32_t newIndex = Register(ComponentType::GetTypeName());
		uint32_t& newStaticIndex = GetStaticTypeIndex<ComponentType>();
		assert(newStaticIndex == -1);
		newStaticIndex = newIndex;
		return newIndex;
	}
}
}
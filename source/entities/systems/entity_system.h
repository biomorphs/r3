#pragma once
#include "engine/systems.h"
#include "entities/component_type_registry.h"
#include "entities/component_storage.h"
#include <memory>


namespace R3
{
namespace Entities
{
	class World;
	class EntitySystem : public System
	{
	public:
		EntitySystem();
		virtual ~EntitySystem();
		static std::string_view GetName() { return "Entities"; }
		virtual void RegisterTickFns();

		template<class ComponentType>
		void RegisterComponentType();

		World* CreateWorld(std::string_view worldName);
		World* GetWorld(std::string_view worldName);
		void DestroyWorld(std::string_view worldName);

	private:
		void RegisterComponentTypeInternal(std::string_view typeName);
		bool ShowGui();
		std::vector<std::unique_ptr<World>> m_worlds;
	};

	template<class ComponentType>
	void EntitySystem::RegisterComponentType()
	{
		RegisterComponentTypeInternal(ComponentType::GetTypeName());
		uint32_t newTypeIndex = ComponentTypeRegistry::GetInstance().GetTypeIndex(ComponentType::GetTypeName());
		ComponentTypeRegistry::GetInstance().SetStorageFactory(ComponentType::GetTypeName(), [newTypeIndex](World* w) {
			return std::make_unique<LinearComponentStorage<ComponentType>>(w, newTypeIndex);
		});
	}
}
}
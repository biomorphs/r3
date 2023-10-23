#pragma once
#include "engine/systems.h"
#include "entities/component_type_registry.h"
#include "entities/component_storage.h"
#include <memory>

namespace R3
{
class ValueInspector;
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
		bool ShowGui();
		bool RunGC();
		std::vector<std::unique_ptr<World>> m_worlds;
	};

	template <typename T>		// SFINAE trick to detect Inspect member fn
	class ComponentHasInspector
	{
		typedef char one;
		typedef long two;
		template <typename C> static one test(decltype(&C::Inspect));
		template <typename C> static two test(...);
	public:
		enum { value = sizeof(test<T>(0)) == sizeof(char) };
	};

	template<class ComponentType>
	void EntitySystem::RegisterComponentType()
	{
		auto& typeRegistry = ComponentTypeRegistry::GetInstance();
		uint32_t newIndex = typeRegistry.Register<ComponentType>();
		typeRegistry.SetStorageFactory(ComponentType::GetTypeName(), [newIndex](World* w) {
			return std::make_unique<LinearComponentStorage<ComponentType>>(w, newIndex);
		});

		// If the component has a 'Inspect' member function, register an inspector
		if constexpr (ComponentHasInspector<ComponentType>::value)
		{
			auto inspectorGlue = [](const EntityHandle& e, World& w, ValueInspector& i) {
				auto* actualComponent = w.GetComponent<ComponentType>(e);
				if (actualComponent)
				{
					actualComponent->Inspect(e, w, i);
				}
			};
			typeRegistry.SetInspector(ComponentType::GetTypeName(), std::move(inspectorGlue));
		}
	}
}
}
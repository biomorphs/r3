#pragma once
#include "engine/systems.h"
#include "engine/systems/lua_system.h"
#include "entities/component_type_registry.h"
#include "entities/component_storage.h"
#include "entities/world.h"
#include <memory>
#include <unordered_map>

namespace R3
{
class ValueInspector;
namespace Entities
{
	class EntitySystem : public System
	{
	public:
		EntitySystem();
		virtual ~EntitySystem();
		static std::string_view GetName() { return "Entities"; }
		virtual void RegisterTickFns();
		virtual bool Init();

		template<class ComponentType>
		void RegisterComponentType(uint32_t initialCapacity = 1024);

		World* CreateWorld(const std::string& id, std::string_view worldName="New World");
		World* GetWorld(const std::string& id);
		void DestroyWorld(const std::string& id);

		void SetActiveWorld(const std::string& id) { m_activeWorldId = id; }
		const std::string& GetActiveWorldID() { return m_activeWorldId; }
		World* GetActiveWorld();

	private:
		bool ShowGui();
		bool RunGC();
		std::unordered_map<std::string, std::unique_ptr<World>> m_worlds;
		std::string m_activeWorldId;
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

	template <typename T>		// SFINAE trick to detect RegisterScripts member fn
	class ComponentHasScripts
	{
		typedef char one;
		typedef long two;
		template <typename C> static one test(decltype(&C::RegisterScripts));
		template <typename C> static two test(...);
	public:
		enum { value = sizeof(test<T>(0)) == sizeof(char) };
	};

	template<class ComponentType>
	void EntitySystem::RegisterComponentType(uint32_t initialCapacity)
	{
		auto& typeRegistry = ComponentTypeRegistry::GetInstance();
		uint32_t newIndex = typeRegistry.Register<ComponentType>();
		typeRegistry.SetStorageFactory(ComponentType::GetTypeName(), [newIndex, initialCapacity](World* w) {
			return std::make_unique<LinearComponentStorage<ComponentType>>(w, newIndex, initialCapacity);
		});

		// If the component has a 'Inspect' member function, register an inspector
		if constexpr (ComponentHasInspector<ComponentType>::value)
		{
			auto inspectorGlue = [](const EntityHandle& e, World& w, ValueInspector& i) {
				auto* actualComponent = w.GetComponent<ComponentType>(e);
				if (actualComponent)
				{
					actualComponent->Inspect(e, &w, i);
				}
			};
			typeRegistry.SetInspector(ComponentType::GetTypeName(), std::move(inspectorGlue));
		}

		// If the component has a 'RegisterScripts' function, call it now
		// Then register script accessors (scripts can only touch the active world)
		if constexpr (ComponentHasScripts<ComponentType>::value)
		{
			auto scripts = Systems::GetSystem<LuaSystem>();
			if (scripts)
			{
				ComponentType::RegisterScripts(*scripts);
				scripts->AddTypeMember<World>("World", std::format("AddComponent_{}", ComponentType::GetTypeName()), 
					[this](Entities::EntityHandle e) -> ComponentType*
				{
					ComponentType* ptr = nullptr;
					auto world = GetActiveWorld();
					if (world)
					{
						world->AddComponent<ComponentType>(e);
						ptr = world->GetComponent<ComponentType>(e);
					}
					return ptr;
				});
				scripts->AddTypeMember<World>("World", std::format("GetComponent_{}", ComponentType::GetTypeName()),
					[this](Entities::EntityHandle e) -> ComponentType*
				{
					ComponentType* ptr = nullptr;
					auto world = GetActiveWorld();
					if (world)
					{
						ptr = world->GetComponent<ComponentType>(e);
					}
					return ptr;
				});
			}
		}
	}
}
}
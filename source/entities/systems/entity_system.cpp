#include "entity_system.h"
#include "entities/world.h"
#include "core/profiler.h"
#include <cassert>

namespace R3
{
namespace Entities
{
	EntitySystem::EntitySystem()
	{
	}

	EntitySystem::~EntitySystem()
	{
	}

	void EntitySystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("Entities::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("Entities::RunGC", [this]() {
			return RunGC();
		});
	}

	World* EntitySystem::CreateWorld(const std::string& id, std::string_view worldName)
	{
		R3_PROF_EVENT();
		if (GetWorld(id) != nullptr)
		{
			assert(!"A world already exists with that ID");
			return nullptr;
		}
		auto newWorld = std::make_unique<World>();
		newWorld->SetName(worldName);
		World* worldPtr = newWorld.get();
		m_worlds[id] = std::move(newWorld);
		return worldPtr;
	}

	World* EntitySystem::GetWorld(const std::string& id)
	{
		R3_PROF_EVENT();
		auto found = m_worlds.find(id);
		return (found != m_worlds.end()) ? found->second.get() : nullptr;
	}

	void EntitySystem::DestroyWorld(const std::string& id)
	{
		R3_PROF_EVENT();
		m_worlds.erase(id);
	}

	World* EntitySystem::GetActiveWorld()
	{
		return GetWorld(m_activeWorldId);
	}

	bool EntitySystem::ShowGui()
	{
		R3_PROF_EVENT();
		return true;
	}

	bool EntitySystem::RunGC()
	{
		R3_PROF_EVENT();
		for (auto& w : m_worlds)
		{
			w.second->CollectGarbage();
		}
		return true;
	}
}
}
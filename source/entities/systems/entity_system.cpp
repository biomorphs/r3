#include "entity_system.h"
#include "entities/world.h"
#include "engine/imgui_menubar_helper.h"
#include "core/profiler.h"
#include <cassert>
#include <imgui.h>

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

	bool EntitySystem::Init()
	{
		auto scripts = Systems::GetSystem<LuaSystem>();
		if (scripts)
		{
			scripts->RegisterType<EntityHandle>("EntityHandle",
				sol::constructors<EntityHandle()>(),
				"GetID", &EntityHandle::GetID
			);
			scripts->RegisterType<World>("World",
				"AddEntity", &World::AddEntity, 
				"GetParent", &World::GetParent,
				"SetParent", &World::SetParent,
				"RemoveEntity", &World::RemoveEntity,
				"IsHandleValid", &World::IsHandleValid,
				"SetEntityName", &World::SetEntityName,
				"GetEntityName", &World::GetEntityName,
				"GetEntityByName", &World::GetEntityByName,
				"ImportScene", &World::Import,
				"GetOwnersOfComponent1", &World::GetOwnersOfComponent1
			);
			scripts->RegisterFunction("ActiveWorld", [this]() -> Entities::World* {
				return GetActiveWorld();
			});
		}
		return true;
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
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Entities", [this]() {
			m_showGui = !m_showGui;
		});
		if (!m_showGui)
		{
			return true;
		}
		R3_PROF_EVENT();
		constexpr auto sizeMb = [](size_t v) -> float
		{
			return (float)v / (1024.0f * 1024.0f);
		};
		if (ImGui::Begin("Entities"))
		{
			const auto& alltypes = ComponentTypeRegistry::GetInstance().AllTypes();
			std::string txt;
			for (auto& w : m_worlds)
			{
				size_t worldTotalBytes = 0, worldBytesUsed = 0;
				txt = std::format("{} ({})", w.second->GetName(), w.first.c_str());
				ImGui::SeparatorText(txt.c_str());
				for (const auto& t : alltypes)
				{
					auto storage = w.second->GetStorage(t.m_name);
					if (storage != nullptr && storage->GetTotalCount() > 0)
					{	
						size_t totalBytes = 0, bytesUsed = 0;
						storage->GetMemoryUsage(totalBytes, bytesUsed);
						txt = std::format("{}: {} ({:.2f} mb in use, {:.2f} mb allocated)", t.m_name, storage->GetTotalCount(), sizeMb(bytesUsed), sizeMb(totalBytes));
						ImGui::Text(txt.c_str());
						worldTotalBytes += totalBytes;
						worldBytesUsed += bytesUsed;
					}
				}
				std::string txt = std::format("World Memory: ({:.2f} mb in use, {:.2f} mb allocated)", sizeMb(worldBytesUsed), sizeMb(worldTotalBytes));
				ImGui::Text(txt.c_str());
				txt = std::format("Active entities: {}", w.second->GetActiveEntityCount());
				ImGui::Text(txt.c_str());
				txt = std::format("Entities Pending Delete: {}", w.second->GetPendingDeleteCount());
				ImGui::Text(txt.c_str());
				txt = std::format("Reserved Handles: {}", w.second->GetReservedHandleCount());
				ImGui::Text(txt.c_str());
			}
		}
		ImGui::End();
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
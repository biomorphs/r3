#include "entity_system.h"
#include "entities/world.h"
#include "entities/component_type_registry.h"
#include "core/profiler.h"
#include "imgui.h"
#include <cassert>
#include <format>

#include "core/time.h"

namespace R3
{
namespace Entities
{
	struct TestComponent1
	{
		static std::string_view GetTypeName() { return "TestComponent1"; }

		std::string m_text = "Hello";
	};

	struct TestComponent2
	{
		static std::string_view GetTypeName() { return "TestComponent2"; }

		std::string m_text = "Yo";
	};

	struct BenchComponent1
	{
		static std::string_view GetTypeName() { return "BenchmarkComponent1"; }
		uint64_t m_someData = 5;
	};

	struct BenchComponent2
	{
		static std::string_view GetTypeName() { return "BenchmarkComponent2"; }
		uint64_t m_someData1 = 10;
		uint64_t m_someData2 = 20;
		uint64_t m_someData3 = 30;
		uint64_t m_someData4 = 40;
	};

	void EntityBenchmarks(World* w)
	{
		const auto benchStart = R3::Time::HighPerformanceCounterTicks();
		const auto freq = R3::Time::HighPerformanceCounterFrequency();
		constexpr uint32_t test1Repeats = 20;
		constexpr uint32_t test2Repeats = 20;
		constexpr uint32_t test3Repeats = 200;

		// test 1, add 10k empty entities multiple times
		for (int i = 0; i < test1Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			for (int i = 0; i < 10000; ++i)
			{
				w->AddEntity();
			}
			auto testEnd = R3::Time::HighPerformanceCounterTicks();
			double taken = (testEnd - testStart) / (double)freq;
			R3::LogInfo("Add 10k entities took {}s", taken);
		}

		// test 2, add a bunch of entities, delete every 3rd one
		std::vector<EntityHandle> toDelete;
		toDelete.reserve(10000 / 2);
		for (int i = 0; i < test2Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			for (int i = 0; i < 10000; ++i)
			{
				EntityHandle e = w->AddEntity();
				if ((i % 3) == 0)
				{
					toDelete.emplace_back(e);
				}
			}
			auto addEnd = R3::Time::HighPerformanceCounterTicks();
			R3::LogInfo("Add 10k entities took {}s", (addEnd - testStart) / (double)freq);

			auto delStart = R3::Time::HighPerformanceCounterTicks();
			for (int d = 0; d < toDelete.size(); ++d)
			{
				w->RemoveEntity(toDelete[d]);
			}
			auto testEnd = R3::Time::HighPerformanceCounterTicks();
			R3::LogInfo("Remove entities took {}s", (testEnd - delStart) / (double)freq);

			auto gcStart = R3::Time::HighPerformanceCounterTicks();
			w->CollectGarbage();
			auto gcEnd = R3::Time::HighPerformanceCounterTicks();
			R3::LogInfo("GC took {}s", (gcEnd - gcStart) / (double)freq);

			toDelete.clear();
		}

		// test 3 add a bunch of entities with components
		for (int i = 0; i < test3Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			for (int i = 0; i < 5000; ++i)
			{
				EntityHandle e = w->AddEntity();
				w->AddComponent(e, "BenchmarkComponent1");
			}
			auto cmp1 = R3::Time::HighPerformanceCounterTicks();
			R3::LogInfo("Add 5000 entities with cmp 1 took {}s", (cmp1 - testStart) / (double)freq);
			for (int i = 0; i < 5000; ++i)
			{
				EntityHandle e = w->AddEntity();
				w->AddComponent(e, "BenchmarkComponent2");
			}
			auto cmp2 = R3::Time::HighPerformanceCounterTicks();
			R3::LogInfo("Add 5000 entities with cmp 2 took {}s", (cmp2 - cmp1) / (double)freq);
			for (int i = 0; i < 5000; ++i)
			{
				EntityHandle e = w->AddEntity();
				w->AddComponent(e, "BenchmarkComponent1");
				w->AddComponent(e, "BenchmarkComponent2");
			}
			auto cmp12 = R3::Time::HighPerformanceCounterTicks();
			R3::LogInfo("Add 5000 entities with cmp 1 + 2 took {}s", (cmp12 - cmp2) / (double)freq);
		}
	}

	EntitySystem::EntitySystem()
	{
		World* testWorld = CreateWorld("Benchmarks");
		RegisterComponentType<BenchComponent1>();
		RegisterComponentType<BenchComponent2>();
		EntityBenchmarks(testWorld);

		RegisterComponentType<TestComponent1>();
		RegisterComponentType<TestComponent2>();
		
		World* editWorld = CreateWorld("EditorWorld");
		auto e1 = editWorld->AddEntity();
		editWorld->AddComponent(e1, "TestComponent1");
		editWorld->AddComponent(e1, "TestComponent2");

		auto e2 = editWorld->AddEntity();
		auto e3 = editWorld->AddEntity();
		editWorld->AddComponent(e3, "TestComponent1");

		editWorld->RemoveEntity(e1);
		editWorld->CollectGarbage();

		auto e4 = editWorld->AddEntity();
		editWorld->AddComponent(e4, "TestComponent2");
	}

	EntitySystem::~EntitySystem()
	{
	}

	void EntitySystem::RegisterTickFns()
	{
		RegisterTick("Entities::ShowGui", [this]() {
			return ShowGui();
		});
	}

	void EntitySystem::RegisterComponentTypeInternal(std::string_view typeName)
	{
		auto& types = ComponentTypeRegistry::GetInstance();
		types.Register(typeName);
	}

	World* EntitySystem::CreateWorld(std::string_view worldName)
	{
		if (GetWorld(worldName) != nullptr)
		{
			assert(!"A world already exists with that name");
			return nullptr;
		}
		auto newWorld = std::make_unique<World>();
		newWorld->SetName(worldName);
		World* worldPtr = newWorld.get();
		m_worlds.emplace_back(std::move(newWorld));
		return worldPtr;
	}

	World* EntitySystem::GetWorld(std::string_view worldName)
	{
		auto found = std::find_if(m_worlds.begin(), m_worlds.end(), [worldName](const std::unique_ptr<World>& w) {
			return w->GetName() == worldName;	// is this a string comparison??? check!
		});
		return found != m_worlds.end() ? found->get() : nullptr;;
	}

	void EntitySystem::DestroyWorld(std::string_view worldName)
	{
		auto found = std::find_if(m_worlds.begin(), m_worlds.end(), [worldName](const std::unique_ptr<World>& w) {
			return w->GetName() == worldName;	// is this a string comparison??? check!
		});
		assert(found != m_worlds.end());
		if (found != m_worlds.end())
		{
			m_worlds.erase(found);
		}
	}

	bool EntitySystem::ShowGui()
	{
		R3_PROF_EVENT();
		ImGui::Begin("Entity System");
		{
			auto str = std::format("All Worlds({})", m_worlds.size());
			if (ImGui::TreeNode(str.c_str()))
			{
				for (int w = 0; w < m_worlds.size(); ++w)
				{
					auto& world = *m_worlds[w].get();
					str = std::format("{} ({} active entities)", world.GetName(), world.GetActiveEntityCount());
					if (ImGui::TreeNode(str.c_str()))
					{
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			const auto& allTypes = ComponentTypeRegistry::GetInstance().AllTypes();
			str = std::format("All Component Types({})", allTypes.size());
			if (ImGui::TreeNode(str.c_str()))
			{
				for (int t = 0; t < allTypes.size(); ++t)
				{
					auto& type = allTypes[t];
					str = std::format("{} ({})", type.m_name, type.m_dynamicIndex);
					if (ImGui::TreeNode(str.c_str()))
					{
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
		return true;
	}
}
}
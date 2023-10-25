#include "entity_system.h"
#include "entities/world.h"
#include "entities/component_type_registry.h"
#include "entities/queries.h"
#include "engine/entity_list_widget.h"
#include "engine/serialiser.h"
#include "editor/world_info_widget.h"
#include "engine/components/environment_settings.h"		// tempshit
#include "core/profiler.h"
#include "core/time.h"
#include "imgui.h"
#include <cassert>
#include <format>

namespace R3
{
	struct TestComponent1
	{
		static std::string_view GetTypeName() { return "TestComponent1"; }
		void SerialiseJson(JsonSerialiser& s)
		{
			s("text", m_text);
		}
		std::string m_text = "Hello";
	};

	struct TestComponent2
	{
		static std::string_view GetTypeName() { return "TestComponent2"; }
		void SerialiseJson(JsonSerialiser& s)
		{
			s("text", m_text);
		}
		std::string m_text = "Yo";
	};

	struct BenchComponent1
	{
		static std::string_view GetTypeName() { return "BenchComponent1"; }
		uint64_t m_someData = 5;
	};

	template<> void SerialiseJson(BenchComponent1& t, JsonSerialiser& s)
	{
		s("SomeData", t.m_someData);
	}

	struct BenchComponent2
	{
		static std::string_view GetTypeName() { return "BenchComponent2"; }
		uint64_t m_someData1 = 10;
		uint64_t m_someData2 = 20;
		uint64_t m_someData3 = 30;
		uint64_t m_someData4 = 40;
	};

	template<> void SerialiseJson(BenchComponent2& t, JsonSerialiser& s)
	{
		s("SomeData1", t.m_someData1);
		s("SomeData2", t.m_someData2);
		s("SomeData3", t.m_someData3);
		s("SomeData4", t.m_someData4);
	}

namespace Entities
{
	void EntityBenchmarks(World* w)
	{
		R3_PROF_EVENT();
		const auto benchStart = R3::Time::HighPerformanceCounterTicks();
		const auto freq = R3::Time::HighPerformanceCounterFrequency();
		constexpr uint32_t test1Repeats = 10;
		constexpr uint32_t test2Repeats = 10;
		constexpr uint32_t test3Repeats = 15;
		constexpr uint32_t test4Repeats = 50;
		constexpr uint32_t test5Repeats = 1;

		// test 1, add 10k empty entities multiple times
		for (int i = 0; i < test1Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			for (int j = 0; j < 10000; ++j)
			{
				w->AddEntity();
			}
			auto testEnd = R3::Time::HighPerformanceCounterTicks();
			double taken = (testEnd - testStart) / (double)freq;
			LogInfo("Add 10k entities took {}s", taken);
		}

		// test 2, add a bunch of entities, delete every 3rd one
		std::vector<EntityHandle> toDelete;
		toDelete.reserve(10000 / 2);
		for (int i = 0; i < test2Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			for (int j = 0; j < 10000; ++j)
			{
				EntityHandle e = w->AddEntity();
				if ((j % 3) == 0)
				{
					toDelete.emplace_back(e);
				}
			}
			auto addEnd = R3::Time::HighPerformanceCounterTicks();
			LogInfo("Add 10k entities took {}s", (addEnd - testStart) / (double)freq);

			auto delStart = R3::Time::HighPerformanceCounterTicks();
			for (int d = 0; d < toDelete.size(); ++d)
			{
				w->RemoveEntity(toDelete[d]);
			}
			auto testEnd = R3::Time::HighPerformanceCounterTicks();
			LogInfo("Remove entities took {}s", (delStart - testStart) / (double)freq);

			auto gcStart = R3::Time::HighPerformanceCounterTicks();
			w->CollectGarbage();
			auto gcEnd = R3::Time::HighPerformanceCounterTicks();
			LogInfo("GC took {}s", (gcEnd - gcStart) / (double)freq);

			toDelete.clear();
		}

		// test 3 add a bunch of entities with components
		for (int i = 0; i < test3Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			for (int j = 0; j < 5000; ++j)
			{
				EntityHandle e = w->AddEntity();
				w->AddComponent<BenchComponent1>(e);
			}
			auto cmp1 = R3::Time::HighPerformanceCounterTicks();
			LogInfo("Add 5000 entities with cmp 1 took {}s", (cmp1 - testStart) / (double)freq);
			for (int j = 0; j < 5000; ++j)
			{
				EntityHandle e = w->AddEntity();
				w->AddComponent<BenchComponent2>(e);
			}
			auto cmp2 = R3::Time::HighPerformanceCounterTicks();
			LogInfo("Add 5000 entities with cmp 2 took {}s", (cmp2 - cmp1) / (double)freq);
			for (int j = 0; j < 5000; ++j)
			{
				EntityHandle e = w->AddEntity();
				w->AddComponent<BenchComponent1>(e);
				w->AddComponent<BenchComponent2>(e);
			}
			auto cmp12 = R3::Time::HighPerformanceCounterTicks();
			LogInfo("Add 5000 entities with cmp 1 + 2 took {}s", (cmp12 - cmp2) / (double)freq);
		}

		// test 4 iterate entities with pair of components
		for (int i = 0; i < test4Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			int foundCount = 0;

			auto fn = [&foundCount](const EntityHandle& e, BenchComponent1& cmp1, BenchComponent2& cmp2)
			{
				foundCount++;
				return true;	// return false = stop iteration
			};
			Entities::Queries::ForEach<BenchComponent1, BenchComponent2>(w, fn);

			auto testEnd = R3::Time::HighPerformanceCounterTicks();
			LogInfo("Iterate over {} entities with cmp 1 + 2 took {}s", foundCount, (testEnd - testStart) / (double)freq);
		}

		// test 5, serialisation
		for (int i = 0; i < test5Repeats; ++i)
		{
			auto testStart = R3::Time::HighPerformanceCounterTicks();
			auto worldJson = w->SerialiseEntities();
			auto testEnd = R3::Time::HighPerformanceCounterTicks();			
			LogInfo("Serialise to JSON took {}s", (testEnd - testStart) / (double)freq);
		}
	}

	EntitySystem::EntitySystem()
	{
		R3_PROF_EVENT();
		RegisterComponentType<BenchComponent1>();
		RegisterComponentType<BenchComponent2>();
		RegisterComponentType<TestComponent1>();
		RegisterComponentType<TestComponent2>();
		RegisterComponentType<EnvironmentSettingsComponent>();
		// EntityBenchmarks(CreateWorld("Benchmarks", "Benchmarks"));
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
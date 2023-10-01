#include "entity_system.h"
#include "entities/world.h"
#include "entities/component_type_registry.h"
#include "entities/queries.h"
#include "engine/serialiser.h"
#include "core/profiler.h"
#include "core/time.h"
#include "imgui.h"
#include <cassert>
#include <format>



namespace R3
{
	struct test
	{
		int m_x = 3;
		int m_y = 7;
		int m_z = 10;
		std::string m_str = "Hello";
	};

	template<> void SerialiseJson(test& t, R3::JsonSerialiser& s)
	{
		s.TypeName("test");
		s("X", t.m_x);
		s("Y", t.m_y);
		s("Z", t.m_z);
		s("Str", t.m_str);
	}

	struct Test2
	{
		int m_x = 0;
	};

	template<> void SerialiseJson(Test2& t, R3::JsonSerialiser& s)
	{
		s.TypeName("Test2");
		s("X", t.m_x);
	}
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
		static std::string_view GetTypeName() { return "BenchComponent1"; }
		uint64_t m_someData = 5;
	};

	struct BenchComponent2
	{
		static std::string_view GetTypeName() { return "BenchComponent2"; }
		uint64_t m_someData1 = 10;
		uint64_t m_someData2 = 20;
		uint64_t m_someData3 = 30;
		uint64_t m_someData4 = 40;
	};

	

	void EntityBenchmarks(World* w)
	{
		R3_PROF_EVENT();
		int x = 1;
		std::string s = "Yo";
		float y = 2.0f;
		JsonSerialiser json(JsonSerialiser::Write);
		json("X", x);
		json("S", s);
		json("Y", y);
		
		test t;
		t.m_str = "Whut";
		json("test", t);

		Test2 t2;
		json("test2", t2);

		std::vector<uint32_t> someInts = { 1,2,3,4,5 };
		json("someInts", someInts);

		std::vector<Test2> someTest2s = {
			{ 1 }, { 5 }, { 7 }
		};
		json("someTest2s", someTest2s);

		LogInfo("Serialised data: \n{}", json.c_str());

		const auto benchStart = R3::Time::HighPerformanceCounterTicks();
		const auto freq = R3::Time::HighPerformanceCounterFrequency();
		constexpr uint32_t test1Repeats = 20;
		constexpr uint32_t test2Repeats = 20;
		constexpr uint32_t test3Repeats = 30;
		constexpr uint32_t test4Repeats = 100;

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
	}

	EntitySystem::EntitySystem()
	{
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
		RegisterTick("Entities::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("Entities::RunGC", [this]() {
			return RunGC();
		});
	}

	World* EntitySystem::CreateWorld(std::string_view worldName)
	{
		R3_PROF_EVENT();
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
		R3_PROF_EVENT();
		auto found = std::find_if(m_worlds.begin(), m_worlds.end(), [worldName](const std::unique_ptr<World>& w) {
			return w->GetName() == worldName;	// is this a string comparison??? check!
		});
		return found != m_worlds.end() ? found->get() : nullptr;;
	}

	void EntitySystem::DestroyWorld(std::string_view worldName)
	{
		R3_PROF_EVENT();
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

	bool EntitySystem::RunGC()
	{
		R3_PROF_EVENT();
		for (auto& w : m_worlds)
		{
			w->CollectGarbage();
		}
		return true;
	}
}
}
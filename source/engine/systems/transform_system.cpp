#include "transform_system.h"
#include "engine/components/transform.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "core/profiler.h"

namespace R3
{
	void TransformSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("Transforms::OnFixedUpdate", [this]() {
			return OnFixedUpdate();
		});
	}

	bool TransformSystem::OnFixedUpdate()
	{
		R3_PROF_EVENT();

		auto world = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (world)
		{
			// we want to run a few cache lines per job
			constexpr size_t c_cacheLineSize = 32 * 1024;
			constexpr size_t c_cacheLineCount = 8;
			constexpr size_t c_componentsPerJob = (c_cacheLineSize * c_cacheLineCount) / sizeof(TransformComponent);
			auto storePrevFrameData = [](const Entities::EntityHandle& e, TransformComponent& cmp) {
				cmp.StorePreviousFrameData();
			};
			Entities::Queries::ForEachAsync<TransformComponent>(world, c_componentsPerJob, storePrevFrameData);
		}

		return true;
	}
}
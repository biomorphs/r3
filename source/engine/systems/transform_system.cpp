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
			Entities::Queries::ForEach<TransformComponent>(world, [](const Entities::EntityHandle& e, TransformComponent& cmp) {
				cmp.StorePreviousFrameData();
				return true;
			});
		}

		return true;
	}
}
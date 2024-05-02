#include "register_engine_components.h"
#include "components/environment_settings.h"
#include "components/transform.h"
#include "components/camera.h"
#include "components/static_mesh.h"
#include "components/static_mesh_materials.h"
#include "components/point_light.h"
#include "entities/systems/entity_system.h"

namespace R3
{
	void RegisterEngineComponents()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		entities->RegisterComponentType<EnvironmentSettingsComponent>();
		entities->RegisterComponentType<TransformComponent>();
		entities->RegisterComponentType<CameraComponent>();
		entities->RegisterComponentType<StaticMeshComponent>();
		entities->RegisterComponentType<StaticMeshMaterialsComponent>();
		entities->RegisterComponentType<PointLightComponent>();
	}
}
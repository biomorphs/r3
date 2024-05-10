#include "register_engine_components.h"
#include "components/environment_settings.h"
#include "components/transform.h"
#include "components/camera.h"
#include "components/static_mesh.h"
#include "components/static_mesh_materials.h"
#include "components/point_light.h"
#include "components/lua_script.h"
#include "entities/systems/entity_system.h"

namespace R3
{
	void RegisterEngineComponents()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		entities->RegisterComponentType<EnvironmentSettingsComponent>(8);
		entities->RegisterComponentType<TransformComponent>(1024 * 32);
		entities->RegisterComponentType<CameraComponent>(32);
		entities->RegisterComponentType<StaticMeshComponent>(1024 * 32);
		entities->RegisterComponentType<StaticMeshMaterialsComponent>(1024);
		entities->RegisterComponentType<PointLightComponent>(1024 * 16);
		entities->RegisterComponentType<LuaScriptComponent>(1024);
	}
}
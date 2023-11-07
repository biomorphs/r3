#include "camera_system.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "engine/components/camera.h"
#include "engine/components/transform.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/systems/imgui_system.h"
#include "render/render_system.h"

namespace R3
{
	CameraSystem::CameraSystem()
	{
	}

	CameraSystem::~CameraSystem()
	{
	}

	void CameraSystem::RegisterTickFns()
	{
		RegisterTick("Cameras::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("Cameras::PreRenderUpdate", [this]() {
			return Update();
		});
	}

	bool CameraSystem::ShowGui()
	{
		auto entitySys = GetSystem<Entities::EntitySystem>();
		auto activeWorld = entitySys->GetActiveWorld();
		auto& cameraMenu = MenuBar::MainMenu().GetSubmenu("Cameras");
		auto& entityMenu = cameraMenu.GetSubmenu("Entities");
		if (activeWorld)
		{
			std::string activeWorldId = entitySys->GetActiveWorldID();
			auto forEachCamera = [&](const Entities::EntityHandle& parent, CameraComponent& c, TransformComponent& t) {
				entityMenu.AddItem(activeWorld->GetEntityDisplayName(parent), [this, activeWorldId, parent]() {
					m_activeCameras[activeWorldId] = parent;
				});
				return true;
			};
			Entities::Queries::ForEach<CameraComponent, TransformComponent>(activeWorld, forEachCamera);
		}

		return true;
	}

	bool CameraSystem::Update()
	{
		auto entitySys = GetSystem<Entities::EntitySystem>();
		auto renderSys = GetSystem<RenderSystem>();
		auto activeWorld = entitySys->GetActiveWorld();
		const auto windowSize = renderSys->GetWindowExtents();
		const float aspectRatio = windowSize.x / windowSize.y;
		if (activeWorld)
		{
			std::string worldId = entitySys->GetActiveWorldID();
			auto foundActiveCam = m_activeCameras.find(worldId);
			if (foundActiveCam != m_activeCameras.end())
			{
				auto camCmp = activeWorld->GetComponent<CameraComponent>(foundActiveCam->second);
				auto transCmp = activeWorld->GetComponent<TransformComponent>(foundActiveCam->second);
				if (camCmp && transCmp)
				{
					const glm::vec3 lookUp = transCmp->GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
					const glm::vec3 lookDirection = transCmp->GetOrientation() * glm::vec3(0.0f, 0.0f, 1.0f);
					const glm::vec3 wsPosition = glm::vec3(transCmp->GetWorldspaceMatrix() * glm::vec4(0, 0, 0, 1));
					m_mainCamera.SetProjection(camCmp->m_fov, aspectRatio, camCmp->m_nearPlane, camCmp->m_farPlane);
					m_mainCamera.LookAt(wsPosition, wsPosition + lookDirection, lookUp);
					return true;
				}
			}
		}

		// some kind of sensible defaults
		m_mainCamera.SetProjection(70.0f, aspectRatio, 0.1f, 100.0f);
		m_mainCamera.LookAt({ 3,3,-15.0 }, { 0,0,0 }, { 0,1,0 });

		return true;
	}
}
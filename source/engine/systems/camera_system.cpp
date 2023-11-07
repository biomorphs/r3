#include "camera_system.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "engine/components/camera.h"
#include "engine/components/transform.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/frustum.h"
#include "engine/systems/imgui_system.h"
#include "render/render_system.h"
#include "render/immediate_renderer.h"

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
		cameraMenu.AddItem(m_drawFrustums ? "Hide Camera Frustums" : "Show Camera Frustums", [this]() {
			m_drawFrustums = !m_drawFrustums;
		});

		return true;
	}

	void CameraSystem::DrawCameraFrustums()
	{
		auto entitySys = GetSystem<Entities::EntitySystem>();
		auto renderSys = GetSystem<RenderSystem>();
		auto activeWorld = entitySys->GetActiveWorld();
		if (activeWorld)
		{
			Camera tmpCam;	// used to build frustum
			auto forEachCam = [&](const Entities::EntityHandle& e, CameraComponent& c, TransformComponent& t) {
				ApplyEntityToCamera(c, t, tmpCam);
				Frustum frustum(tmpCam.ProjectionMatrix() * tmpCam.ViewMatrix());
				renderSys->GetImRenderer().AddFrustum(frustum, { 1,1,0,1 });
				return true;
			};
			Entities::Queries::ForEach<CameraComponent, TransformComponent>(activeWorld, forEachCam);
		}
	}

	void CameraSystem::ApplyEntityToCamera(const CameraComponent& camCmp, const TransformComponent& transCmp, Camera& target)
	{
		auto renderSys = GetSystem<RenderSystem>();
		const auto windowSize = renderSys->GetWindowExtents();
		const float aspectRatio = windowSize.x / windowSize.y;
		const glm::vec3 lookUp = transCmp.GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 lookDirection = transCmp.GetOrientation() * glm::vec3(0.0f, 0.0f, 1.0f);
		const glm::vec3 wsPosition = glm::vec3(transCmp.GetWorldspaceMatrix() * glm::vec4(0, 0, 0, 1));
		target.SetProjection(camCmp.m_fov, aspectRatio, camCmp.m_nearPlane, camCmp.m_farPlane);
		target.LookAt(wsPosition, wsPosition + lookDirection, lookUp);
	}

	bool CameraSystem::Update()
	{
		DrawCameraFrustums();

		auto entitySys = GetSystem<Entities::EntitySystem>();
		auto activeWorld = entitySys->GetActiveWorld();
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
					ApplyEntityToCamera(*camCmp,*transCmp, m_mainCamera);
					return true;
				}
			}
		}

		// some kind of sensible defaults
		m_mainCamera.SetProjection(70.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		m_mainCamera.LookAt({ 3,3,-15.0 }, { 0,0,0 }, { 0,1,0 });

		return true;
	}
}
#include "camera_system.h"
#include "entities/systems/entity_system.h"
#include "engine/systems/imgui_system.h"
#include "engine/systems/input_system.h"
#include "engine/systems/time_system.h"
#include "engine/systems/lua_system.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/components/camera.h"
#include "engine/components/transform.h"
#include "engine/components/environment_settings.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/utils/frustum.h"
#include "engine/graphics/flycam.h"
#include "entities/queries.h"
#include "render/render_system.h"
#include <imgui.h>

namespace R3
{
	CameraSystem::CameraSystem()
	{
		m_flyCam = std::make_unique<Flycam>();
		m_flyCam->SetPosition({ 0,2,-5 });
		m_flyCam->SetYaw(glm::radians(180.0f));
	}

	CameraSystem::~CameraSystem()
	{
	}

	void CameraSystem::RegisterTickFns()
	{
		RegisterTick("Cameras::ShowGui", [this]() {
			return ShowGui();
		});
		RegisterTick("Cameras::FixedUpdate", [this]() {
			return FixedUpdate();
		});
		RegisterTick("Cameras::PreRenderUpdate", [this]() {
			return Update();
		});
	}

	bool CameraSystem::ShowGui()
	{
		R3_PROF_EVENT();
		auto entitySys = GetSystem<Entities::EntitySystem>();
		auto activeWorld = entitySys->GetActiveWorld();
		auto& cameraMenu = MenuBar::MainMenu().GetSubmenu("Cameras");
		auto& entityMenu = cameraMenu.GetSubmenu("Entities");
		if (activeWorld)
		{
			std::string activeWorldId = entitySys->GetActiveWorldID();
			entityMenu.AddItem("No Entity", [this, activeWorldId]() {
				SetActiveCamera({});
			});
			auto forEachCamera = [&](const Entities::EntityHandle& parent, CameraComponent& c, TransformComponent& t) {
				entityMenu.AddItem(activeWorld->GetEntityDisplayName(parent), [this, activeWorldId, parent]() {
					SetActiveCamera(parent);
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
		R3_PROF_EVENT();
		auto entitySys = GetSystem<Entities::EntitySystem>();
		auto activeWorld = entitySys->GetActiveWorld();
		if (activeWorld)
		{
			Camera tmpCam;	// used to build frustum
			auto forEachCam = [&](const Entities::EntityHandle& e, CameraComponent& c, TransformComponent& t) {
				ApplyEntityToCamera(*activeWorld, e, c, t, tmpCam);
				Frustum frustum(tmpCam.ProjectionMatrix() * tmpCam.ViewMatrix());
				GetSystem<ImmediateRenderSystem>()->m_imRender->AddFrustum(frustum, {1,1,0,1});
				return true;
			};
			Entities::Queries::ForEach<CameraComponent, TransformComponent>(activeWorld, forEachCam);
		}
	}

	bool CameraSystem::Init()
	{
		auto scripts = GetSystem<LuaSystem>();
		scripts->RegisterFunction("SetActiveCamera", [this](const R3::Entities::EntityHandle& e) {
			SetActiveCamera(e);
		});
		return true;
	}

	void CameraSystem::SetActiveCamera(const R3::Entities::EntityHandle& e)
	{
		auto entitySys = GetSystem<Entities::EntitySystem>();
		std::string activeWorldId = entitySys->GetActiveWorldID();
		auto world = entitySys->GetWorld(activeWorldId);

		auto currentCamEntity = m_activeCameras.find(activeWorldId);
		if (currentCamEntity != m_activeCameras.end())
		{
			CameraComponent* activeCamera = entitySys->GetWorld(activeWorldId)->GetComponent<CameraComponent>(currentCamEntity->second);
			if (activeCamera)
			{
				TransformComponent* camTransform = world->GetComponent<TransformComponent>(currentCamEntity->second);
				if (camTransform)
				{
					m_flyCam->SetPosition(camTransform->GetPosition());
				}
			}
		}

		m_activeCameras[activeWorldId] = e;
	}

	bool CameraSystem::FixedUpdate()
	{
		R3_PROF_EVENT();
		if (IsFlycamActive())
		{
			auto input = GetSystem<InputSystem>();
			double delta = GetSystem<TimeSystem>()->GetFixedUpdateDelta();
			m_flyCam->StorePreviousFrameData();
			m_flyCam->Update(input->ControllerState(0), delta);
			if (!input->IsGuiCapturingInput())
			{
				m_flyCam->Update(input->GetMouseState(), delta);
				m_flyCam->Update(input->GetKeyboardState(), delta);
			}
		}

		return true;
	}

	void CameraSystem::ApplyEntityToCamera(Entities::World& w, Entities::EntityHandle camEntity, const CameraComponent& camCmp, const TransformComponent& transCmp, Camera& target)
	{
		R3_PROF_EVENT();
		auto renderSys = GetSystem<RenderSystem>();
		const auto windowSize = renderSys->GetWindowExtents();
		const float aspectRatio = windowSize.x / windowSize.y;
		const glm::mat4 interpolatedTransform = transCmp.GetWorldspaceInterpolated(camEntity, w);
		glm::mat3 rotationPart(interpolatedTransform);
		const glm::vec3 lookUp = rotationPart * glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 lookDirection = rotationPart * glm::vec3(0.0f, 0.0f, 1.0f);
		const glm::vec3 wsPosition(interpolatedTransform[3]);
		target.SetProjection(camCmp.m_fov, aspectRatio, camCmp.m_nearPlane, camCmp.m_farPlane);
		target.LookAt(wsPosition, wsPosition + lookDirection, lookUp);
	}

	bool CameraSystem::IsFlycamActive()
	{
		R3_PROF_EVENT();
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
				return !(camCmp && transCmp);
			}
		}
		return true;
	}

	void CameraSystem::ApplyFlycamToCamera()
	{
		R3_PROF_EVENT();
		auto renderSys = GetSystem<RenderSystem>();
		const auto windowSize = renderSys->GetWindowExtents();
		const float aspectRatio = windowSize.x / windowSize.y;
		m_flyCam->ApplyToCamera(m_mainCamera);
		m_mainCamera.SetProjection(70.0f, aspectRatio, 0.1f, m_flyCamFarPlane);
	}

	bool CameraSystem::Update()
	{
		R3_PROF_EVENT();

		if (m_drawFrustums)
		{
			DrawCameraFrustums();
		}

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
					ApplyEntityToCamera(*activeWorld, foundActiveCam->second, *camCmp,*transCmp, m_mainCamera);
					return true;
				}
			}

			// Get flycam settings from environment components
			auto forEachEnv = [this](Entities::EntityHandle e, EnvironmentSettingsComponent& cmp)
			{
				m_flyCamFarPlane = cmp.m_flyCamFarPlane;
				m_flyCam->m_forwardSpeed = cmp.m_flyCamMoveSpeed;
				return true;
			};
			Entities::Queries::ForEach<EnvironmentSettingsComponent>(activeWorld, forEachEnv);
		}

		ApplyFlycamToCamera();

		return true;
	}
}
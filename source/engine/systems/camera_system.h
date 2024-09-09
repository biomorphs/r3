#pragma once

#include "engine/systems.h"
#include "entities/entity_handle.h"
#include "render/camera.h"
#include <unordered_map>

namespace R3
{
	class Flycam;
	class CameraSystem : public System
	{
	public:
		static std::string_view GetName() { return "Cameras"; }
		CameraSystem();
		virtual ~CameraSystem();
		virtual void RegisterTickFns();
		const Camera& GetMainCamera() { return m_mainCamera; }
		void SetActiveCamera(const R3::Entities::EntityHandle& e);
	private:
		void ApplyFlycamToCamera();
		bool IsFlycamActive();
		bool FixedUpdate();
		void ApplyEntityToCamera(const class CameraComponent& camCmp, const class TransformComponent& transCmp, Camera& target);
		void DrawCameraFrustums();
		bool Init();
		bool ShowGui();
		bool Update();
		bool m_drawFrustums = false;
		std::unique_ptr<Flycam> m_flyCam;
		std::unordered_map<std::string, Entities::EntityHandle> m_activeCameras;	// per-world active camera entity, key = world ID
		Camera m_mainCamera;	// updated each frame from active world/entity
	};
}
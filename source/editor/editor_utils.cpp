#include "editor_utils.h"
#include "engine/systems/input_system.h"
#include "engine/systems/camera_system.h"
#include "engine/systems/model_data_system.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "entities/world.h"
#include "render/immediate_renderer.h"
#include "render/camera.h"
#include "render/render_system.h"
#include "core/profiler.h"

namespace R3
{
	void MouseCursorToWorldspaceRay(float rayDistance, glm::vec3& rayStart, glm::vec3& rayEnd)
	{
		R3_PROF_EVENT();
		auto input = Systems::GetSystem<InputSystem>();
		const auto& mainCam = Systems::GetSystem<CameraSystem>()->GetMainCamera();
		const glm::vec2 windowExtents = Systems::GetSystem<RenderSystem>()->GetWindowExtents();
		const glm::vec2 cursorPos(input->GetMouseState().m_cursorX, input->GetMouseState().m_cursorY);
		glm::vec3 mouseWorldSpace = mainCam.WindowPositionToWorldSpace(cursorPos, windowExtents);
		const glm::vec3 lookDirWorldspace = glm::normalize(mouseWorldSpace - mainCam.Position());
		rayStart = mainCam.Position();
		rayEnd = mainCam.Position() + lookDirWorldspace * rayDistance;
	}

	void DrawEntityBounds(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 colour)
	{
		auto modelDataSys = Systems::GetSystem<ModelDataSystem>();
		auto& imRender = Systems::GetSystem<RenderSystem>()->GetImRenderer();
		auto transformCmp = w.GetComponent<TransformComponent>(e);
		auto staticMeshCmp = w.GetComponent<StaticMeshComponent>(e);
		if (transformCmp && staticMeshCmp)
		{
			auto modelHandle = staticMeshCmp->GetModel();
			auto modelData = modelDataSys->GetModelData(modelHandle);
			if (modelData.m_data)
			{
				glm::vec3 bounds[2] = { modelData.m_data->m_boundsMin, modelData.m_data->m_boundsMax };
				glm::mat4 transform = transformCmp->GetWorldspaceMatrix();
				imRender.DrawAABB(bounds[0], bounds[1], transform, colour);
				return;
			}
		}
		if(transformCmp)
		{
			imRender.AddAxisAtPoint(transformCmp->GetPosition(), transformCmp->GetWorldspaceMatrix());
		}
	}
}
#include "editor_utils.h"
#include "engine/systems/input_system.h"
#include "engine/systems/camera_system.h"
#include "engine/systems/model_data_system.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "engine/utils/intersection_tests.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "render/immediate_renderer.h"
#include "render/camera.h"
#include "render/render_system.h"
#include "core/profiler.h"

namespace R3
{
	Entities::EntityHandle FindClosestActiveEntityIntersectingRay(Entities::World& world, glm::vec3 rayStart, glm::vec3 rayEnd)
	{
		R3_PROF_EVENT();

		struct HitEntityRecord {
			Entities::EntityHandle m_entity;
			float m_hitDistance;
		};
		std::vector<HitEntityRecord> hitEntities;
		auto forEachEntity = [&]<class CmpType>(const Entities::EntityHandle& e, CmpType& smc, TransformComponent& t)
		{
			if (smc.GetShouldDraw())
			{
				const auto modelData = Systems::GetSystem<ModelDataSystem>()->GetModelData(smc.GetModelHandle());
				if (modelData.m_data)
				{
					// transform the ray into model space so we can do a simple AABB test
					const glm::mat4 inverseTransform = glm::inverse(t.GetWorldspaceMatrix(e, world));
					const auto rs = glm::vec3(inverseTransform * glm::vec4(rayStart, 1));
					const auto re = glm::vec3(inverseTransform * glm::vec4(rayEnd, 1));
					float hitT = 0.0f;
					if (RayIntersectsAABB(rs, re, modelData.m_data->m_boundsMin, modelData.m_data->m_boundsMax, hitT))
					{
						hitEntities.push_back({ e, hitT });
					}
				}
			}
			return true;
		};
		Entities::Queries::ForEach<StaticMeshComponent, TransformComponent>(&world, forEachEntity);
		Entities::Queries::ForEach<DynamicMeshComponent, TransformComponent>(&world, forEachEntity);

		// now find the closest hit entity that is in front of the ray
		Entities::EntityHandle closestHit = {};
		float closestHitDistance = FLT_MAX;
		for (int i = 0; i < hitEntities.size(); ++i)
		{
			if (hitEntities[i].m_hitDistance >= 0.0f && hitEntities[i].m_hitDistance < closestHitDistance)
			{
				closestHit = hitEntities[i].m_entity;
				closestHitDistance = hitEntities[i].m_hitDistance;
			}
		}
		return closestHit;
	}

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
		auto& imRender = Systems::GetSystem<ImmediateRenderSystem>()->m_imRender;
		auto transformCmp = w.GetComponent<TransformComponent>(e);
		if (transformCmp)
		{
			auto drawMeshCmp = [&]<class Type>(Type* cmp)
			{
				auto modelHandle = cmp->GetModelHandle();
				auto modelData = modelDataSys->GetModelData(modelHandle);
				if (modelData.m_data)
				{
					glm::vec3 bounds[2] = { modelData.m_data->m_boundsMin, modelData.m_data->m_boundsMax };
					glm::mat4 transform = transformCmp->GetWorldspaceInterpolated(e, w);
					imRender->DrawAABB(bounds[0], bounds[1], transform, colour);
				}
			}; 
			if (auto staticMeshCmp = w.GetComponent<StaticMeshComponent>(e))
			{
				drawMeshCmp(staticMeshCmp);
			}
			else if (auto dynamicMeshCmp = w.GetComponent<DynamicMeshComponent>(e))
			{
				drawMeshCmp(dynamicMeshCmp);
			}
			else
			{
				auto wsMatrix = transformCmp->GetWorldspaceInterpolated(e, w);
				imRender->AddAxisAtPoint(glm::vec3(wsMatrix[3]), 1.0f, wsMatrix);
			}
		}
	}

	void DrawParentLines(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 colour)
	{
		auto& imRender = Systems::GetSystem<ImmediateRenderSystem>()->m_imRender;
		auto childCmp = w.GetComponent<TransformComponent>(e);
		const auto parent = w.GetParent(e);
		auto parentCmp = w.GetComponent<TransformComponent>(parent);
		if (parentCmp != nullptr && childCmp != nullptr)
		{
			ImmediateRenderer::PosColourVertex verts[2];
			verts[0].m_position = childCmp->GetWorldspaceInterpolated(e, w)[3];
			verts[0].m_colour = colour;
			verts[1].m_position = parentCmp->GetWorldspaceInterpolated(parent, w)[3];
			verts[1].m_colour = colour * 0.9f;
			imRender->AddLine(verts);
			DrawParentLines(w, parent, colour * 0.9f);
		}
	}

	void DrawEntityChildren(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 boxColour, glm::vec4 lineColour)
	{
		std::vector<Entities::EntityHandle> results;
		w.GetChildren(e, results);
		for (int c = 0; c < results.size(); ++c)
		{
			DrawParentLines(w, results[c], lineColour);
			DrawEntityChildren(w, results[c], boxColour, lineColour);
		}
		DrawEntityBounds(w, e, boxColour);
	}
}
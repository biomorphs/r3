#include "world_editor_transform_widget.h"
#include "world_editor_window.h"
#include "editor_utils.h"
#include "editor/commands/world_editor_set_entity_positions_cmd.h"
#include "entities/world.h"
#include "engine/systems/camera_system.h"
#include "engine/components/transform.h"
#include "engine/systems/input_system.h"
#include "engine/intersection_tests.h"
#include "render/render_system.h"
#include "render/immediate_renderer.h"

namespace R3
{
	glm::vec3 WorldEditorTransformWidget::FindCenterPosition(const std::vector<Entities::EntityHandle>& entities)
	{
		R3_PROF_EVENT();

		auto world = m_editorWindow->GetWorld();
		glm::vec3 newPosition(0.0f, 0.0f, 0.0f);
		for (auto handle : entities)
		{
			auto transformCmp = world->GetComponent<TransformComponent>(handle);
			if (transformCmp)
			{
				newPosition = newPosition + glm::vec3(transformCmp->GetWorldspaceMatrix()[3]);
			}
		}
		if (entities.size() > 0)
		{
			newPosition = newPosition / static_cast<float>(entities.size());
		}
		return newPosition;
	}

	void WorldEditorTransformWidget::Reset(const std::vector<Entities::EntityHandle>& entities)
	{
		R3_PROF_EVENT();
		// first find the new position of the widget from the world-space positions of the entities
		auto world = m_editorWindow->GetWorld();
		glm::vec3 newPosition = FindCenterPosition(entities);
		m_widgetTransform = glm::translate(newPosition);

		// track the entities + world space transforms for later
		m_trackedEntities.clear();
		for (auto handle : entities)
		{
			auto transformCmp = world->GetComponent<TransformComponent>(handle);
			if (transformCmp)
			{
				m_trackedEntities.push_back({ handle, glm::vec3(transformCmp->GetWorldspaceMatrix()[3]) });
			}
		}
		m_currentEntities = entities;

		// rescale the widget based on current distance to camera
		const float scaleMin = 0.1f;
		const float scaleMax = 256.0f;
		const float scaleMaxDist = 1000.0f;
		auto camPos = Systems::GetSystem<CameraSystem>()->GetMainCamera().Position();
		float distanceToCam = glm::length(glm::vec3(m_widgetTransform[3]) - camPos);
		if (distanceToCam > 0.001f)
		{
			float mixT = glm::min(distanceToCam / scaleMaxDist, 1.0f);
			m_widgetScale = glm::mix(scaleMin, scaleMax, mixT);
		}
	}

	void WorldEditorTransformWidget::RestoreEntityTransforms()
	{
		auto world = m_editorWindow->GetWorld();
		for (auto& it : m_trackedEntities)
		{
			auto transformCmp = world->GetComponent<TransformComponent>(it.m_entity);
			if (transformCmp)
			{
				transformCmp->SetPosition(it.m_originalPosition);
			}
		}
	}

	void WorldEditorTransformWidget::CommitTranslation(glm::vec3 translation)
	{
		auto cmd = std::make_unique<WorldEditorSetEntityPositionsCommand>(m_editorWindow);
		for (auto it : m_trackedEntities)
		{
			cmd->AddEntity(it.m_entity, it.m_originalPosition, it.m_originalPosition + translation);
		}
		m_editorWindow->PushCommand(std::move(cmd));
	}

	bool WorldEditorTransformWidget::HandleMouseOverTranslateHandle(Axis axis,
		glm::vec3 mouseRayStart, glm::vec3 mouseRayEnd, bool mouseDownThisFrame,
		float barWidth, float widgetScale)
	{
		R3_PROF_EVENT();

		auto& imRender = Systems::GetSystem<RenderSystem>()->GetImRenderer();
		glm::vec3 widgetPos = glm::vec3(m_widgetTransform[3]);

		// x
		glm::vec3 boxMin = { 0.0f, -barWidth / 2.0f, -barWidth / 2.0f };
		glm::vec3 boxMax = { widgetScale, barWidth / 2.0f, barWidth / 2.0f };
		glm::vec3 axisDirection = AxisVector(axis);
		glm::vec4 colour = { axisDirection, 1.0f };
		if (axis == Axis::Y)
		{
			boxMin = { -barWidth / 2.0f, 0.0f, -barWidth / 2.0f };
			boxMax = { barWidth / 2.0f, widgetScale, barWidth / 2.0f };
		}
		else if (axis == Axis::Z)
		{
			boxMin = { -barWidth / 2.0f, -barWidth / 2.0f, 0.0f };
			boxMax = { barWidth / 2.0f, barWidth / 2.0f, widgetScale };
		}

		float hitT = 0.0f;
		if (RayIntersectsAABB(mouseRayStart, mouseRayEnd, widgetPos + boxMin, widgetPos + boxMax, hitT))
		{
			colour = { 1.0f,1.0f,1.0f,1.0f };
			if (mouseDownThisFrame && !m_mouseBtnDownLastFrame)
			{
				glm::vec3 widgetAxis = glm::normalize(glm::mat3(m_widgetTransform) * axisDirection) * widgetScale;
				glm::vec3 c0, c1;	// closest points from mouse ray to axis (one point per ray)
				if (GetNearPointsBetweenLines(mouseRayStart, mouseRayEnd, widgetPos, widgetPos + widgetAxis, c0, c1))
				{
					m_mouseDragStartTransform = m_widgetTransform;
					m_mouseClickPosOnAxis = c1;
					m_dragAxis = axis;
					return true;
				}
			}
		}
		imRender.DrawAABB(boxMin, boxMax, m_widgetTransform, colour);

		return false;
	}

	glm::vec3 WorldEditorTransformWidget::AxisVector(Axis axis)
	{
		switch (axis)
		{
		case Axis::X:
			return { 1.0f,0.0f,0.0f };
		case Axis::Y:
			return { 0.0f,1.0f,0.0f };
		case Axis::Z:
			return { 0.0f,0.0f,1.0f };
		default:
			return { 0.0f,0.0f,0.0f };
		}
	}

	WorldEditorTransformWidget::WorldEditorTransformWidget(WorldEditorWindow* w)
		: m_editorWindow(w)
	{
	}

	void WorldEditorTransformWidget::Update(const std::vector<Entities::EntityHandle>& entities)
	{
		R3_PROF_EVENT();
		auto input = Systems::GetSystem<InputSystem>();
		auto world = m_editorWindow->GetWorld();
		auto& imRender = Systems::GetSystem<RenderSystem>()->GetImRenderer();

		if (entities != m_currentEntities)
		{
			Reset(entities);
		}

		if (m_trackedEntities.size() > 0)
		{
			glm::vec3 widgetPos = glm::vec3(m_widgetTransform[3]);
			glm::vec3 mouseRayStartWs, mouseRayEndWs;
			MouseCursorToWorldspaceRay(1000.0f, mouseRayStartWs, mouseRayEndWs);
			glm::mat4 widgetInverse = glm::inverse(m_widgetTransform);
			bool leftBtnDown = false;
			if (!input->IsGuiCapturingInput())
			{
				leftBtnDown = input->GetMouseState().m_buttonState & MouseButtons::LeftButton;
			}

			float widgetScale = m_widgetScale;
			float barWidth = m_widgetScale / 32.0f;

			// dragging...
			if (m_mouseBtnDownLastFrame && m_dragAxis != Axis::None)
			{
				glm::vec3 originalPos = glm::vec3(m_mouseDragStartTransform[3]);
				glm::vec3 widgetAxis = glm::normalize(glm::mat3(m_mouseDragStartTransform) * AxisVector(m_dragAxis)) * widgetScale;
				glm::vec3 c0, c1;	// closest points from mouse ray to axis (one point per ray)
				if (GetNearPointsBetweenLines(mouseRayStartWs, mouseRayEndWs, originalPos, originalPos + widgetAxis, c0, c1))
				{
					imRender.AddAxisAtPoint(c1, 8.0f);
					glm::vec3 translation = c1 - originalPos;
					if (leftBtnDown)
					{
						m_widgetTransform = glm::translate(originalPos);
						for (auto it : m_trackedEntities)
						{
							auto transformCmp = world->GetComponent<TransformComponent>(it.m_entity);
							if (transformCmp)
							{
								glm::vec3 newPos = it.m_originalPosition + translation;
								transformCmp->SetPosition(newPos);	// todo - needs to be set worldspace position
							}
						}
					}
					else    // just let go of mouse
					{
						CommitTranslation(translation);
						Reset(entities);
						m_dragAxis = Axis::None;
					}
				}
			}

			if (HandleMouseOverTranslateHandle(Axis::X, mouseRayStartWs, mouseRayEndWs, leftBtnDown, barWidth, widgetScale))
			{
			}
			else if (HandleMouseOverTranslateHandle(Axis::Y, mouseRayStartWs, mouseRayEndWs, leftBtnDown, barWidth, widgetScale))
			{
			}
			else if (HandleMouseOverTranslateHandle(Axis::Z, mouseRayStartWs, mouseRayEndWs, leftBtnDown, barWidth, widgetScale))
			{
			}

			m_mouseBtnDownLastFrame = leftBtnDown;
		}
	}
}
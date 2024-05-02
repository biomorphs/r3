#pragma once
#include "entities/entity_handle.h"
#include "core/glm_headers.h"
#include <vector>

namespace R3
{
	class WorldEditorWindow;
	class WorldEditorTransformWidget
	{
	public:
		WorldEditorTransformWidget(WorldEditorWindow* w);
		void Update(const std::vector<Entities::EntityHandle>& entities);
	private:
		void RestoreEntityTransforms();
		void Reset(const std::vector<Entities::EntityHandle>& entities);
		void CommitTranslation(glm::vec3 translation);
		glm::vec3 FindCenterPosition(const std::vector<Entities::EntityHandle>& entities);
		enum class Axis {
			None, X, Y, Z
		};
		// Return true if axis was clicked
		bool HandleMouseOverTranslateHandle(Axis axis,
			glm::vec3 mouseRayStart, glm::vec3 mouseRayEnd, bool mouseDownThisFrame,
			float barWidth, float widgetScale);
		glm::vec3 AxisVector(Axis axis);
		struct TrackedEntity
		{
			Entities::EntityHandle m_entity;
			glm::vec3 m_originalPosition;
		};
		std::vector<Entities::EntityHandle> m_currentEntities;
		std::vector<TrackedEntity> m_trackedEntities;
		glm::mat4 m_currentCenterTransform;		// updated each frame
		glm::mat4 m_widgetTransform;			// original transform since dragging started, only updated on reset
		glm::mat4 m_mouseDragStartTransform;	// transform when mouse started being dragged
		glm::vec3 m_mouseClickPosOnAxis;		// position on the original axis where drag started
		bool m_mouseBtnDownLastFrame = false;
		Axis m_dragAxis = Axis::None;
		float m_widgetScale = 1.0f;
		WorldEditorWindow* m_editorWindow = nullptr;
	};
}
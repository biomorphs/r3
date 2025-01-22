#pragma once

#include "core/glm_headers.h"

namespace Entities
{
	class EntityHandle;
	class World;
}

namespace R3
{
	// Searches the active world for any entities intersect the ray
	Entities::EntityHandle FindClosestActiveEntityIntersectingRay(Entities::World& w, glm::vec3 rayStart, glm::vec3 rayEnd);
	void MouseCursorToWorldspaceRay(float rayDistance, glm::vec3& rayStart, glm::vec3& rayEnd);
	void DrawEntityBounds(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 colour);
	void DrawParentLines(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 colour);
	void DrawEntityChildren(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 boxColour, glm::vec4 lineColour);
}
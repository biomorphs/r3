#pragma once

#include "core/glm_headers.h"

namespace Entities
{
	class EntityHandle;
	class World;
}

namespace R3
{
	void MouseCursorToWorldspaceRay(float rayDistance, glm::vec3& rayStart, glm::vec3& rayEnd);
	void DrawEntityBounds(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 colour);	// only works for entities in active world
	void DrawParentLines(Entities::World& w, const Entities::EntityHandle& e, glm::vec4 colour);
}
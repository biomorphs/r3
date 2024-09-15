#include "dda.h"
#pragma once

namespace R3
{
	template<class Intersector>
	inline std::optional<glm::ivec3> DDAIntersect(const glm::vec3& rayStart, const glm::vec3& rayEnd, const glm::vec3& voxelSize, Intersector& intersecter)
	{
		// Rescale ray as if we were acting on a grid with voxel size = 1
		const glm::vec3 scaledRayStart = rayStart / voxelSize;
		const glm::vec3 scaledRayEnd = rayEnd / voxelSize;
		const glm::vec3 rayDirection = scaledRayEnd - scaledRayStart;

		glm::ivec3 currentV(glm::floor(scaledRayStart));		// current voxel indices
		glm::ivec3 vStep(glm::sign(rayDirection));				// step direction in voxels

		// Vectorised DDA parameters
		const glm::vec3 c_threshold(1.0e-6f);
		const auto dirAxisAligned = glm::lessThan(glm::abs(rayDirection), c_threshold);
		const auto dirAxisAlignedInverted = glm::equal(glm::tvec3<bool>(false), dirAxisAligned);
		const glm::vec3 planeDiff = glm::abs((glm::floor(scaledRayStart) + 0.5f) + (glm::vec3(vStep) * 0.5f) - scaledRayStart);

		// Calculate inverse direction (sucks, but I can't see a way to vectorise)
		glm::vec3 inverseDirection;
		inverseDirection.x = dirAxisAligned.x ? 0.0f : (1.0f / rayDirection.x);
		inverseDirection.y = dirAxisAligned.y ? 0.0f : (1.0f / rayDirection.y);
		inverseDirection.z = dirAxisAligned.z ? 0.0f : (1.0f / rayDirection.z);

		// Ray increment per step
		const glm::vec3 rayIncrement = glm::vec3(dirAxisAlignedInverted) * glm::abs(inverseDirection);

		// Ray initial T value
		glm::vec3 rayT;											// current T value for ray in 3 axes
		rayT.x = dirAxisAligned.x ? 1.01f : planeDiff.x * rayIncrement.x;
		rayT.y = dirAxisAligned.y ? 1.01f : planeDiff.y * rayIncrement.y;
		rayT.z = dirAxisAligned.z ? 1.01f : planeDiff.z * rayIncrement.z;

		while (glm::any(glm::lessThanEqual(rayT, glm::vec3(1.0f))))
		{
			if (!intersecter(currentV))
			{
				return currentV;
			}

			// find the dimension with the closest intersection
			int iMinAxis = -1;
			for (int i = 0; i < 3; i++)
			{
				// it's the minimum (or the first)
				if (iMinAxis == -1 || rayT[i] < rayT[iMinAxis])
				{
					iMinAxis = i;
				}
			}

			// move to next cell along the dimension of minimum intersection
			currentV[iMinAxis] += vStep[iMinAxis];
			rayT[iMinAxis] += rayIncrement[iMinAxis];
		}

		return {};	// nothing was hit
	}

	template<class Intersector>
	std::optional<glm::ivec2> BresenhamsLineIntersect(const glm::ivec2& rayStart, const glm::ivec2& rayEnd, Intersector& intersecter)
	{
		int x0 = rayStart.x;
		int y0 = rayStart.y;
		int x1 = rayEnd.x;
		int y1 = rayEnd.y;
		int dx = abs(x1 - x0);
		int sx = x0 < x1 ? 1 : -1;
		int dy = -abs(y1 - y0);
		int sy = y0 < y1 ? 1 : -1;
		int err = dx + dy;
		int e2 = 0;
		for (;;) 
		{
			auto currentPos = glm::ivec2(x0, y0);
			if (!intersecter(currentPos))
			{
				return currentPos;
			}
			if (x0 == x1 && y0 == y1)
			{
				break;
			}
			e2 = 2 * err;
			if (e2 >= dy) 
			{ 
				err += dy; 
				x0 += sx; 
			} // e_xy+e_x > 0
			if (e2 <= dx) 
			{ 
				err += dx; 
				y0 += sy; 
			} // e_xy+e_y < 0
		}
		return {};
	}
}
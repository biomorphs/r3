#pragma once
#include <glm/glm.hpp>
#include <optional>

// Implementation of the 3d-DDA algorithm described in 
// "Efficient implementation of the 3D-DDA ray traversal algorithm
// on GPU and its application in radiation dose calculation." - Kai Xiao, Jun 03, 2014 

namespace R3
{
	// Performs DDA algorithm in 3d space, calling a custom intersect function
	//	Takes a structure/class/whatever evaluating to a single function, 
	//	bool fn(const glm::ivec3& p)
	//	fn is called for all intersecting voxels on the path
	//	If the intersector returns false, a hit is detected and the function returns the hit position
	// returns a hit voxel or nothing if the entire path was traversed
	template<class Intersector>	
	std::optional<glm::ivec3> DDAIntersect(const glm::vec3& rayStart, const glm::vec3& rayEnd, const glm::vec3& voxelSize, Intersector& intersecter);

	// Similar API for bresenham's line algorithm, but for 2d integer coords
	template<class Intersector>
	std::optional<glm::ivec2> BresenhamsLineIntersect(const glm::ivec2& rayStart, const glm::ivec2& rayEnd, Intersector& intersecter);
}

#include "dda.inl"
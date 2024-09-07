#pragma once

// ALWAYS use this to include glm stuff
// It enables various features required to get decent perf in debug
#define GLM_FORCE_DEPTH_ZERO_TO_ONE				// gl uses -1 to 1 on z clip space, vulkan uses 0 to 1
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_EXPLICIT_CTOR					//disable implicit conversions between int and float types
#define GLM_FORCE_INLINE						// always inline
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES		// don't pack types, required for simd
#define GLM_FORCE_AVX
#ifndef NDEBUG
#define I_DISABLED_DEBUG
#define NDEBUG
#endif

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/fast_square_root.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/closest_point.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/hash.hpp>


#ifdef I_DISABLED_DEBUG
#undef NDEBUG
#endif

namespace R3
{
	// Align up to a power of two value
	template<class IntType>
	constexpr IntType AlignUpPow2(IntType v, IntType alignment)
	{
		intptr_t int_ptr = static_cast<intptr_t>(v);
		return int_ptr + (-int_ptr & (alignment - 1));
	}

	inline void Decompose4(const glm::mat4& m, glm::vec3& pos, glm::vec3& scale, glm::quat& rot)
	{
		glm::vec3 skew;	// we don't care about skew
		glm::vec4 perspective;	// or perspective
		glm::decompose(m, scale, rot, pos, skew, perspective);
	}

	inline glm::mat4 InterpolateMat4(const glm::mat4& m0, const glm::mat4& m1, float alpha)	// alpha = 0 to 1
	{
		glm::mat4 result;

		// decompose and interpolate components separately (allows us to handle scale properly)
		glm::vec3 pos0(m0[3]), pos1(m1[3]);
		glm::vec3 scale0, scale1;
		glm::quat rot0, rot1;
		Decompose4(m0, pos0, scale0, rot0);
		Decompose4(m1, pos1, scale1, rot1);
		glm::vec3 p = glm::mix(pos0, pos1, alpha);
		glm::vec3 s = glm::mix(scale0, scale1, alpha);
		glm::quat finalRot = glm::slerp(rot0, rot1, alpha);

		// rebuild the transform
		result = glm::translate(glm::identity<glm::mat4>(), p);
		result = result * glm::toMat4(finalRot);
		result = glm::scale(result, s);

		return result;
	}
}

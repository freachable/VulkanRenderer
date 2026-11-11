#pragma once

#include <array>

#include <glm/glm.hpp>

// View frustum as six inward-facing planes, for culling bounding boxes on the CPU.
struct Frustum
{
	std::array<glm::vec4, 6> planes{};  // (normal, distance): inside when dot(normal, p) + distance >= 0

	// Gribb/Hartmann plane extraction. Assumes Vulkan clip space with depth in [0, 1]
	// (GLM_FORCE_DEPTH_ZERO_TO_ONE).
	static Frustum fromViewProjection(glm::mat4 const& m)
	{
		auto row = [&m](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };

		Frustum frustum;
		frustum.planes =
		{
			row(3) + row(0),  // left
			row(3) - row(0),  // right
			row(3) + row(1),  // bottom
			row(3) - row(1),  // top
			row(2),           // near (z >= 0)
			row(3) - row(2)   // far
		};

		for (glm::vec4& plane : frustum.planes)
		{
			plane /= glm::length(glm::vec3(plane));
		}

		return frustum;
	}

	// Conservative: may report boxes near frustum corners as visible, never the reverse.
	bool intersectsAabb(glm::vec3 const& boundsMin, glm::vec3 const& boundsMax) const
	{
		for (glm::vec4 const& plane : planes)
		{
			// The box corner furthest along the plane normal.
			glm::vec3 const corner
			{
				plane.x >= 0.0f ? boundsMax.x : boundsMin.x,
				plane.y >= 0.0f ? boundsMax.y : boundsMin.y,
				plane.z >= 0.0f ? boundsMax.z : boundsMin.z
			};

			if (glm::dot(glm::vec3(plane), corner) + plane.w < 0.0f)
			{
				return false;
			}
		}

		return true;
	}
};

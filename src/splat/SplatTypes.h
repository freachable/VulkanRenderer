#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

// Matches UniformBufferObject in src/graphics/renderer/FrameResources.hpp
struct FrameUniforms
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::vec3 cameraPos;      // World space.
	alignas(16) glm::vec4 viewportParams; // x = width, y = height (pixels), z = focal_x, w = focal_y (pixels).
};

// Input 3D Gaussian
// Assumes CPU loader has already activated opacity in [0, 1] and scale > 0.
struct Gaussian3D
{
	alignas(16) glm::vec4 posOpacity; // xyz = world position (Z-up), w = opacity
	alignas(16) glm::vec4 scale;      // xyz = 3D scale, w = unused
	alignas(16) glm::vec4 rot;        // xyzw = unit quaternion (x, y, z, w)
	alignas(16) glm::vec4 color;      // rgb = SH0 base color, w = unused
};

// Output 2D projected Gaussian (written by project.slang, read by P3 billboard vertex/fragment shaders).
struct ProjectedSplat2D
{
	alignas(16) glm::vec4 ndcCenterDepth; // xy = NDC center [-1, 1], z = NDC depth [0, 1], w = view-space depth
	alignas(16) glm::vec4 axes;           // xy = major 3-sigma axis (NDC), zw = minor 3-sigma axis (NDC)
	alignas(16) glm::vec4 conicOpacity;   // xyz = inverse 2D covariance (a, b, c), w = opacity
	alignas(16) glm::vec4 color;          // rgb = color, w = unused
};

// Matches VkDrawIndirectCommand (16 bytes).
// project.slang atomically increments instanceCount for each surviving splat.
struct DrawIndirectCommand
{
	uint32_t vertexCount   = 6; // 2 triangles per billboard quad
	uint32_t instanceCount = 0;
	uint32_t firstVertex   = 0;
	uint32_t firstInstance = 0;
};

// Small push constants for P2 compute dispatches.
struct SplatComputePushConstants
{
	uint32_t numGaussians = 0;
	uint32_t shift        = 0; // Radix sort bit offset: 0, 8, 16, 24
};

// Compile-time layout checks to guarantee std430 / Slang compatibility.
static_assert(sizeof(Gaussian3D) == 64);
static_assert(sizeof(ProjectedSplat2D) == 64);
static_assert(sizeof(DrawIndirectCommand) == 16);
static_assert(offsetof(DrawIndirectCommand, instanceCount) == 4);
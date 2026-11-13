#pragma once

#include <cstdint>
#include <vector>

// Image pixels in CPU memory: 8-bit RGBA, rows top to bottom, tightly packed (width * height * 4 bytes).
// Holds no GPU resources; upload it with graphics/gpu/Texture.
struct ImageData
{
	uint32_t             width  = 0;
	uint32_t             height = 0;
	std::vector<uint8_t> pixels;
};

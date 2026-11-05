#pragma once

// Single place that decides how Vulkan-Hpp is pulled in: as the C++20 module
// (ENABLE_CPP20_MODULE=ON) or as the classic RAII header.
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

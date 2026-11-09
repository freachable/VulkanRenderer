#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"

#include <cstdint>
#include <string>
#include <utility>

class Buffer;

class Texture
{
public:
	Texture(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::string const& path);

	vk::raii::Image const& getImage() const { return textureImage; }
	vk::raii::ImageView const& getImageView() const { return textureImageView; }

private:
	void createTextureImage(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::string const& path);
	void createTextureImageView(VulkanContext const& context);

	static std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
	    VulkanContext const& context, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
	    vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

	void transitionImageLayout(vk::raii::CommandBuffer& commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
	void copyBufferToImage(vk::raii::CommandBuffer& commandBuffer, Buffer const& buffer, uint32_t width, uint32_t height);

	// Destroyed in reverse order: view, then image, then memory.
	vk::raii::DeviceMemory textureImageMemory = nullptr;
	vk::raii::Image        textureImage       = nullptr;
	vk::raii::ImageView    textureImageView   = nullptr;
};

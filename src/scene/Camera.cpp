#include "Camera.hpp"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
	constexpr glm::vec3 worldUp { 0.0f, 0.0f, 1.0f };
	constexpr float moveSpeed        = 2.0f;   // Units per second.
	constexpr float mouseSensitivity = 0.003f; // Radians per pixel.
}

void Camera::update(Window& window, float dt)
{
	bool const looking = window.isRightMouseDown();
	window.setCursorCaptured(looking);

	// Always consume the delta, so movement while not looking doesn't pile up.
	glm::vec2 const delta = window.consumeMouseDelta();
	if (!looking)
	{
		return;
	}

	yaw  -= delta.x * mouseSensitivity;
	pitch = glm::clamp(pitch - delta.y * mouseSensitivity, glm::radians(-89.0f), glm::radians(89.0f));

	glm::vec3 const forward = getForward();
	glm::vec3 const right   = glm::normalize(glm::cross(forward, worldUp));

	glm::vec3 move{ 0.0f };

	if (window.isKeyDown(Key::W)) move += forward;
	if (window.isKeyDown(Key::S)) move -= forward;
	if (window.isKeyDown(Key::D)) move += right;
	if (window.isKeyDown(Key::A)) move -= right;
	if (window.isKeyDown(Key::E)) move += worldUp;
	if (window.isKeyDown(Key::Q)) move -= worldUp;

	if (glm::length(move) > 0.0f)
	{
		position += glm::normalize(move) * moveSpeed * dt;
	}
}

glm::mat4 Camera::getView() const
{
	return glm::lookAt(position, position + getForward(), worldUp);
}

glm::mat4 Camera::getProjection(float aspect) const
{
	return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

glm::vec3 Camera::getForward() const
{
	return { std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch) };
}

#pragma once

#include "platform/Window.hpp"

#include <glm/glm.hpp>

// Free-fly camera: hold the right mouse button to look around, and move with WASD (Q/E for down/up).
// The world is Z-up.
class Camera
{
public:
	void update(Window& window, float dt);

	glm::mat4 getView() const;
	glm::mat4 getProjection(float aspect) const;

private:
	glm::vec3 getForward() const;

	glm::vec3 position{ 2.0f, 2.0f, 2.0f };
	float yaw   = glm::radians(-135.0f); // Around +Z; 0 looks along +X.
	float pitch = glm::radians(-35.0f);  // Positive looks up.
};

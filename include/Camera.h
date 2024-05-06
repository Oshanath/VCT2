#ifndef CAMERA_H
#define CAMERA_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct ViewProjectionMatrices {
	glm::mat4 view;
	glm::mat4 proj;
};

class Camera
{
public:
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;

	float moveSpeed = 1000.0;
	float deltaTime;

	bool movingForward = false;
	bool movingBackward = false;
	bool movingLeft = false;
	bool movingRight = false;
	bool movingUp = false;
	bool movingDown = false;
	bool freeLook = false;

	Camera(glm::vec3 position, glm::vec3 target);

	ViewProjectionMatrices getViewProjectionMatrices(float width, float height);

	void move();
	void mouse_callback(double xpos, double ypos);
};

#endif // !CAMERA_H

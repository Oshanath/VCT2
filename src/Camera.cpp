#include "Camera.h"

#include <iostream>

Camera::Camera(glm::vec3 position, glm::vec3 target)
{
	this->position = position;
	this->worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	this->front = glm::normalize(target - position);
	this->right = glm::normalize(glm::cross(this->front, this->worldUp));
	this->up = glm::normalize(glm::cross(this->right, this->front));
}

ViewProjectionMatrices Camera::getViewProjectionMatrices(float width, float height)
{
	ViewProjectionMatrices matrices;

	matrices.view = glm::lookAt(this->position, this->position + this->front, this->up);
	matrices.proj = glm::perspective(glm::radians(45.0f), width / height, 10.0f, 1000000.0f);
	matrices.proj[1][1] *= -1;

	return matrices;
}

void Camera::move()
{
	if (this->movingForward)
	{
		this->position += this->front * this->moveSpeed * this->deltaTime;
	}
	if (this->movingBackward)
	{
		this->position -= this->front * this->moveSpeed * this->deltaTime;
	}
	if (this->movingLeft)
	{
		this->position -= this->right * this->moveSpeed * this->deltaTime;
	}
	if (this->movingRight)
	{
		this->position += this->right * this->moveSpeed * this->deltaTime;
	}
	if (this->movingUp)
	{
		this->position += this->up * this->moveSpeed * this->deltaTime;
	}
	if (this->movingDown)
	{
		this->position -= this->up * this->moveSpeed * this->deltaTime;
	}
}

void Camera::mouse_callback(double xpos, double ypos)
{
	if (freeLook)
	{
		static double lastX = xpos;
		static double lastY = ypos;

		double xoffset = lastX - xpos;
		double yoffset = lastY - ypos;

		lastX = xpos;
		lastY = ypos;

		float sensitivity = 0.1f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		this->front = glm::normalize(glm::rotate(glm::mat4(1.0f), glm::radians((float)xoffset), this->up) * glm::vec4(this->front, 0.0f));
		this->front = glm::normalize(glm::rotate(glm::mat4(1.0f), glm::radians((float)yoffset), this->right) * glm::vec4(this->front, 0.0f));
		this->right = glm::normalize(glm::cross(this->front, this->worldUp));
		this->up = glm::normalize(glm::cross(this->right, this->front));
	}
}
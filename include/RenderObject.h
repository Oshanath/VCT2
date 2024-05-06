#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include "Helper.h"

class RenderObject
{
public:
	std::shared_ptr<Helper> helper;
	std::shared_ptr<Model> model;

	glm::vec3 position = glm::vec3(0.0f);
	glm::mat4 rotation = glm::mat4(1.0f);
	float scale = 1.0f;

	inline RenderObject(std::shared_ptr<Helper> helper, std::shared_ptr<Model> model) : helper(helper), model(model) {}

	inline glm::mat4 getModelMatrix()
	{
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
		modelMatrix = modelMatrix * rotation;
		modelMatrix = glm::translate(modelMatrix, position);
		return modelMatrix;
	}
};

#endif // !RENDER_OBJECT_H

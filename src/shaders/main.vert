#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout( push_constant ) uniform constants{
	mat4 model;
} PushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragPosition;
layout(location = 2) out vec3 fragNormal;

void main() {

    vec4 world_pos = PushConstants.model * vec4(inPosition, 1.0);
    fragPosition = world_pos.xyz;
    fragTexCoord = inTexCoord;
    fragNormal = mat3(PushConstants.model) * inNormal.xyz;

    gl_Position = ubo.proj * ubo.view * PushConstants.model * vec4(inPosition, 1.0);
}
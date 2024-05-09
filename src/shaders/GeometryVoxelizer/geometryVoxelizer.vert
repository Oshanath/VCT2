#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout (location = 0) out vec3 GS_IN_FragPos;
layout (location = 1) out vec3 GS_IN_Normal;
layout (location = 2) out vec2 GS_IN_Texcoord;

layout( push_constant ) uniform constants{
	mat4 model;
} pc;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

void main() 
{
    // Transform position into world space
	vec4 world_pos = pc.model * vec4(inPosition, 1.0);

    // Pass world position into Fragment shader
    GS_IN_FragPos = world_pos.xyz;

    GS_IN_Texcoord = inTexCoord;

    // Transform world position into clip space
	gl_Position = ubo.proj * ubo.view * world_pos;
	
    // Transform vertex normal into world space
    mat3 normal_mat = mat3(pc.model);

	GS_IN_Normal = normal_mat * inNormal;
}
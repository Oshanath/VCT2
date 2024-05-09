#version 450

layout (location = 0) in vec3 FS_IN_FragPos;
layout (location = 1) in vec3 FS_IN_Normal;
layout (location = 2) in vec2 FS_IN_Texcoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout (set = 0, binding = 1) uniform voxelGridUBO{
	vec4 aabb_min;
	vec4 aabb_max;
} voxelGrid;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexture;

layout(set = 2, binding = 0, rgba8) uniform image3D voxelTexture;

int get_index(float boundary1, float boundary2, float value, int voxels_per_side)
{
	return int((value - boundary1) / (boundary2 - boundary1) * voxels_per_side);
}

void main()
{
	vec3 _min = voxelGrid.aabb_min.xyz;
	vec3 _max = voxelGrid.aabb_max.xyz;
	int voxels_per_side = imageSize(voxelTexture).x;
	float voxel_width = (_max.x - _min.x) / float(voxels_per_side);
	ivec3 voxel_coordinate = ivec3((FS_IN_FragPos - _min) / voxel_width);

	vec3 diffuse = texture(diffuseTexture, FS_IN_Texcoord).xyz;
	const vec4 current_voxel_value = imageLoad(voxelTexture, voxel_coordinate);
	const vec4 voxel_value = vec4(diffuse, 1.0);

	imageStore(voxelTexture, voxel_coordinate, voxel_value);
}
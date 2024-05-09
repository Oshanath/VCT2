#version 450

layout (location = 0) out vec4 FS_OUT_Color;

layout(std140, set=1, binding = 1) buffer InstanceColorBuffer {
   vec4 colors[];
};

// Take instance index as a flat input
layout (location = 1) flat in int instanceIndex;

void main()
{
	FS_OUT_Color = vec4(colors[instanceIndex].xyz, 1.0);
}
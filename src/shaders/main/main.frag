#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragPosition;
layout(location = 2) in vec3 fragNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout (set = 0, binding = 1) uniform LightsUBO {	
    vec4 direction;
} lights;

layout (set = 0, binding = 2) uniform LightSpaceMatrix {	
    mat4 matrix;
} lightSpaceMatrix;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexture;

layout(set = 2, binding = 0) uniform sampler2D shadow_map;

layout(location = 0) out vec4 outColor;

float ambient = 0.03;

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadow_map, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambient;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadow_map, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main()
{
    vec3 light_dir = lights.direction.xyz;
	vec3 n = normalize(fragPosition);

	float lambert = max(0.0f, dot(n, -light_dir));

    vec3 diffuse = texture(diffuseTexture, fragTexCoord).xyz;
	vec3 ambient = diffuse * ambient;

	vec4 FragPosLightSpace = lightSpaceMatrix.matrix * vec4(fragPosition, 1.0);

	vec4 fragNDCCoords = FragPosLightSpace / FragPosLightSpace.w;

	fragNDCCoords.xy = fragNDCCoords.xy * 0.5 + 0.5;

	float closestDepth = texture(shadow_map, fragNDCCoords.xy).r;
	float currentDepth = fragNDCCoords.z;
	float shadowValue = filterPCF(fragNDCCoords);

	vec3 color = diffuse * lambert * shadowValue + ambient;

	// HDR tonemapping
    //color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0 / 1.2));

	outColor = vec4(color, 1.0);
}
#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragPosition;
layout(location = 2) in vec3 fragNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
	vec4 cameraPosition;
} ubo;

layout (set = 0, binding = 1) uniform LightsUBO {	
    vec4 direction;
} lights;

layout (set = 0, binding = 2) uniform LightSpaceMatrix {	
    mat4 matrix;
} lightSpaceMatrix;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexture;

layout(set = 2, binding = 0) uniform sampler2D shadow_map;

layout(set = 3, binding = 0, rgba8) uniform image3D voxelTexture[];

layout (set = 4, binding = 1) uniform voxelGridUBO{
	vec4 aabb_min;
	vec4 aabb_max;
} voxelGrid;

layout(set = 5, binding = 0) uniform sampler2D noiseTexture;

layout( push_constant ) uniform constants{
	mat4 model;
	float occlusionDecayFactor;
	bool ambientOcclusionEnabled;
	bool visualizeOcclusion;
	float surfaceOffset;
	float coneCutoff;
} PushConstants;

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

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

float calculateVoxelWidth(int mipLevel)
{
	return (voxelGrid.aabb_max.x - voxelGrid.aabb_min.x) / float(imageSize(voxelTexture[mipLevel]).x);
}

uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

uint hash( uvec2 v ) {
    return hash( v.x ^ hash(v.y) );
}

uint hash( uvec3 v ) {
    return hash( v.x ^ hash(v.y) ^ hash(v.z) );
}

uint hash( uvec4 v ) {
    return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) );
}

float random( float f ) {
    const uint mantissaMask = 0x007FFFFFu;
    const uint one          = 0x3F800000u;
   
    uint h = hash( floatBitsToUint( f ) );
    h &= mantissaMask;
    h |= one;
    
    float  r2 = uintBitsToFloat( h );
    return r2 - 1.0;
}

float random( vec2 f ) {
    const uint mantissaMask = 0x007FFFFFu;
    const uint one          = 0x3F800000u;
   
    uint h = hash(uvec2(floatBitsToUint(f.x), floatBitsToUint(f.y)));
    h &= mantissaMask;
    h |= one;
    
    float  r2 = uintBitsToFloat( h );
    return r2 - 1.0;
}

float random( vec3 f ) {
    const uint mantissaMask = 0x007FFFFFu;
    const uint one          = 0x3F800000u;
   
    uint h = hash(uvec3(floatBitsToUint(f.x), floatBitsToUint(f.y), floatBitsToUint(f.z)));
    h &= mantissaMask;
    h |= one;
    
    float  r2 = uintBitsToFloat( h );
    return r2 - 1.0;
}

bool isInsideVoxelGrid(vec3 position){
	return position.x >= voxelGrid.aabb_min.x || position.x <= voxelGrid.aabb_max.x ||
		position.y >= voxelGrid.aabb_min.y || position.y <= voxelGrid.aabb_max.y ||
		position.z >= voxelGrid.aabb_min.z || position.z <= voxelGrid.aabb_max.z;
}

float calculateAmbientOcclusion(){

	int levels = int(log2(imageSize(voxelTexture[0]).x) + 1);

	vec3 normal = normalize(fragNormal);

	if(dot(normal, ubo.cameraPosition.xyz - fragPosition) < 0) 
		normal = -normal;
	
	vec3 position = fragPosition + normal * PushConstants.surfaceOffset;

	#define CONE_COUNT 7
	#define CONE_HALF_ANGLE 45.0

	vec3 directions[CONE_COUNT];
	directions[0] = normal;
	uint randomCounter = 0;

	for(uint i = 1; i < CONE_COUNT; i++)
	{
		directions[i] = vec3(random(vec3(gl_FragCoord.x, i, directions[i-1].x)), random(vec3(gl_FragCoord.y, i, directions[i-1].y)), random(vec3(gl_FragCoord.z, i, directions[i-1].z)));
		directions[i] *= 2.0;
		directions[i] -= vec3(1.0);
		directions[i] = normalize(directions[i]);

		if(dot(normal, directions[i]) < 0.0)
		{
			directions[i] = -directions[i];
		}
	}

	float occlusion = 0.0f;

	for(int i = 0; i < CONE_COUNT; i++)
	{
		int currentMipLevel = 0;
		float voxelWidth = calculateVoxelWidth(currentMipLevel);

		vec3 sampleLocation = position + directions[i] * voxelWidth * 1.75;

		ivec3 currentVoxel = ivec3((position - voxelGrid.aabb_min.xyz) / voxelWidth);
		ivec3 sampleVoxel = ivec3((sampleLocation - voxelGrid.aabb_min.xyz) / voxelWidth);

		float sampleLength = length(sampleLocation - position);
		float radius = sampleLength * tan(radians(CONE_HALF_ANGLE));
		float coneOcclusion = 0.0f;

		while(sampleLength < PushConstants.coneCutoff)
		{
			if(radius > voxelWidth){
				currentMipLevel++;
				voxelWidth = calculateVoxelWidth(currentMipLevel);
			}

			ivec3 voxelCoord = ivec3((sampleLocation - voxelGrid.aabb_min.xyz) / voxelWidth);
			float currentOcclusion = imageLoad(voxelTexture[currentMipLevel], voxelCoord).w;
			currentOcclusion += (1.0 / (1.0 + PushConstants.occlusionDecayFactor * sampleLength)) * currentOcclusion;
			coneOcclusion = coneOcclusion + (1 - coneOcclusion) * currentOcclusion;

			sampleLocation += directions[i] * voxelWidth;
			sampleLength = length(sampleLocation - position);
			radius = sampleLength * tan(radians(CONE_HALF_ANGLE));

		}

		occlusion += coneOcclusion;
	}
	
	return occlusion / 4.0;

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

	vec3 color;

	if(PushConstants.ambientOcclusionEnabled)
	{
		float ambientOcclusion = calculateAmbientOcclusion();

		if(PushConstants.visualizeOcclusion)
		{
			color = vec3(1.0 - ambientOcclusion);
		}
		else{
			color = diffuse * lambert * shadowValue + ambient * (1.0 - ambientOcclusion);
		}
	}
	else{
		color = diffuse * lambert * shadowValue + ambient;
	}

	// HDR tonemapping
    //color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0 / 1.2));

	outColor = vec4(color, 1.0);
}
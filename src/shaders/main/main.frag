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

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

float ambient = 0.03;

void main() {
    vec3 light_dir = lights.direction.xyz;
    vec3 n = normalize(fragNormal);
    float lambert = max(0.0f, dot(n, -light_dir));
    vec3 diffuse = texture(texSampler, fragTexCoord).xyz;
    vec3 ambient = diffuse * ambient;

    vec3 color = diffuse * lambert + ambient;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    //color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
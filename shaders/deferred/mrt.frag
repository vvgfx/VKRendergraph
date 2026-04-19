#version 450

#extension GL_GOOGLE_include_directive : require
#include "mrt_input_structures.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inPosition;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outMetalllicRoughness;

void main()
{
    outPosition = vec4(inPosition, 1.0f);
    outNormal = vec4(normalize(inNormal), 1.0f);

    vec3 textureColor = inColor * texture(colorTex, inUV).xyz;
    // convert to linear space.
    textureColor = pow(textureColor, vec3(2.2));

    outAlbedo = vec4(textureColor, 1.0f);

    vec2 metalRough = texture(metalRoughTex, inUV).bg;
    float metallic = metalRough.x * materialData.metal_rough_factors.x;
    float roughness = metalRough.y * materialData.metal_rough_factors.y;

    outMetalllicRoughness.x = metallic;
    outMetalllicRoughness.y = roughness;
    outMetalllicRoughness.z = 0.0f;
    outMetalllicRoughness.w = 0.0f;
}

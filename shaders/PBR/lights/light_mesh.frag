#version 450

#extension GL_GOOGLE_include_directive : require
#include "light_input_structures.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inPos;

layout(location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
// ----------------------------------------------------------------------------

void main()
{
    vec3 viewVec, lightVec, halfwayVec;
    vec3 tempNormal;
    float dist, attenuation;
    vec3 radiance, specular;
    vec3 kS, kD;
    float nDotL;
    float NDF;
    float G;
    vec3 F;

    tempNormal = normalize(inNormal); // world space
    // if (!gl_FrontFacing)
    //     tempNormal = -tempNormal;
    viewVec = (normalize(sceneData.cameraPos - inPos)).xyz; // world space.
    vec3 albedo = pow(texture(colorTex, vec2(inUV.s, inUV.t)).rgb, vec3(2.2)); // conversion from sRGB
    // to linear space
    vec3 normal = tempNormal;
    vec2 metalRough = texture(metalRoughTex, inUV).bg;
    float metallic = metalRough.x * materialData.metal_rough_factors.x;
    float roughness = metalRough.y * materialData.metal_rough_factors.y;
    float ao = 1;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0f);

    for (int i = 0; i < lightData.numLights; i++)
    {
        PointLight currLight = lightData.pointLights[i];
        vec3 lightPos = currLight.transform[3].xyz;
        vec3 lightDistVec = lightPos - inPos.xyz;
        dist = length(lightDistVec);
        lightVec = lightDistVec / dist;

        halfwayVec = normalize(viewVec + lightVec);

        // attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        attenuation = 1.0 / (dist * dist);
        radiance = currLight.color * attenuation * currLight.intensity;

        NDF = DistributionGGX(normal, halfwayVec, roughness);
        G = GeometrySmith(normal, viewVec, lightVec, roughness);
        F = FresnelSchlick(clamp(dot(halfwayVec, viewVec), 0.0f, 1.0f), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, viewVec), 0.0f) * max(dot(normal, lightVec), 0.0f) + 0.001;
        specular = numerator / denominator;

        kS = F; // specular coefficient is equal to fresnel
        kD = vec3(1.0f) - kS;
        kD *= 1.0 - metallic;

        nDotL = max(dot(normal, lightVec), 0.0f);

        Lo += (kD * albedo / PI + specular) * radiance * nDotL;
    }

    vec3 ambient = vec3(0.03f) * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = ACESFilm(color); // this tonemapping seems to preserve blacks better.
    // gamma correct
    color = pow(color, vec3(1.0 / 2.2));

    outFragColor = vec4(color, 1.0);
}

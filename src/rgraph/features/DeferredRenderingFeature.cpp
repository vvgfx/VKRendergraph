#include "rgraph/features/DeferredRenderingFeature.h"
#include "DeferredRenderingFeature.h"
#include "rgraph/Rendergraph.h"
#include "vk_pipelines.h"
#include <vulkan/vulkan_core.h>

rgraph::DeferredRenderingFeature::DeferredRenderingFeature(DrawContext &drawContext, VkDevice _device, GPUSceneData &gpuSceneData,
                                                           VkDescriptorSetLayout gpuSceneLayout, MaterialSystemCreateInfo &materialSystemCreateInfo,
                                                           DeletionQueue &delQueue)
    : drawContext(drawContext), gpuSceneData(gpuSceneData)
{
    _gpuSceneDataDescriptorLayout = gpuSceneLayout;

    // create descriptor set for lights.
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        lightDescriptorSetLayout = layoutBuilder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    createPipelines(materialSystemCreateInfo);
}

void rgraph::DeferredRenderingFeature::Register(rgraph::Rendergraph *builder)
{
}

void rgraph::DeferredRenderingFeature::createPipelines(MaterialSystemCreateInfo &materialSystemCreateInfo)
{

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../shaders/deferred.frag.spv", materialSystemCreateInfo._device, &meshFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module\n");
    }

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("../shaders/deferred.vert.spv", materialSystemCreateInfo._device, &meshVertexShader))
    {
        fmt::println("Error when building the triangle vertex shader module\n");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout materialLayout = VulkanEngine::Instance().GetMaterialSystem().materialLayout;

    VkDescriptorSetLayout layouts[] = {materialSystemCreateInfo._gpuSceneDataDescriptorLayout, lightDescriptorSetLayout, materialLayout};
}
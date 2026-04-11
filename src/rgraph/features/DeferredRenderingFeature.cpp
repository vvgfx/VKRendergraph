#include "rgraph/features/DeferredRenderingFeature.h"
#include "DeferredRenderingFeature.h"
#include "rgraph/Rendergraph.h"

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

void rgraph::DeferredRenderingFeature::createPipelines(MaterialSystemCreateInfo &materialSystemCreateInfo)
{
}

void rgraph::DeferredRenderingFeature::Register(rgraph::Rendergraph *builder)
{
}
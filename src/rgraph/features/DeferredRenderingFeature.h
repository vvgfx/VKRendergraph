#pragma once
#include "../IFeature.h"
#include "MaterialSystem.h"
#include "vk_engine.h"
#include "vk_types.h"

namespace rgraph
{

    /**
     * @brief An implementation of IFeature that does deferred rendering
     *
     */
    class DeferredRenderingFeature : public IFeature
    {
      public:
        DeferredRenderingFeature(DrawContext &drawContext, VkDevice _device, GPUSceneData &gpuSceneData, VkDescriptorSetLayout gpuSceneLayout,
                                 MaterialSystemCreateInfo &materialSystemCreateInfo, DeletionQueue &delQueue);

        void Register(Rendergraph *builder) override;

      private:
        void createPipelines(MaterialSystemCreateInfo &materialSystemCreateInfo);
        // execution lambdas for run.
        void renderScene(PassExecution &passExec);

        VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
        VkDescriptorSetLayout lightDescriptorSetLayout;
        VkDescriptorSetLayout compDescriptorSetLayout;

        MaterialPipeline geometryPipeline;
        MaterialPipeline compositePipeline;

        DrawContext &drawContext;
        GPUSceneData &gpuSceneData;
    };
} // namespace rgraph

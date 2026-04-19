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
        // lighting data struct.
        struct PointLight
        {
            glm::mat4 transform;
            glm::vec3 color;
            float intensity;
        };

        struct LightData
        {
            PointLight pointLights[25];
            int numLights;
        };

        void createPipelines(MaterialSystemCreateInfo &materialSystemCreateInfo);

        void createImages(DeletionQueue &delQueue);
        // execution lambdas for run.
        void geometryPass(PassExecution &passExec);

        void compositePass(PassExecution &passExec);

        // images
        AllocatedImage position_gbuf;
        AllocatedImage normal_gbuf;
        AllocatedImage albedo_gbuf;
        AllocatedImage metalrough_gbuf;
        AllocatedImage depth_gbuf;

        VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
        VkDescriptorSetLayout lightDescriptorSetLayout;
        VkDescriptorSetLayout compDescriptorSetLayout;

        MaterialPipeline geometryPipeline;
        MaterialPipeline compositePipeline;

        DrawContext &drawContext;
        GPUSceneData &gpuSceneData;
    };
} // namespace rgraph

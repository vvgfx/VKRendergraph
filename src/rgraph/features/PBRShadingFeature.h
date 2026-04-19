#pragma once
#include "../IFeature.h"
#include "MaterialSystem.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "vk_engine.h"
#include "vk_types.h"

namespace rgraph
{

    /**
     * @brief An implementation of IFeature that does simple PBR Shading
     *
     */
    class PBRShadingFeature : public IFeature
    {
      public:
        PBRShadingFeature(DrawContext &drwCtx, VkDevice _device, MaterialSystemCreateInfo &materialSystemCreateInfo, GPUSceneData &sceneData,
                          VkDescriptorSetLayout gpuSceneLayout, DeletionQueue &delQueue);

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
        // execution lambdas for run.
        void renderScene(PassExecution &passExec);

        MaterialPipeline opaquePipeline;
        MaterialPipeline transparentPipeline;

        VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
        VkDescriptorSetLayout lightDescriptorSetLayout;
        DrawContext &drawContext;
        GPUSceneData &sceneData;
    };
} // namespace rgraph

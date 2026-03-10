#pragma once
#include "IFeature.h"
#include "MaterialSystem.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "vk_engine.h"
#include "vk_types.h"
#include <memory>

namespace rgraph
{

    /**
     * @brief An implementation of IFeature that does simple PBR Shading
     *
     */
    class PBRShadingFeature : public IFeature
    {
      public:
        PBRShadingFeature(DrawContext &drwCtx, VkDevice _device,
                          GLTFMRMaterialSystemCreateInfo &materialSystemCreateInfo, GPUSceneData &sceneData,
                          VkDescriptorSetLayout gpuSceneLayout, DeletionQueue &delQueue);

        void Register(Rendergraph *builder) override;

        std::shared_ptr<GLTFMRMaterialSystem> getMaterialSystemReference();

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

        void createPipelines(GLTFMRMaterialSystemCreateInfo &materialSystemCreateInfo);
        // execution lambdas for run.
        void renderScene(PassExecution &passExec);

        std::shared_ptr<GLTFMRMaterialSystem> materialSystem;

        MaterialPipeline opaquePipeline;
        MaterialPipeline transparentPipeline;

        VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
        VkDescriptorSetLayout lightDescriptorSetLayout;
        DrawContext &drawContext;
        GPUSceneData &sceneData;
    };
} // namespace rgraph

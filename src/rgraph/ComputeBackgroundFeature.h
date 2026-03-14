#pragma once
#include "IFeature.h"
#include "vk_engine.h"
#include "vk_types.h"

namespace rgraph
{

    /**
     * @brief An implementation of IFeature that uses compute shaders to draw a background.
     *
     */
    class ComputeBackgroundFeature : public IFeature
    {
      public:
        ComputeBackgroundFeature(VkDevice _device, DeletionQueue &delQueue, VkExtent3D imageExtent,
                                 AllocatedImage drawImage);

        void Register(Rendergraph *builder) override;

      private:
        void InitPipeline(VkDevice _device, DeletionQueue &delQueue);
        void DrawBackground(PassExecution &passExec);

        // variables required for the compute pass.
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorLayout;
        VkDescriptorSet descriptorSet;
        ComputePushConstants data;
        DescriptorAllocatorGrowable descriptorAllocator;
    };
} // namespace rgraph
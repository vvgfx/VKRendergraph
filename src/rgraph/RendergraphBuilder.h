#pragma once
#include "vk_engine.h"
#include "vk_types.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
namespace rgraph
{

    class IFeature;

    enum PassType
    {
        Compute,
        Graphics
    };

    struct PassImageRead
    {
        std::string name;
        VkImageLayout startingLayout;
    };

    struct PassImageWrite
    {
        std::string name;

        // These are used for color and depth attachments
        bool store;
        VkClearValue *clear;
    };

    struct PassBufferCreationInfo
    {
        std::string name;
        size_t size;
        VkBufferUsageFlags usageFlags;
    };

    struct Pass
    {
        friend class RendergraphBuilder;

        // these are for compute.
        void ReadsImage(const std::string name, VkImageLayout layout);
        void WritesImage(const std::string name);

        // Add these back as I require.
        // these are for graphics.
        void AddColorAttachment(const std::string name, bool store, VkClearValue *clear = nullptr);
        void AddDepthStencilAttachment(const std::string name, bool store, VkClearValue *clear = nullptr);

        // need a resolve target for MSAA
        void AddResolveTarget(
            const std::string resolveColorImageName, std::string resolveDepthImageName,
            VkResolveModeFlagBits colorResolutionMode = VK_RESOLVE_MODE_AVERAGE_BIT,
            VkResolveModeFlagBits depthResolutionMode = VK_RESOLVE_MODE_MAX_BIT); // using reverse-z, so MAX, not MIN

        void CreatesBuffer(const std::string name, size_t size, VkBufferUsageFlags usages);

        void ReadsBuffer(const std::string name);
        void WritesBuffer(const std::string name);

        PassType type;
        std::string name;

      private:
        // add imageRead vector
        std::vector<PassImageRead> imageReads;
        // add imageWrite vector
        std::vector<PassImageWrite> imageWrites;

        // add imageWrite vector for colorAttachments
        std::vector<PassImageWrite> colorAttachments;

        // add PassBufferCreationInfo vector for buffer creations
        std::vector<PassBufferCreationInfo> bufferCreations;
        // add string vector for buffer dependencies

        // add depth attachment read, bool storeDepth, and a reference to the creating builder itself.
        PassImageWrite depthAttachment{};
        bool storeDepth;
        bool bresolveColor = false;
        bool bresolveDepth = false;
        std::string resolveColorImageName;
        std::string resolveDepthImageName;
        VkResolveModeFlagBits colorResolutionMode;
        VkResolveModeFlagBits depthResolutionMode;
    };

    struct PassExecution
    {
        VkCommandBuffer cmd;
        VkDevice _device;
        std::unordered_map<std::string, AllocatedBuffer> allocatedBuffers;
        std::unordered_map<std::string, AllocatedImage> allocatedImages;

        // temporary, need to change later.
        VkExtent3D _drawExtent;

        // stuff required for per-frame data.
        DeletionQueue *delQueue;
        DescriptorAllocatorGrowable *frameDescriptor;

        // performance variables.
        float dispatchCalls; // compute
        float drawCalls;     // graphics
        float triangles;     // graphics
    };

    struct TransitionData
    {
        std::string imageName;
        VkImageLayout currentLayout, newLayout;
    };

    struct PassTiming
    {
        std::string name;
        float gpuMs = 0.0f;
        uint32_t barrierCount = 0;
    };

    /**
     * @brief This class builds the rendergraph, and is expected to be called every frame.
     *
     */
    class RendergraphBuilder
    {

      public:
        void AddComputePass(const std::string name, std::function<void(Pass &)> setup,
                            std::function<void(PassExecution &)> run);
        void AddGraphicsPass(const std::string name, std::function<void(Pass &)> setup,
                             std::function<void(PassExecution &)> run);

        void AddTrackedImage(const std::string name, VkImageLayout startLayout, AllocatedImage image);
        void AddTrackedBuffer(const std::string name, AllocatedBuffer buffer);

        void Build(FrameData &frameData);

        // the framedata is used for per-frame deletion queue and unique command buffers.
        void Run(FrameData &frameData);

        void AddFeature(std::weak_ptr<IFeature> feature);

        void setReqData(VkDevice _device, VkExtent3D _extent);

        // performance stuff.
        void SetTimestampPeriod(float period)
        {
            timestampPeriod = period;
        }
        const std::vector<PassTiming> &GetLastFrameTimings() const
        {
            return lastFrameTimings;
        }
        void ReadTimestamps(FrameData &frameData);

        float GetTotalGpuTime() const
        {
            return totalGpuMs;
        }

      private:
        std::vector<Pass> passData;

        std::vector<std::function<void(PassExecution &)>> executionLambdas;

        std::unordered_map<std::string, AllocatedImage> images;
        std::unordered_map<std::string, AllocatedBuffer> buffers;

        std::vector<std::weak_ptr<IFeature>> features;

        std::unordered_map<std::string, std::vector<TransitionData>> transitionData;
        VkDevice _device;
        VkExtent3D _extent;

        // performance stuff.
        std::vector<PassTiming> lastFrameTimings;
        std::vector<uint64_t> timestampBuffer;
        float timestampPeriod = 1.0f;
        float totalGpuMs = 0.0f;
    };
} // namespace rgraph
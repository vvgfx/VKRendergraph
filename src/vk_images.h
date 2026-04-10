
#pragma once

#include <cassert>
#include <vector>

#define MAX_IMAGE_BARRIERS 10

namespace vkutil
{
    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

    void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);

    // singleton!
    struct BarrierMerger
    {
        void transition_image(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

        void flushBarriers(VkCommandBuffer cmd);

        static BarrierMerger &GetInstance();
        BarrierMerger();

      private:
        std::vector<VkImageMemoryBarrier2> imgBarriers;
        static BarrierMerger *instance;
    };
}; // namespace vkutil
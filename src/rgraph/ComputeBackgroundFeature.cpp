#include "ComputeBackgroundFeature.h"
#include "vk_engine.h"
#include "vk_pipelines.h"
#include "vk_types.h"

rgraph::ComputeBackgroundFeature::ComputeBackgroundFeature(VkDevice _device, DeletionQueue &delQueue,
                                                           VkExtent3D imageExtent, AllocatedImage drawImage)
{
    // CREATE REQUIRED IMAGE

    // Since I'm using a single draw image for all my passes, I'm not creating the draw image here, it will remain on
    // vk_engine drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT; drawImage.imageExtent = imageExtent;

    // VkImageUsageFlags drawImageUsages{};
    // drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    // drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    // drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, imageExtent);

    // VmaAllocationCreateInfo rimg_allocinfo = {};
    // rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    // rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // gpuResourceAllocator->create_image(&rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation,
    // nullptr);

    // VkImageViewCreateInfo rview_info =
    //     vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    // VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &drawImage.imageView));

    // CREATE DESCRIPTOR ALLOCATOR
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
    descriptorAllocator.init(_device, 10, sizes);
    // CREATE DESCRIPTOR SET LAYOUT
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        descriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // CREATE IMAGE DESCRIPTOR SET
    descriptorSet = descriptorAllocator.allocate(_device, descriptorLayout);

    DescriptorWriter writer;
    writer.write_image(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.update_set(_device, descriptorSet);

    // create the pipeline.
    InitPipeline(_device, delQueue);

    // add to deletion queue.
    delQueue.push_function(
        [_device, this]()
        {
            // vkDestroyImageView(_device, drawImage.imageView, nullptr);
            // gpuResourceAllocator->destroy_image(drawImage.image, drawImage.allocation);

            descriptorAllocator.destroy_pools(_device);
            vkDestroyDescriptorSetLayout(_device, descriptorLayout, nullptr);
        });
}

void rgraph::ComputeBackgroundFeature::InitPipeline(VkDevice _device, DeletionQueue &delQueue)
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &descriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &pipelineLayout));

    VkShaderModule skyShader;
    if (!vkutil::load_shader_module("../shaders/sky.comp.spv", _device, &skyShader))
        fmt::print("Error when building the compute shader \n");

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = skyShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    // for compatibility.
    data = {};
    // default sky parameters
    data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    vkDestroyShaderModule(_device, skyShader, nullptr);
    delQueue.push_function(
        [=, this]()
        {
            vkDestroyPipelineLayout(_device, pipelineLayout, nullptr);
            vkDestroyPipeline(_device, pipeline, nullptr);
        });
}

void rgraph::ComputeBackgroundFeature::Register(rgraph::Rendergraph *builder)
{
    builder->AddComputePass(
        "background-pass",
        [](rgraph::Pass &pass)
        {
            // this should write to the draw image
            // it does not take any other input besides the compute push constants.
            pass.WritesImage("drawImage");
        },
        [&](rgraph::PassExecution &passExec) { DrawBackground(passExec); });
}

void rgraph::ComputeBackgroundFeature::DrawBackground(rgraph::PassExecution &passExec)
{
    vkCmdBindPipeline(passExec.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // DescriptorWriter writer;

    // writer.write_image(0, passExec.allocatedImages["drawImage"].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
    //                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    // writer.update_set(passExec._device, descriptorSet);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(passExec.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0,
                            nullptr);

    vkCmdPushConstants(passExec.cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants),
                       &data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(passExec.cmd, std::ceil(passExec._drawExtent.width / 16.0),
                  std::ceil(passExec._drawExtent.height / 16.0), 1);

    passExec.dispatchCalls =
        std::ceil(passExec._drawExtent.width / 16.0) * std::ceil(passExec._drawExtent.height / 16.0) * 1;
}
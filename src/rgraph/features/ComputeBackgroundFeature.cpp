#include "ComputeBackgroundFeature.h"
#include "vk_engine.h"
#include "vk_pipelines.h"
#include "vk_types.h"

rgraph::ComputeBackgroundFeature::ComputeBackgroundFeature(VkDevice _device, DeletionQueue &delQueue, VkExtent3D imageExtent,
                                                           AllocatedImage drawImage)
{

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
    writer.write_image(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.update_set(_device, descriptorSet);

    // create the pipeline.
    InitPipeline(_device, delQueue);

    // add to deletion queue.
    delQueue.push_function(
        [_device, this]()
        {
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
    {
        fmt::print("Error when building the compute shader \n");
    }

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

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(passExec.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdPushConstants(passExec.cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(passExec.cmd, std::ceil(passExec._drawExtent.width / 16.0), std::ceil(passExec._drawExtent.height / 16.0), 1);

    passExec.dispatchCalls = std::ceil(passExec._drawExtent.width / 16.0) * std::ceil(passExec._drawExtent.height / 16.0) * 1;
}
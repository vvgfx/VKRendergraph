#include "rgraph/features/DeferredRenderingFeature.h"
#include "DeferredRenderingFeature.h"
#include "GPUResourceAllocator.h"
#include "rgraph/Rendergraph.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include <vulkan/vulkan_core.h>

rgraph::DeferredRenderingFeature::DeferredRenderingFeature(DrawContext &drawContext, VkDevice _device, GPUSceneData &gpuSceneData,
                                                           VkDescriptorSetLayout gpuSceneLayout, MaterialSystemCreateInfo &materialSystemCreateInfo,
                                                           DeletionQueue &delQueue)
    : drawContext(drawContext), gpuSceneData(gpuSceneData)
{
    _gpuSceneDataDescriptorLayout = gpuSceneLayout;

    // descriptor set for lights.
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        lightDescriptorSetLayout = layoutBuilder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // descriptor set for composite pass inputs.
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // position
        layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // normal
        layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // albedo
        layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // metallic-roughness
        compDescriptorSetLayout = layoutBuilder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // create images here and add them to be tracked.
    createImages(delQueue);

    createPipelines(materialSystemCreateInfo);

    delQueue.push_function(
        [_device, this]()
        {
            vkDestroyDescriptorSetLayout(_device, lightDescriptorSetLayout, nullptr);
            vkDestroyDescriptorSetLayout(_device, compDescriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(_device, geometryPipeline.layout, nullptr);
            vkDestroyPipelineLayout(_device, compositePipeline.layout, nullptr);
            vkDestroyPipeline(_device, geometryPipeline.pipeline, nullptr);
            vkDestroyPipeline(_device, compositePipeline.pipeline, nullptr);
        });
}

void rgraph::DeferredRenderingFeature::Register(rgraph::Rendergraph *builder)
{

    builder->AddGraphicsPass(
        "Geometry Pass",
        [](Pass &pass)
        {
            static VkClearValue colorClearValue{};
            colorClearValue.color = {0.0f, 0.0f, 0.0f, 0.0f};
            colorClearValue.depthStencil = {0.0f};
            pass.AddColorAttachment("position_gbuf", true, &colorClearValue);
            pass.AddColorAttachment("normal_gbuf", true, &colorClearValue);
            pass.AddColorAttachment("albedo_gbuf", true, &colorClearValue);
            pass.AddColorAttachment("metalrough_gbuf", true, &colorClearValue);
            pass.AddDepthStencilAttachment("depth_gbuf", false, &colorClearValue);
            pass.CreatesBuffer("gpuSceneBuffer", sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        },
        [&](PassExecution &passExec) { geometryPass(passExec); });

    builder->AddGraphicsPass(
        "Composite Pass",
        [](Pass &pass)
        {
            static VkClearValue colorClearValue{};
            colorClearValue.color = {0.0f, 0.0f, 0.0f, 0.0f};
            colorClearValue.depthStencil = {0.0f};
            pass.ReadsImage("position_gbuf", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            pass.ReadsImage("normal_gbuf", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            pass.ReadsImage("albedo_gbuf", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            pass.ReadsImage("metalrough_gbuf", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            pass.AddColorAttachment("drawImage", false, &colorClearValue);
            pass.AddDepthStencilAttachment("depth_gbuf", true, &colorClearValue);
            pass.CreatesBuffer("lightBuffer", sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            pass.CreatesBuffer("gpuSceneBuffer", sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        },
        [&](PassExecution &passExec) { compositePass(passExec); });
}

void rgraph::DeferredRenderingFeature::geometryPass(rgraph::PassExecution &passExec)
{
}

void rgraph::DeferredRenderingFeature::compositePass(rgraph::PassExecution &passExec)
{
}

void rgraph::DeferredRenderingFeature::createPipelines(MaterialSystemCreateInfo &materialSystemCreateInfo)
{

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../shaders/deferred/mrt.frag.spv", materialSystemCreateInfo._device, &meshFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module\n");
    }

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("../shaders/deferred/mrt.vert.spv", materialSystemCreateInfo._device, &meshVertexShader))
    {
        fmt::println("Error when building the triangle vertex shader module\n");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout materialLayout = VulkanEngine::Instance().GetMaterialSystem().materialLayout;

    // create pipeline for mrt pass ---------------------------------
    VkDescriptorSetLayout mrtLayouts[] = {materialSystemCreateInfo._gpuSceneDataDescriptorLayout, materialLayout};

    VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::pipeline_layout_create_info();
    meshLayoutInfo.setLayoutCount = 2;
    meshLayoutInfo.pSetLayouts = mrtLayouts;
    meshLayoutInfo.pPushConstantRanges = &matrixRange;
    meshLayoutInfo.pushConstantRangeCount = 1;

    VkPipelineLayout geomPipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(materialSystemCreateInfo._device, &meshLayoutInfo, nullptr, &geomPipelineLayout));

    std::vector<VkFormat> colorAttachmentFormats = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                    VK_FORMAT_R16G16B16A16_SFLOAT};

    geometryPipeline.layout = geomPipelineLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.set_multiple_color_attachments(colorAttachmentFormats);
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.set_depth_format(materialSystemCreateInfo.depthFormat);

    pipelineBuilder._pipelineLayout = geomPipelineLayout;

    geometryPipeline.pipeline = pipelineBuilder.build_pipeline(materialSystemCreateInfo._device);

    vkDestroyShaderModule(materialSystemCreateInfo._device, meshFragShader, nullptr);
    vkDestroyShaderModule(materialSystemCreateInfo._device, meshVertexShader, nullptr);

    // now create pipeline for comp pass -----------------------------

    if (!vkutil::load_shader_module("../shaders/deferred/comp.vert.spv", materialSystemCreateInfo._device, &meshVertexShader))
    {
        fmt::println("Error when building the triangle fragment shader module\n");
    }

    if (!vkutil::load_shader_module("../shaders/deferred/comp.frag.spv", materialSystemCreateInfo._device, &meshFragShader))
    {
        fmt::println("Error when building the triangle fragment shader module\n");
    }

    VkDescriptorSetLayout compLayouts[] = {compDescriptorSetLayout, materialSystemCreateInfo._gpuSceneDataDescriptorLayout, lightDescriptorSetLayout};

    meshLayoutInfo.setLayoutCount = 3;
    meshLayoutInfo.pSetLayouts = compLayouts;
    meshLayoutInfo.pPushConstantRanges = nullptr;
    meshLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout compPipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(materialSystemCreateInfo._device, &meshLayoutInfo, nullptr, &compPipelineLayout));

    compositePipeline.layout = compPipelineLayout;

    pipelineBuilder.clear();

    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.set_color_attachment_format(materialSystemCreateInfo.colorFormat);
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_depth_format(materialSystemCreateInfo.depthFormat);
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.disable_depthtest();

    pipelineBuilder._pipelineLayout = compPipelineLayout;

    compositePipeline.pipeline = pipelineBuilder.build_pipeline(materialSystemCreateInfo._device);

    vkDestroyShaderModule(materialSystemCreateInfo._device, meshVertexShader, nullptr);
    vkDestroyShaderModule(materialSystemCreateInfo._device, meshFragShader, nullptr);
}

void rgraph::DeferredRenderingFeature::createImages(DeletionQueue &delQueue)
{
    auto windowExtent = VulkanEngine::Instance().GetWindowExtent();
    auto device = VulkanEngine::Instance().GetVkDevice();
    VkExtent3D imageExtent = {windowExtent.width, windowExtent.height, 1};

    std::vector<AllocatedImage *> gbufImages = {&position_gbuf, &normal_gbuf, &albedo_gbuf, &metalrough_gbuf};
    std::vector<std::string> gbufNames = {"position_gbuf", "normal_gbuf", "albedo_gbuf", "metalrough_gbuf"};

    Rendergraph &rgraphInstance = Rendergraph::Instance();

    // create the color attachments
    for (int i = 0; i < gbufImages.size(); i++)
    {
        auto &gbufImage = gbufImages[i];
        gbufImage->imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        gbufImage->imageExtent = imageExtent;

        VkImageUsageFlags colorImageUses{};
        colorImageUses |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        colorImageUses |= VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageCreateInfo rimg_info = vkinit::image_create_info(gbufImage->imageFormat, colorImageUses, imageExtent);

        VmaAllocationCreateInfo rimg_allocinfo = {};
        rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        GPUResourceAllocator &_gpuResourceAllocator = GPUResourceAllocator::Instance();
        _gpuResourceAllocator.create_image(&rimg_info, &rimg_allocinfo, &gbufImage->image, &gbufImage->allocation, nullptr);

        VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(gbufImage->imageFormat, gbufImage->image, VK_IMAGE_ASPECT_COLOR_BIT);

        VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &gbufImage->imageView));

        rgraphInstance.AddTrackedImage(gbufNames[i], VK_IMAGE_LAYOUT_UNDEFINED, *gbufImage);

        delQueue.push_function(
            [=, this]
            {
                auto &_gpuResourceAllocator = GPUResourceAllocator::Instance();
                vkDestroyImageView(device, gbufImage->imageView, nullptr);
                _gpuResourceAllocator.destroy_image(gbufImage->image, gbufImage->allocation);
            });
    }

    // create the depth-stencil attachment

    depth_gbuf.imageFormat = VK_FORMAT_D32_SFLOAT;
    depth_gbuf.imageExtent = imageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(depth_gbuf.imageFormat, depthImageUsages, imageExtent);

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    GPUResourceAllocator::Instance().create_image(&dimg_info, &rimg_allocinfo, &depth_gbuf.image, &depth_gbuf.allocation, nullptr);

    // build a image-view for the depth image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(depth_gbuf.imageFormat, depth_gbuf.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &depth_gbuf.imageView));

    rgraphInstance.AddTrackedImage("depth_gbuf", VK_IMAGE_LAYOUT_UNDEFINED, depth_gbuf);

    delQueue.push_function(
        [=, this]
        {
            auto &_gpuResourceAllocator = GPUResourceAllocator::Instance();
            vkDestroyImageView(device, depth_gbuf.imageView, nullptr);
            _gpuResourceAllocator.destroy_image(depth_gbuf.image, depth_gbuf.allocation);
        });
}
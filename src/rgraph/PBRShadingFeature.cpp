#include "PBRShadingFeature.h"
#include "MaterialSystem.h"
#include "RendergraphBuilder.h"
#include "fmt/base.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include <algorithm>
#include <memory>

// multisampling config ---------------------------

VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

// multisampling config ---------------------------

// forward declaration for now, TODO: come back and move this function here. remove from VKEngine.
bool is_visible(const RenderObject &obj, const glm::mat4 &viewproj);

rgraph::PBRShadingFeature::PBRShadingFeature(DrawContext &drwCtx, VkDevice _device,
                                             GLTFMRMaterialSystemCreateInfo &materialSystemCreateInfo,
                                             GPUSceneData &scnData, VkDescriptorSetLayout gpuSceneLayout,
                                             DeletionQueue &delQueue)
    : drawContext(drwCtx), sceneData(scnData)
{
    _gpuSceneDataDescriptorLayout = gpuSceneLayout;
    materialSystem = std::make_shared<GLTFMRMaterialSystem>();
    materialSystem->build_descriptors(_device);

    // create descriptor set for lights.
    {
        DescriptorLayoutBuilder layoutBuilder;
        layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        lightDescriptorSetLayout =
            layoutBuilder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    createPipelines(materialSystemCreateInfo);
    delQueue.push_function(
        [_device, this]()
        {
            materialSystem->clear_resources(_device);
            vkDestroyDescriptorSetLayout(_device, lightDescriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(_device, transparentPipeline.layout, nullptr);
            vkDestroyPipeline(_device, transparentPipeline.pipeline, nullptr);
            vkDestroyPipeline(_device, opaquePipeline.pipeline, nullptr);
        });
}

void rgraph::PBRShadingFeature::Register(rgraph::RendergraphBuilder *builder)
{
    builder->AddGraphicsPass(
        "renderPass",
        [](Pass &pass)
        {
            static VkClearValue colorClearValue{};
            colorClearValue.color = {0.0f, 0.0f, 0.0f, 0.0f};
            colorClearValue.depthStencil = {0.0f};
            pass.AddColorAttachment("msaaColor", false, &colorClearValue);
            pass.AddDepthStencilAttachment("msaaDepth", false, &colorClearValue);
            pass.AddResolveTarget("drawImage", "");
            pass.CreatesBuffer("gpuSceneBuffer", sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            pass.CreatesBuffer("lightBuffer", sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        },
        [&](PassExecution &passExec) { renderScene(passExec); });
}

std::shared_ptr<GLTFMRMaterialSystem> rgraph::PBRShadingFeature::getMaterialSystemReference()
{
    return materialSystem;
}

void rgraph::PBRShadingFeature::renderScene(rgraph::PassExecution &passExec)
{

    // reset counters
    passExec.drawCalls = 0;
    passExec.triangles = 0;

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(drawContext.OpaqueSurfaces.size());

    for (int i = 0; i < drawContext.OpaqueSurfaces.size(); i++)
        if (is_visible(drawContext.OpaqueSurfaces[i], sceneData.viewproj))
            opaque_draws.push_back(i);

    // sort the opaque surfaces by material and mesh
    std::sort(opaque_draws.begin(), opaque_draws.end(),
              [&](const auto &iA, const auto &iB)
              {
                  const RenderObject &A = drawContext.OpaqueSurfaces[iA];
                  const RenderObject &B = drawContext.OpaqueSurfaces[iB];
                  if (A.material == B.material)
                      return A.indexBuffer < B.indexBuffer;
                  else
                      return A.material < B.material;
              });

    AllocatedBuffer gpuSceneDataBuffer = passExec.allocatedBuffers["gpuSceneBuffer"];

    // similarly, set the data for the lights.
    AllocatedBuffer lightAllocBuffer = passExec.allocatedBuffers["lightBuffer"];

    LightData *lightdata = (LightData *)lightAllocBuffer.info.pMappedData;
    lightdata->numLights = drawContext.lights.size();
    for (int i = 0; i < lightdata->numLights; i++)
    {
        PointLight pl = {};
        pl.color = drawContext.lights[i].color;
        pl.transform = drawContext.lights[i].transform;
        pl.intensity = drawContext.lights[i].intensity;
        lightdata->pointLights[i] = pl;
    }

    VkDescriptorSet lightDescriptor = passExec.frameDescriptor->allocate(passExec._device, lightDescriptorSetLayout);

    DescriptorWriter lightWriter;
    lightWriter.write_buffer(0, lightAllocBuffer.buffer, sizeof(LightData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    lightWriter.update_set(passExec._device, lightDescriptor);

    // write the buffer
    GPUSceneData *sceneUniformData = (GPUSceneData *)gpuSceneDataBuffer.info.pMappedData;
    *sceneUniformData = sceneData;

    // // create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = passExec.frameDescriptor->allocate(
        passExec._device, _gpuSceneDataDescriptorLayout); // temporarily getting the layout through the constructor.
                                                          // Will need to figure out a better way later.

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(passExec._device, globalDescriptor);

    // defined outside of the draw function, this is the state we will try to skip
    MaterialPass lastPass = MaterialPass::Other;
    MaterialPipeline *lastPipeline = nullptr;
    MaterialInstance *lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject &r)
    {
        if (r.material != lastMaterial)
        {
            lastMaterial = r.material;
            // rebind pipeline and descriptors if the material changed
            if (r.material->passType != lastPass)
            {
                lastPass = r.material->passType;
                lastPipeline = lastPass == MaterialPass::Transparent
                                   ? &transparentPipeline
                                   : &opaquePipeline; // change to use passtype instead of pipeline.

                VkDescriptorSet ds[] = {globalDescriptor, lightDescriptor};

                vkCmdBindPipeline(passExec.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline->pipeline);
                vkCmdBindDescriptorSets(passExec.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline->layout, 0, 2, ds,
                                        0, nullptr);

                VkViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)passExec._drawExtent.width;
                viewport.height = (float)passExec._drawExtent.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;

                vkCmdSetViewport(passExec.cmd, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = passExec._drawExtent.width;
                scissor.extent.height = passExec._drawExtent.height;

                vkCmdSetScissor(passExec.cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(passExec.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline->layout, 2, 1,
                                    &r.material->materialSet, 0, nullptr);
        }
        // rebind index buffer if needed
        if (r.indexBuffer != lastIndexBuffer)
        {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(passExec.cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        // calculate final mesh matrix
        GPUDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.vertexBuffer = r.vertexBufferAddress;

        vkCmdPushConstants(passExec.cmd, lastPipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(passExec.cmd, r.indexCount, 1, r.firstIndex, 0, 0);
        // stats
        passExec.drawCalls++;
        passExec.triangles += r.indexCount / 3;
    };

    for (auto &r : opaque_draws)
        draw(drawContext.OpaqueSurfaces[r]);

    for (auto &r : drawContext.TransparentSurfaces)
        draw(r);
}

void rgraph::PBRShadingFeature::createPipelines(GLTFMRMaterialSystemCreateInfo &info)
{
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../shaders/light_mesh.frag.spv", info._device, &meshFragShader))
        fmt::println("Error when building the triangle fragment shader module\n");

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("../shaders/light_mesh.vert.spv", info._device, &meshVertexShader))
        fmt::println("Error when building the triangle vertex shader module\n");

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    auto materialLayout = materialSystem->materialLayout;

    VkDescriptorSetLayout layouts[] = {info._gpuSceneDataDescriptorLayout, lightDescriptorSetLayout, materialLayout};

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 3;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(info._device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_custom(VK_SAMPLE_COUNT_4_BIT);
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // render format
    pipelineBuilder.set_color_attachment_format(info.colorFormat);
    pipelineBuilder.set_depth_format(info.depthFormat);

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = newLayout;

    // finally build the pipeline
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(info._device);

    // create the transparent variant
    pipelineBuilder.enable_blending_additive();

    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(info._device);

    vkDestroyShaderModule(info._device, meshFragShader, nullptr);
    vkDestroyShaderModule(info._device, meshVertexShader, nullptr);
}
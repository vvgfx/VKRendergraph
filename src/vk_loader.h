#pragma once

#include "GPUResourceAllocator.h"
#include "sgraph/ScenegraphStructs.h"
#include "vk_descriptors.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vk_types.h>

// importing PBEngine gives me circular dependency issues,so forward declaring this for now.
struct GLTFMRMaterialSystem;

struct GLTFMaterial
{
    MaterialInstance data;
};

struct Bounds
{
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

// contains details requried for the loaders.
struct GLTFCreatorData
{
    VkDevice _device;
    AllocatedImage loadErrorImage;
    AllocatedImage defaultImage;
    VkSampler _defaultSamplerLinear;
    GLTFMRMaterialSystem *materialSystemReference;
};

// lighting data
struct LightingData
{
    glm::vec3 color;
    float intensity;
    enum LightType
    {
        Directional,
        Spot,
        Point
    };

    LightType type;
    float range;
    float innerConeAngle;
    float outerConeAngle;
    std::string name;
};

// forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine *engine,
                                                                      std::filesystem::path filePath);
namespace sgraph
{

    struct GLTFScene : public INode
    {
        // storage for all the data on a given glTF file
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
        std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
        std::unordered_map<std::string, AllocatedImage> images;
        std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
        std::unordered_map<std::string, std::shared_ptr<LightingData>> lightingData;

        // nodes that dont have a parent, for iterating through the file in tree order
        std::vector<std::shared_ptr<Node>> topNodes;

        std::vector<VkSampler> samplers;

        DescriptorAllocatorGrowable descriptorPool;

        AllocatedBuffer materialDataBuffer;

        GLTFCreatorData creator;

        ~GLTFScene()
        {
            clearAll();
        };

        virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx);

        std::string name;

      private:
        void clearAll();
    };

} // namespace sgraph

std::optional<std::shared_ptr<sgraph::GLTFScene>> loadGltf(GLTFCreatorData creatorData, std::string_view filePath);
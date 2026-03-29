## This repo contains a Vulkan rendering engine built around a rendergraph architecture.

This serves as a sandbox for experimenting with rendering algorithms and architecture, while also doubling as a portfolio piece. It is actively being worked on, and both features and architecture are still evolving.

### Screenshots:

![image](screenshots/vk_engine.png)

### Features:

It has the following features:

- Rendergraph - Manages pass ordering, image layout transitions, and barrier insertion. Features declare their resource requirements and the rendergraph handles all synchronisation. This means features never issue their own barriers.
- Feature-based architecture - Rendering techniques are self-contained modules that implement an `IFeature` interface. Each feature registers its passes with the rendergraph builder using a setup/execute lambda pattern.
- PBR shading - Metallic-roughness workflow with baseColor and metallicRoughness texture loading from GLTF files. Uses sRGB-correct linear workflow with ACES filmic tonemapping.
- MSAA - 4x multisample anti-aliasing with rendergraph-managed resolve targets. MSAA color and depth are separate tracked images that resolve into the draw image.
- GLTF scene loading - Mesh and light node support via fastgltf. Lights are passed to shaders through storage buffers.
- GPU profiling - Per-pass timestamp queries measuring GPU time, with results read back from a previous frame to avoid CPU-GPU stalls. Displayed through an ImGui overlay.
- Bindless vertex fetching - Mesh buffer addresses are passed to shaders via buffer device addresses (BDA) through push constants, bypassing the traditional fixed-function vertex input pipeline.
- Dynamic rendering - Uses Vulkan 1.3's dynamic rendering. No render pass objects or framebuffers.

### To-do list:

- Deferred Rendering - This is WIP
- Clustered shading - This is WIP, dependent on Deferred rendering
- SMAA - This is WIP
- Rendergraph dependency tracking with automatic pass ordering and culling
- Transient image management (rendergraph-owned, auto-resized per frame)
- Async compute for overlapping independent passes

(This is not exhaustive)

### Build and run:

SPIR-V shader compilation is integrated into the CMake build. No additional shader tooling setup is required.

```sh
cmake -B build -G Ninja
cmake --build build
```

### Requirements:

- Vulkan 1.3 (dynamic rendering, synchronization2)
- C++ compiler
- CMake with Ninja generator

All other dependencies are vendored into the repository:

- [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (Vulkan Memory Allocator)
- [SDL](https://github.com/libsdl-org/SDL)
- [GLM](https://github.com/g-truc/glm)
- [fastgltf](https://github.com/spnda/fastgltf)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [VkBootstrap](https://github.com/charles-lunarg/vk-bootstrap)

(This is not exhaustive)

### References:

- https://vkguide.dev/ - Foundational reference for the engine's Vulkan architecture.
- https://logins.github.io/graphics/2021/05/31/RenderGraphs.html - Rendergraphs.
- https://www.iryoku.com/smaa/ - SMAA Reference.

And more to be added eventually!

<!-- ### Acknowledgements:

Thank you to the graphics community for the resources, references, and motivation to keep building. -->

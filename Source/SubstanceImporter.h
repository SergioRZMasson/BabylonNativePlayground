#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "imgui.h"

#ifdef HAS_SUBSTANCE_SDK
#include <substance/framework/framework.h>
#include <Babylon/Plugins/ExternalTexture.h>
#include <Babylon/Graphics/Device.h>
#endif

// Describes an external texture output bound to a substance graph output
struct SubstanceExternalOutput
{
    std::string identifier;
    std::string channelType;
#ifdef HAS_SUBSTANCE_SDK
    std::optional<Babylon::Plugins::ExternalTexture> externalTexture;
#endif
    uint32_t width = 0;
    uint32_t height = 0;
};

// Callback for applying substance external textures to a BabylonJS material.
// Receives the material uniqueId and a reference to the external outputs.
using SubstanceApplyCallback = std::function<void(uint32_t materialUid,
    const std::vector<SubstanceExternalOutput>& outputs)>;

class SubstanceImporter
{
public:
    SubstanceImporter();
    ~SubstanceImporter();

    // Set the graphics device (needed for creating native textures)
    void SetGraphicsDevice(Babylon::Graphics::Device* device);

    // Set callback for applying textures to a BabylonJS material
    void SetApplyCallback(SubstanceApplyCallback callback);

    // Load an .sbsar file from disk. Returns true on success.
    bool LoadSbsarFile(const std::string& filePath);

    // Render the substance tab content (call inside an existing ImGui window/tab)
    void RenderTab();

    // Apply the latest substance textures to a material by UID
    void ApplyToMaterial(uint32_t materialUid);

    // Returns true if there is at least one loaded material with rendered outputs
    bool HasRenderedOutputs() const;

private:
#ifdef HAS_SUBSTANCE_SDK
    struct LoadedMaterial
    {
        std::string filePath;
        std::string displayName;
        std::vector<unsigned char> archiveData;
        std::unique_ptr<SubstanceAir::PackageDesc> package;
        SubstanceAir::GraphInstances graphInstances;
        std::vector<SubstanceExternalOutput> externalOutputs;
    };

    void RenderMaterialInputs(LoadedMaterial& mat, SubstanceAir::GraphInstance& graph);
    void RenderMaterialOutputs(SubstanceAir::GraphInstance& graph);
    void RenderGraph(LoadedMaterial& mat, size_t graphIdx);
    void CreateExternalTextures(LoadedMaterial& mat, SubstanceAir::GraphInstance& graph);
    void UpdateExternalTextures(LoadedMaterial& mat, SubstanceAir::GraphInstance& graph);

    std::vector<LoadedMaterial> m_materials;
    std::unique_ptr<SubstanceAir::Renderer> m_renderer;
    Babylon::Graphics::Device* m_graphicsDevice = nullptr;
    int m_selectedMaterial = -1;
    int m_selectedGraph = 0;
#endif

    SubstanceApplyCallback m_applyCallback;
};

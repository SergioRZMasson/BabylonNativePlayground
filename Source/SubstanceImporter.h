#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"

#ifdef HAS_SUBSTANCE_SDK
#include <substance/framework/framework.h>
#endif

// Describes a single rendered output texture to be sent to JS
struct SubstanceTextureOutput
{
    std::string identifier;  // Output identifier (e.g. "baseColor", "normal")
    std::string graphUrl;    // Parent graph URL
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t pixelFormat = 0;
    uint8_t channelsOrder = 0;
    const uint8_t* data = nullptr;
    size_t dataSize = 0;
};

// Callback type for sending rendered texture data to the application layer
using SubstanceTextureCallback = std::function<void(const SubstanceTextureOutput&)>;

class SubstanceImporter
{
public:
    SubstanceImporter();
    ~SubstanceImporter();

    // Set callback invoked whenever a texture output is rendered/updated
    void SetTextureCallback(SubstanceTextureCallback callback);

    // Load an .sbsar file from disk. Returns true on success.
    bool LoadSbsarFile(const std::string& filePath);

    // Render the substance tab content (call inside an existing ImGui window/tab)
    void RenderTab();

private:
#ifdef HAS_SUBSTANCE_SDK
    struct LoadedMaterial
    {
        std::string filePath;
        std::string displayName;
        std::vector<unsigned char> archiveData;
        std::unique_ptr<SubstanceAir::PackageDesc> package;
        SubstanceAir::GraphInstances graphInstances;
    };

    void RenderMaterialInputs(SubstanceAir::GraphInstance& graph);
    void RenderMaterialOutputs(SubstanceAir::GraphInstance& graph);
    void RenderGraph(LoadedMaterial& mat, size_t graphIdx);

    std::vector<LoadedMaterial> m_materials;
    std::unique_ptr<SubstanceAir::Renderer> m_renderer;
    int m_selectedMaterial = -1;
    int m_selectedGraph = 0;
#endif

    SubstanceTextureCallback m_textureCallback;
};

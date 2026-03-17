#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

namespace SceneInspector
{

static constexpr uint32_t MAGIC   = 0x42534E44; // "BSND"
static constexpr uint32_t VERSION = 1;

// ---------------------------------------------------------------------------
// Binary reader with bounds checking
// ---------------------------------------------------------------------------
class BinaryReader
{
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos   = 0;
    bool   m_error = false;

    void ensureAvailable(size_t n)
    {
        if (m_pos + n > m_size) m_error = true;
    }

public:
    BinaryReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size) {}

    bool valid() const { return !m_error; }

    uint8_t readU8()
    {
        ensureAvailable(1);
        if (m_error) return 0;
        return m_data[m_pos++];
    }

    uint16_t readU16()
    {
        ensureAvailable(2);
        if (m_error) return 0;
        uint16_t val;
        std::memcpy(&val, m_data + m_pos, 2);
        m_pos += 2;
        return val;
    }

    uint32_t readU32()
    {
        ensureAvailable(4);
        if (m_error) return 0;
        uint32_t val;
        std::memcpy(&val, m_data + m_pos, 4);
        m_pos += 4;
        return val;
    }

    int32_t readI32()
    {
        ensureAvailable(4);
        if (m_error) return 0;
        int32_t val;
        std::memcpy(&val, m_data + m_pos, 4);
        m_pos += 4;
        return val;
    }

    float readF32()
    {
        ensureAvailable(4);
        if (m_error) return 0.0f;
        float val;
        std::memcpy(&val, m_data + m_pos, 4);
        m_pos += 4;
        return val;
    }

    std::string readString()
    {
        uint16_t len = readU16();
        if (m_error) return "";
        ensureAvailable(len);
        if (m_error) return "";
        std::string s(reinterpret_cast<const char*>(m_data + m_pos), len);
        m_pos += len;
        return s;
    }

    void readVec3(float out[3])
    {
        out[0] = readF32();
        out[1] = readF32();
        out[2] = readF32();
    }

    void readVec4(float out[4])
    {
        out[0] = readF32();
        out[1] = readF32();
        out[2] = readF32();
        out[3] = readF32();
    }
};

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------
struct CameraInfo
{
    std::string name;
    uint8_t type; // 0=free, 1=arc, 2=follow, 3=other
    float position[3]{};
    float target[3]{};
    float fov{};
    float minZ{}, maxZ{};
    bool isActive{};
};

struct NodeInfo
{
    std::string name;
    float position[3]{};
    bool hasQuaternion{};
    float rotation[4]{}; // euler(xyz) or quaternion(xyzw)
    float scaling[3]{};
    bool isEnabled{};
    std::string className;
};

struct MeshInfo
{
    std::string name;
    uint32_t totalVertices{};
    uint32_t totalIndices{};
    bool hasPositions{}, hasNormals{}, hasTangents{};
    bool hasUVs{}, hasUV2s{}, hasColors{};
    bool isVisible{}, isEnabled{};
    float position[3]{};
    bool hasQuaternion{};
    float rotation[4]{};
    float scaling[3]{};
    bool hasBoundingBox{};
    float bbMin[3]{}, bbMax[3]{};
    int32_t materialIndex{-1};
    std::string className;
};

struct TextureSlot
{
    std::string slotName;
    std::string textureName;
};

struct MaterialInfo
{
    std::string name;
    std::string typeName;
    float alpha{1.0f};
    bool backFaceCulling{}, wireframe{};
    float diffuseColor[3]{};
    float specularColor[3]{};
    float emissiveColor[3]{};
    float ambientColor[3]{};
    std::vector<TextureSlot> textureSlots;
};

struct TextureInfo
{
    std::string name;
    std::string url;
    int32_t width{-1}, height{-1};
    bool hasAlpha{}, isCube{};
    std::string className;
};

struct LightInfo
{
    std::string name;
    uint8_t type{}; // 0=point, 1=directional, 2=spot, 3=hemispheric
    float position[3]{};
    float direction[3]{};
    float intensity{};
    float diffuse[3]{};
    float specular[3]{};
    bool isEnabled{};
    float range{};
};

struct SceneData
{
    bool valid = false;
    std::vector<CameraInfo>   cameras;
    std::vector<NodeInfo>     nodes;
    std::vector<MeshInfo>     meshes;
    std::vector<MaterialInfo> materials;
    std::vector<TextureInfo>  textures;
    std::vector<LightInfo>    lights;
};

// ---------------------------------------------------------------------------
// Parse binary buffer into SceneData
// ---------------------------------------------------------------------------
inline SceneData Parse(const uint8_t* data, size_t size)
{
    SceneData scene;
    if (!data || size < 32) return scene;

    BinaryReader r(data, size);

    uint32_t magic   = r.readU32();
    uint32_t version = r.readU32();
    if (!r.valid() || magic != MAGIC || version != VERSION) return scene;

    uint32_t numCameras   = r.readU32();
    uint32_t numNodes     = r.readU32();
    uint32_t numMeshes    = r.readU32();
    uint32_t numMaterials = r.readU32();
    uint32_t numTextures  = r.readU32();
    uint32_t numLights    = r.readU32();
    if (!r.valid()) return scene;

    // --- Cameras ---
    scene.cameras.reserve(numCameras);
    for (uint32_t i = 0; i < numCameras && r.valid(); i++)
    {
        CameraInfo cam{};
        cam.name = r.readString();
        cam.type = r.readU8();
        r.readVec3(cam.position);
        r.readVec3(cam.target);
        cam.fov  = r.readF32();
        cam.minZ = r.readF32();
        cam.maxZ = r.readF32();
        cam.isActive = r.readU8() != 0;
        scene.cameras.push_back(std::move(cam));
    }

    // --- Transform Nodes ---
    scene.nodes.reserve(numNodes);
    for (uint32_t i = 0; i < numNodes && r.valid(); i++)
    {
        NodeInfo node{};
        node.name = r.readString();
        r.readVec3(node.position);
        node.hasQuaternion = r.readU8() != 0;
        if (node.hasQuaternion)
            r.readVec4(node.rotation);
        else
        {
            node.rotation[0] = r.readF32();
            node.rotation[1] = r.readF32();
            node.rotation[2] = r.readF32();
            node.rotation[3] = 0.0f;
        }
        r.readVec3(node.scaling);
        node.isEnabled = r.readU8() != 0;
        node.className = r.readString();
        scene.nodes.push_back(std::move(node));
    }

    // --- Meshes ---
    scene.meshes.reserve(numMeshes);
    for (uint32_t i = 0; i < numMeshes && r.valid(); i++)
    {
        MeshInfo mesh{};
        mesh.name         = r.readString();
        mesh.totalVertices = r.readU32();
        mesh.totalIndices  = r.readU32();
        mesh.hasPositions  = r.readU8() != 0;
        mesh.hasNormals    = r.readU8() != 0;
        mesh.hasTangents   = r.readU8() != 0;
        mesh.hasUVs        = r.readU8() != 0;
        mesh.hasUV2s       = r.readU8() != 0;
        mesh.hasColors     = r.readU8() != 0;
        mesh.isVisible     = r.readU8() != 0;
        mesh.isEnabled     = r.readU8() != 0;
        r.readVec3(mesh.position);
        mesh.hasQuaternion = r.readU8() != 0;
        if (mesh.hasQuaternion)
            r.readVec4(mesh.rotation);
        else
        {
            mesh.rotation[0] = r.readF32();
            mesh.rotation[1] = r.readF32();
            mesh.rotation[2] = r.readF32();
            mesh.rotation[3] = 0.0f;
        }
        r.readVec3(mesh.scaling);
        mesh.hasBoundingBox = r.readU8() != 0;
        r.readVec3(mesh.bbMin);
        r.readVec3(mesh.bbMax);
        mesh.materialIndex = r.readI32();
        mesh.className     = r.readString();
        scene.meshes.push_back(std::move(mesh));
    }

    // --- Materials ---
    scene.materials.reserve(numMaterials);
    for (uint32_t i = 0; i < numMaterials && r.valid(); i++)
    {
        MaterialInfo mat{};
        mat.name            = r.readString();
        mat.typeName        = r.readString();
        mat.alpha           = r.readF32();
        mat.backFaceCulling = r.readU8() != 0;
        mat.wireframe       = r.readU8() != 0;
        r.readVec3(mat.diffuseColor);
        r.readVec3(mat.specularColor);
        r.readVec3(mat.emissiveColor);
        r.readVec3(mat.ambientColor);
        uint8_t numSlots = r.readU8();
        mat.textureSlots.reserve(numSlots);
        for (uint8_t j = 0; j < numSlots && r.valid(); j++)
        {
            TextureSlot slot;
            slot.slotName    = r.readString();
            slot.textureName = r.readString();
            mat.textureSlots.push_back(std::move(slot));
        }
        scene.materials.push_back(std::move(mat));
    }

    // --- Textures ---
    scene.textures.reserve(numTextures);
    for (uint32_t i = 0; i < numTextures && r.valid(); i++)
    {
        TextureInfo tex{};
        tex.name     = r.readString();
        tex.url      = r.readString();
        tex.width    = r.readI32();
        tex.height   = r.readI32();
        tex.hasAlpha = r.readU8() != 0;
        tex.isCube   = r.readU8() != 0;
        tex.className = r.readString();
        scene.textures.push_back(std::move(tex));
    }

    // --- Lights ---
    scene.lights.reserve(numLights);
    for (uint32_t i = 0; i < numLights && r.valid(); i++)
    {
        LightInfo light{};
        light.name      = r.readString();
        light.type      = r.readU8();
        r.readVec3(light.position);
        r.readVec3(light.direction);
        light.intensity = r.readF32();
        r.readVec3(light.diffuse);
        r.readVec3(light.specular);
        light.isEnabled = r.readU8() != 0;
        light.range     = r.readF32();
        scene.lights.push_back(std::move(light));
    }

    scene.valid = r.valid();
    return scene;
}

// ---------------------------------------------------------------------------
// Helpers for ImGui rendering
// ---------------------------------------------------------------------------
inline const char* CameraTypeName(uint8_t type)
{
    switch (type)
    {
    case 0:  return "FreeCamera";
    case 1:  return "ArcRotateCamera";
    case 2:  return "FollowCamera";
    default: return "Camera";
    }
}

inline const char* LightTypeName(uint8_t type)
{
    switch (type)
    {
    case 0:  return "PointLight";
    case 1:  return "DirectionalLight";
    case 2:  return "SpotLight";
    case 3:  return "HemisphericLight";
    default: return "Light";
    }
}

inline void ColorPreview(const char* label, const float color[3])
{
    char id[64];
    snprintf(id, sizeof(id), "##%s", label);
    ImVec4 c(color[0], color[1], color[2], 1.0f);
    ImGui::ColorButton(id, c, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
    ImGui::SameLine();
    ImGui::Text("%s: (%.2f, %.2f, %.2f)", label, color[0], color[1], color[2]);
}

inline void AttributeBadge(const char* name, bool has)
{
    if (has)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", name);
    else
        ImGui::TextDisabled("%s", name);
    ImGui::SameLine();
}

// ---------------------------------------------------------------------------
// Render the ImGui Scene Inspector window
// ---------------------------------------------------------------------------
inline void RenderInspector(const SceneData& data)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - 420, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(400, io.DisplaySize.y - 40), ImGuiCond_FirstUseEver);

    ImGui::Begin("Scene Inspector");

    if (!data.valid)
    {
        ImGui::TextDisabled("No scene data available");
        ImGui::End();
        return;
    }

    // Summary
    ImGui::Text("Meshes: %zu  Materials: %zu  Textures: %zu  Lights: %zu",
        data.meshes.size(), data.materials.size(),
        data.textures.size(), data.lights.size());
    ImGui::Separator();

    char hdr[128];

    // -----------------------------------------------------------------------
    // Cameras
    // -----------------------------------------------------------------------
    snprintf(hdr, sizeof(hdr), "Cameras (%zu)", data.cameras.size());
    if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t i = 0; i < data.cameras.size(); i++)
        {
            const auto& cam = data.cameras[i];
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]%s###cam%zu",
                cam.name.c_str(), CameraTypeName(cam.type),
                cam.isActive ? " *Active*" : "", i);
            if (ImGui::TreeNode(label))
            {
                ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                    cam.position[0], cam.position[1], cam.position[2]);
                ImGui::Text("Target:   (%.2f, %.2f, %.2f)",
                    cam.target[0], cam.target[1], cam.target[2]);
                ImGui::Text("FOV: %.3f rad (%.1f deg)",
                    cam.fov, cam.fov * 57.2957795f);
                ImGui::Text("Clip: %.2f - %.2f", cam.minZ, cam.maxZ);
                if (cam.isActive)
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                        "Active Camera");
                ImGui::TreePop();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Transform Nodes
    // -----------------------------------------------------------------------
    snprintf(hdr, sizeof(hdr), "Transform Nodes (%zu)", data.nodes.size());
    if (ImGui::CollapsingHeader(hdr))
    {
        for (size_t i = 0; i < data.nodes.size(); i++)
        {
            const auto& node = data.nodes[i];
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###node%zu",
                node.name.c_str(), node.className.c_str(), i);
            if (ImGui::TreeNode(label))
            {
                ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                    node.position[0], node.position[1], node.position[2]);
                if (node.hasQuaternion)
                    ImGui::Text("Rotation (quat): (%.3f, %.3f, %.3f, %.3f)",
                        node.rotation[0], node.rotation[1],
                        node.rotation[2], node.rotation[3]);
                else
                    ImGui::Text("Rotation: (%.2f, %.2f, %.2f)",
                        node.rotation[0], node.rotation[1], node.rotation[2]);
                ImGui::Text("Scale: (%.2f, %.2f, %.2f)",
                    node.scaling[0], node.scaling[1], node.scaling[2]);
                ImGui::Text("Enabled: %s", node.isEnabled ? "Yes" : "No");
                ImGui::TreePop();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Meshes
    // -----------------------------------------------------------------------
    snprintf(hdr, sizeof(hdr), "Meshes (%zu)", data.meshes.size());
    if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t i = 0; i < data.meshes.size(); i++)
        {
            const auto& mesh = data.meshes[i];
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###mesh%zu",
                mesh.name.c_str(), mesh.className.c_str(), i);
            if (ImGui::TreeNode(label))
            {
                ImGui::Text("Vertices: %u  |  Indices: %u",
                    mesh.totalVertices, mesh.totalIndices);

                // Attribute badges
                ImGui::Text("Attributes: ");
                ImGui::SameLine();
                AttributeBadge("Pos", mesh.hasPositions);
                AttributeBadge("Nrm", mesh.hasNormals);
                AttributeBadge("Tan", mesh.hasTangents);
                AttributeBadge("UV",  mesh.hasUVs);
                AttributeBadge("UV2", mesh.hasUV2s);
                AttributeBadge("Col", mesh.hasColors);
                ImGui::NewLine();

                ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                    mesh.position[0], mesh.position[1], mesh.position[2]);
                if (mesh.hasQuaternion)
                    ImGui::Text("Rotation (quat): (%.3f, %.3f, %.3f, %.3f)",
                        mesh.rotation[0], mesh.rotation[1],
                        mesh.rotation[2], mesh.rotation[3]);
                else
                    ImGui::Text("Rotation: (%.2f, %.2f, %.2f)",
                        mesh.rotation[0], mesh.rotation[1], mesh.rotation[2]);
                ImGui::Text("Scale: (%.2f, %.2f, %.2f)",
                    mesh.scaling[0], mesh.scaling[1], mesh.scaling[2]);

                if (mesh.hasBoundingBox)
                {
                    ImGui::Text("BBox Min: (%.2f, %.2f, %.2f)",
                        mesh.bbMin[0], mesh.bbMin[1], mesh.bbMin[2]);
                    ImGui::Text("BBox Max: (%.2f, %.2f, %.2f)",
                        mesh.bbMax[0], mesh.bbMax[1], mesh.bbMax[2]);
                }

                if (mesh.materialIndex >= 0 &&
                    mesh.materialIndex < static_cast<int32_t>(data.materials.size()))
                    ImGui::Text("Material: \"%s\"",
                        data.materials[mesh.materialIndex].name.c_str());
                else
                    ImGui::TextDisabled("Material: (none)");

                ImGui::Text("Visible: %s  |  Enabled: %s",
                    mesh.isVisible ? "Yes" : "No",
                    mesh.isEnabled ? "Yes" : "No");

                ImGui::TreePop();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Materials
    // -----------------------------------------------------------------------
    snprintf(hdr, sizeof(hdr), "Materials (%zu)", data.materials.size());
    if (ImGui::CollapsingHeader(hdr))
    {
        for (size_t i = 0; i < data.materials.size(); i++)
        {
            const auto& mat = data.materials[i];
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###mat%zu",
                mat.name.c_str(), mat.typeName.c_str(), i);
            if (ImGui::TreeNode(label))
            {
                ImGui::Text("Alpha: %.2f", mat.alpha);
                ImGui::Text("Back-face Culling: %s  |  Wireframe: %s",
                    mat.backFaceCulling ? "Yes" : "No",
                    mat.wireframe ? "Yes" : "No");

                ColorPreview("Diffuse",  mat.diffuseColor);
                ColorPreview("Specular", mat.specularColor);
                ColorPreview("Emissive", mat.emissiveColor);
                ColorPreview("Ambient",  mat.ambientColor);

                if (!mat.textureSlots.empty())
                {
                    ImGui::Text("Textures:");
                    for (const auto& slot : mat.textureSlots)
                        ImGui::BulletText("%s -> \"%s\"",
                            slot.slotName.c_str(), slot.textureName.c_str());
                }

                ImGui::TreePop();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Textures
    // -----------------------------------------------------------------------
    snprintf(hdr, sizeof(hdr), "Textures (%zu)", data.textures.size());
    if (ImGui::CollapsingHeader(hdr))
    {
        for (size_t i = 0; i < data.textures.size(); i++)
        {
            const auto& tex = data.textures[i];
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###tex%zu",
                tex.name.c_str(), tex.className.c_str(), i);
            if (ImGui::TreeNode(label))
            {
                if (!tex.url.empty())
                    ImGui::TextWrapped("URL: %s", tex.url.c_str());
                if (tex.width > 0 && tex.height > 0)
                    ImGui::Text("Size: %d x %d", tex.width, tex.height);
                else
                    ImGui::TextDisabled("Size: unknown");
                ImGui::Text("Has Alpha: %s  |  Cube: %s",
                    tex.hasAlpha ? "Yes" : "No",
                    tex.isCube   ? "Yes" : "No");
                ImGui::TreePop();
            }
        }
    }

    // -----------------------------------------------------------------------
    // Lights
    // -----------------------------------------------------------------------
    snprintf(hdr, sizeof(hdr), "Lights (%zu)", data.lights.size());
    if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t i = 0; i < data.lights.size(); i++)
        {
            const auto& light = data.lights[i];
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###light%zu",
                light.name.c_str(), LightTypeName(light.type), i);
            if (ImGui::TreeNode(label))
            {
                ImGui::Text("Intensity: %.2f", light.intensity);

                // Position for point/spot lights
                if (light.type == 0 || light.type == 2)
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        light.position[0], light.position[1],
                        light.position[2]);

                // Direction for directional/spot/hemispheric
                if (light.type != 0)
                    ImGui::Text("Direction: (%.2f, %.2f, %.2f)",
                        light.direction[0], light.direction[1],
                        light.direction[2]);

                ColorPreview("Diffuse",  light.diffuse);
                ColorPreview("Specular", light.specular);

                if (light.type == 0 || light.type == 2)
                    ImGui::Text("Range: %.2f", light.range);

                ImGui::Text("Enabled: %s", light.isEnabled ? "Yes" : "No");
                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

} // namespace SceneInspector

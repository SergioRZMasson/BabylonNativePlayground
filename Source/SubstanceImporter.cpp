#include "SubstanceImporter.h"

#ifdef HAS_SUBSTANCE_SDK

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string ExtractFileName(const std::string& path)
{
    auto pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

static std::string GraphDisplayName(const SubstanceAir::GraphDesc& desc)
{
    if (!desc.mLabel.empty())
        return std::string(desc.mLabel.c_str());
    auto url = std::string(desc.mPackageUrl.c_str());
    auto slash = url.find_last_of('/');
    return (slash != std::string::npos) ? url.substr(slash + 1) : url;
}

static size_t PixelFormatBytesPerPixel(uint8_t pf)
{
    uint8_t channels = (pf & 0x0C);  // Substance_PF_MASK_RAWChannels
    uint8_t precision = (pf & 0x50); // Substance_PF_MASK_RAWPrecision
    int numChannels = 4;
    if (channels == 0x0C)      numChannels = 1; // L
    else if (channels == 0x08) numChannels = 3; // RGB
    else if (channels == 0x04) numChannels = 4; // RGBx
    else                       numChannels = 4; // RGBA

    int bytesPerChannel = 1;
    if (precision == 0x10)      bytesPerChannel = 2; // 16I
    else if (precision == 0x50) bytesPerChannel = 2; // 16F
    else if (precision == 0x40) bytesPerChannel = 4; // 32F

    return static_cast<size_t>(numChannels * bytesPerChannel);
}

static const char* PixelFormatName(uint8_t pf)
{
    uint8_t base = pf & 0x5F; // mask out sRGB
    switch (base)
    {
        case Substance_PF_RGBA:        return "RGBA8";
        case Substance_PF_RGBA | Substance_PF_16b: return "RGBA16";
        case Substance_PF_RGBA | Substance_PF_16b | Substance_PF_FP: return "RGBA16F";
        case Substance_PF_RGBA | Substance_PF_FP: return "RGBA32F";
        case Substance_PF_RGB:         return "RGB8";
        case Substance_PF_L:           return "L8";
        case Substance_PF_L | Substance_PF_16b: return "L16";
        default:                       return "Unknown";
    }
}

static const char* ChannelUseName(SubstanceAir::ChannelUse ch)
{
    switch (ch)
    {
        case SubstanceAir::Channel_BaseColor:        return "BaseColor";
        case SubstanceAir::Channel_Normal:           return "Normal";
        case SubstanceAir::Channel_Roughness:        return "Roughness";
        case SubstanceAir::Channel_Metallic:         return "Metallic";
        case SubstanceAir::Channel_AmbientOcclusion: return "AmbientOcclusion";
        case SubstanceAir::Channel_Emissive:         return "Emissive";
        case SubstanceAir::Channel_Height:           return "Height";
        case SubstanceAir::Channel_Opacity:          return "Opacity";
        case SubstanceAir::Channel_Glossiness:       return "Glossiness";
        case SubstanceAir::Channel_Specular:         return "Specular";
        case SubstanceAir::Channel_Diffuse:          return "Diffuse";
        default:                                     return "Other";
    }
}

// ---------------------------------------------------------------------------
// SubstanceImporter
// ---------------------------------------------------------------------------
SubstanceImporter::SubstanceImporter()
    : m_renderer(std::make_unique<SubstanceAir::Renderer>())
{
}

SubstanceImporter::~SubstanceImporter()
{
    m_materials.clear();
    m_renderer.reset();
}

void SubstanceImporter::SetTextureCallback(SubstanceTextureCallback callback)
{
    m_textureCallback = std::move(callback);
}

bool SubstanceImporter::LoadSbsarFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "[Substance] Failed to open: " << filePath << std::endl;
        return false;
    }

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    LoadedMaterial mat;
    mat.filePath = filePath;
    mat.displayName = ExtractFileName(filePath);
    mat.archiveData.resize(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(mat.archiveData.data()), fileSize))
    {
        std::cerr << "[Substance] Failed to read: " << filePath << std::endl;
        return false;
    }
    file.close();

    SubstanceAir::OutputOptions outputOptions{};
    outputOptions.mAllowedFormats = SubstanceAir::Format_RGBA8;
    outputOptions.mMipmap = SubstanceAir::Mipmap_ForceNone;

    mat.package = std::make_unique<SubstanceAir::PackageDesc>(
        mat.archiveData.data(),
        mat.archiveData.size(),
        outputOptions);

    if (!mat.package->isValid())
    {
        std::cerr << "[Substance] Invalid package: " << filePath << std::endl;
        return false;
    }

    SubstanceAir::instantiate(mat.graphInstances, *mat.package);

    // Push all graph instances and do an initial render
    m_renderer->push(mat.graphInstances);
    m_renderer->run();

    // Grab initial outputs and send to callback
    for (auto& graphPtr : mat.graphInstances)
    {
        const auto& outputs = graphPtr->getOutputs();
        for (auto* output : outputs)
        {
            SubstanceAir::OutputInstance::Result result(output->grabResult());
            if (result.get() && result->isImage())
            {
                auto* imgResult = dynamic_cast<SubstanceAir::RenderResultImage*>(result.get());
                const auto& tex = imgResult->getTexture();
                if (tex.buffer && m_textureCallback)
                {
                    SubstanceTextureOutput texOut;
                    texOut.identifier = std::string(output->mDesc.mIdentifier.c_str());
                    texOut.graphUrl = std::string(graphPtr->mDesc.mPackageUrl.c_str());
                    texOut.width = tex.level0Width;
                    texOut.height = tex.level0Height;
                    texOut.pixelFormat = tex.pixelFormat;
                    texOut.channelsOrder = tex.channelsOrder;
                    size_t bpp = PixelFormatBytesPerPixel(tex.pixelFormat);
                    texOut.dataSize = static_cast<size_t>(tex.level0Width) * tex.level0Height * bpp;
                    texOut.data = static_cast<const uint8_t*>(tex.buffer);
                    m_textureCallback(texOut);
                }
            }
        }
    }

    std::cout << "[Substance] Loaded: " << mat.displayName
              << " (" << mat.graphInstances.size() << " graph(s))" << std::endl;

    m_selectedMaterial = static_cast<int>(m_materials.size());
    m_selectedGraph = 0;
    m_materials.push_back(std::move(mat));
    return true;
}

// ---------------------------------------------------------------------------
// Input parameter UI
// ---------------------------------------------------------------------------
void SubstanceImporter::RenderMaterialInputs(SubstanceAir::GraphInstance& graph)
{
    using namespace SubstanceAir;

    const auto& inputs = graph.getInputs();
    if (inputs.empty())
    {
        ImGui::TextDisabled("No input parameters");
        return;
    }

    bool anyChanged = false;
    std::string currentGroup;

    for (auto* inputBase : inputs)
    {
        const auto& desc = inputBase->mDesc;

        // Skip image and font inputs (not editable via sliders)
        if (desc.mType == Substance_IOType_Image || desc.mType == Substance_IOType_Font)
            continue;

        // Group header
        std::string group(desc.mGuiGroup.c_str());
        if (group != currentGroup)
        {
            if (!group.empty())
                ImGui::SeparatorText(group.c_str());
            currentGroup = group;
        }

        std::string label = desc.mLabel.empty()
            ? std::string(desc.mIdentifier.c_str())
            : std::string(desc.mLabel.c_str());
        std::string id = "##" + std::string(desc.mIdentifier.c_str());

        // Tooltip on hover
        if (!desc.mGuiDescription.empty())
        {
            // Render label then tooltip
        }

        switch (desc.mType)
        {
            case Substance_IOType_Float:
            {
                auto* input = static_cast<InputInstanceFloat*>(inputBase);
                auto& d = input->getDesc();
                float val = input->getValue();
                float mn = d.mMinValue, mx = d.mMaxValue;
                if (mn >= mx) { mn = 0.0f; mx = 1.0f; }

                if (desc.mGuiWidget == Input_Angle)
                {
                    float degrees = val * 57.2957795f;
                    float mnDeg = mn * 57.2957795f, mxDeg = mx * 57.2957795f;
                    if (ImGui::SliderFloat((label + id).c_str(), &degrees, mnDeg, mxDeg, "%.1f deg"))
                    {
                        input->setValue(degrees / 57.2957795f);
                        anyChanged = true;
                    }
                }
                else
                {
                    if (ImGui::SliderFloat((label + id).c_str(), &val, mn, mx))
                    {
                        input->setValue(val);
                        anyChanged = true;
                    }
                }
                break;
            }

            case Substance_IOType_Float2:
            {
                auto* input = static_cast<InputInstanceFloat2*>(inputBase);
                auto& d = input->getDesc();
                Vec2Float val = input->getValue();
                float v[2] = {val.x, val.y};
                if (ImGui::SliderFloat2((label + id).c_str(), v,
                        d.mMinValue.x, d.mMaxValue.x >= d.mMinValue.x ? d.mMaxValue.x : 1.0f))
                {
                    input->setValue(Vec2Float(v[0], v[1]));
                    anyChanged = true;
                }
                break;
            }

            case Substance_IOType_Float3:
            {
                auto* input = static_cast<InputInstanceFloat3*>(inputBase);
                Vec3Float val = input->getValue();
                float v[3] = {val.x, val.y, val.z};
                bool changed = false;
                if (desc.mGuiWidget == Input_Color)
                    changed = ImGui::ColorEdit3((label + id).c_str(), v);
                else
                {
                    auto& d = input->getDesc();
                    changed = ImGui::SliderFloat3((label + id).c_str(), v,
                        d.mMinValue.x, d.mMaxValue.x >= d.mMinValue.x ? d.mMaxValue.x : 1.0f);
                }
                if (changed)
                {
                    input->setValue(Vec3Float(v[0], v[1], v[2]));
                    anyChanged = true;
                }
                break;
            }

            case Substance_IOType_Float4:
            {
                auto* input = static_cast<InputInstanceFloat4*>(inputBase);
                Vec4Float val = input->getValue();
                float v[4] = {val.x, val.y, val.z, val[3]};
                bool changed = false;
                if (desc.mGuiWidget == Input_Color)
                    changed = ImGui::ColorEdit4((label + id).c_str(), v);
                else
                {
                    auto& d = input->getDesc();
                    changed = ImGui::SliderFloat4((label + id).c_str(), v,
                        d.mMinValue.x, d.mMaxValue.x >= d.mMinValue.x ? d.mMaxValue.x : 1.0f);
                }
                if (changed)
                {
                    input->setValue(Vec4Float(v[0], v[1], v[2], v[3]));
                    anyChanged = true;
                }
                break;
            }

            case Substance_IOType_Integer:
            {
                auto* input = static_cast<InputInstanceInt*>(inputBase);
                auto& d = input->getDesc();
                int val = input->getValue();

                if (desc.mGuiWidget == Input_Togglebutton)
                {
                    bool b = (val != 0);
                    if (ImGui::Checkbox((label + id).c_str(), &b))
                    {
                        input->setValue(b ? 1 : 0);
                        anyChanged = true;
                    }
                }
                else if (desc.mGuiWidget == Input_Combobox && !d.mEnumValues.empty())
                {
                    // Build combo items
                    int currentIdx = 0;
                    std::vector<std::string> items;
                    items.reserve(d.mEnumValues.size());
                    for (size_t i = 0; i < d.mEnumValues.size(); ++i)
                    {
                        items.push_back(std::string(d.mEnumValues[i].second.c_str()));
                        if (d.mEnumValues[i].first == val)
                            currentIdx = static_cast<int>(i);
                    }

                    if (ImGui::BeginCombo((label + id).c_str(), items[currentIdx].c_str()))
                    {
                        for (size_t i = 0; i < items.size(); ++i)
                        {
                            bool selected = (static_cast<int>(i) == currentIdx);
                            if (ImGui::Selectable(items[i].c_str(), selected))
                            {
                                input->setValue(d.mEnumValues[i].first);
                                anyChanged = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    int mn = d.mMinValue, mx = d.mMaxValue;
                    if (mn >= mx) { mn = 0; mx = 16; }
                    if (ImGui::SliderInt((label + id).c_str(), &val, mn, mx))
                    {
                        input->setValue(val);
                        anyChanged = true;
                    }
                }
                break;
            }

            case Substance_IOType_Integer2:
            {
                auto* input = static_cast<InputInstanceInt2*>(inputBase);
                auto& d = input->getDesc();
                Vec2Int val = input->getValue();
                int v[2] = {val.x, val.y};
                if (ImGui::SliderInt2((label + id).c_str(), v,
                        d.mMinValue.x, d.mMaxValue.x >= d.mMinValue.x ? d.mMaxValue.x : 16))
                {
                    input->setValue(Vec2Int(v[0], v[1]));
                    anyChanged = true;
                }
                break;
            }

            case Substance_IOType_Integer3:
            {
                auto* input = static_cast<InputInstanceInt3*>(inputBase);
                auto& d = input->getDesc();
                Vec3Int val = input->getValue();
                int v[3] = {val.x, val.y, val.z};
                if (ImGui::SliderInt3((label + id).c_str(), v,
                        d.mMinValue.x, d.mMaxValue.x >= d.mMinValue.x ? d.mMaxValue.x : 16))
                {
                    input->setValue(Vec3Int(v[0], v[1], v[2]));
                    anyChanged = true;
                }
                break;
            }

            case Substance_IOType_Integer4:
            {
                auto* input = static_cast<InputInstanceInt4*>(inputBase);
                auto& d = input->getDesc();
                Vec4Int val = input->getValue();
                int v[4] = {val.x, val.y, val.z, val[3]};
                if (ImGui::SliderInt4((label + id).c_str(), v,
                        d.mMinValue.x, d.mMaxValue.x >= d.mMinValue.x ? d.mMaxValue.x : 16))
                {
                    input->setValue(Vec4Int(v[0], v[1], v[2], v[3]));
                    anyChanged = true;
                }
                break;
            }

            case Substance_IOType_String:
            {
                auto* input = static_cast<InputInstanceString*>(inputBase);
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s", input->getString().c_str());
                if (ImGui::InputText((label + id).c_str(), buf, sizeof(buf)))
                {
                    input->setString(SubstanceAir::string(buf));
                    anyChanged = true;
                }
                break;
            }

            default:
                break;
        }

        // Tooltip
        if (!desc.mGuiDescription.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", desc.mGuiDescription.c_str());
    }

    // Re-render if any input changed
    if (anyChanged)
    {
        m_renderer->push(graph);
        m_renderer->run();

        const auto& outputs = graph.getOutputs();
        for (auto* output : outputs)
        {
            SubstanceAir::OutputInstance::Result result(output->grabResult());
            if (result.get() && result->isImage())
            {
                auto* imgResult = dynamic_cast<SubstanceAir::RenderResultImage*>(result.get());
                const auto& tex = imgResult->getTexture();
                if (tex.buffer && m_textureCallback)
                {
                    SubstanceTextureOutput texOut;
                    texOut.identifier = std::string(output->mDesc.mIdentifier.c_str());
                    texOut.graphUrl = std::string(graph.mDesc.mPackageUrl.c_str());
                    texOut.width = tex.level0Width;
                    texOut.height = tex.level0Height;
                    texOut.pixelFormat = tex.pixelFormat;
                    texOut.channelsOrder = tex.channelsOrder;
                    size_t bpp = PixelFormatBytesPerPixel(tex.pixelFormat);
                    texOut.dataSize = static_cast<size_t>(tex.level0Width) * tex.level0Height * bpp;
                    texOut.data = static_cast<const uint8_t*>(tex.buffer);
                    m_textureCallback(texOut);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Output info display
// ---------------------------------------------------------------------------
void SubstanceImporter::RenderMaterialOutputs(SubstanceAir::GraphInstance& graph)
{
    const auto& outputs = graph.getOutputs();
    if (outputs.empty())
    {
        ImGui::TextDisabled("No outputs");
        return;
    }

    for (auto* output : outputs)
    {
        const auto& desc = output->mDesc;
        std::string outLabel = desc.mLabel.empty()
            ? std::string(desc.mIdentifier.c_str())
            : std::string(desc.mLabel.c_str());

        if (ImGui::TreeNode(outLabel.c_str()))
        {
            ImGui::Text("Identifier: %s", desc.mIdentifier.c_str());
            if (desc.isImage())
            {
                int w = 1 << desc.mWidth;
                int h = 1 << desc.mHeight;
                ImGui::Text("Size: %d x %d", w, h);
                ImGui::Text("Format: %s%s", PixelFormatName(static_cast<uint8_t>(desc.mFormat)),
                    (desc.mFormat & Substance_PF_sRGB) ? " (sRGB)" : "");
            }
            if (!desc.mChannels.empty())
            {
                std::string channels;
                for (auto ch : desc.mChannels)
                {
                    if (!channels.empty()) channels += ", ";
                    channels += ChannelUseName(ch);
                }
                ImGui::Text("Channel: %s", channels.c_str());
            }
            ImGui::Text("Enabled: %s", output->mEnabled ? "Yes" : "No");
            ImGui::TreePop();
        }
    }
}

// ---------------------------------------------------------------------------
// Per-graph rendering
// ---------------------------------------------------------------------------
void SubstanceImporter::RenderGraph(LoadedMaterial& mat, size_t graphIdx)
{
    if (graphIdx >= mat.graphInstances.size())
        return;

    auto& graph = *mat.graphInstances[graphIdx];

    if (ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen))
    {
        RenderMaterialInputs(graph);
    }

    if (ImGui::CollapsingHeader("Outputs", ImGuiTreeNodeFlags_DefaultOpen))
    {
        RenderMaterialOutputs(graph);
    }
}

// ---------------------------------------------------------------------------
// Main render (tab content — no window wrapper)
// ---------------------------------------------------------------------------
void SubstanceImporter::RenderTab()
{
    if (m_materials.empty())
    {
        ImGui::TextWrapped("No Substance materials loaded.\nDrag & drop .sbsar files to import.");
        return;
    }

    // Material list
    if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t i = 0; i < m_materials.size(); ++i)
        {
            bool selected = (m_selectedMaterial == static_cast<int>(i));
            if (ImGui::Selectable(m_materials[i].displayName.c_str(), selected))
            {
                m_selectedMaterial = static_cast<int>(i);
                m_selectedGraph = 0;
            }
        }
    }

    ImGui::Separator();

    // Selected material detail
    if (m_selectedMaterial >= 0 && m_selectedMaterial < static_cast<int>(m_materials.size()))
    {
        auto& mat = m_materials[m_selectedMaterial];

        // Graph selector (if multiple graphs in the package)
        if (mat.graphInstances.size() > 1)
        {
            if (ImGui::BeginCombo("Graph", GraphDisplayName(
                    mat.graphInstances[m_selectedGraph]->mDesc).c_str()))
            {
                for (size_t i = 0; i < mat.graphInstances.size(); ++i)
                {
                    bool selected = (m_selectedGraph == static_cast<int>(i));
                    auto name = GraphDisplayName(mat.graphInstances[i]->mDesc);
                    if (ImGui::Selectable(name.c_str(), selected))
                        m_selectedGraph = static_cast<int>(i);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else if (!mat.graphInstances.empty())
        {
            ImGui::Text("Graph: %s",
                GraphDisplayName(mat.graphInstances[0]->mDesc).c_str());
        }

        ImGui::Separator();

        if (m_selectedGraph >= 0 && m_selectedGraph < static_cast<int>(mat.graphInstances.size()))
        {
            RenderGraph(mat, static_cast<size_t>(m_selectedGraph));
        }
    }
}

#else // !HAS_SUBSTANCE_SDK

// Stub implementation when Substance SDK is not available
SubstanceImporter::SubstanceImporter() = default;
SubstanceImporter::~SubstanceImporter() = default;
void SubstanceImporter::SetTextureCallback(SubstanceTextureCallback) {}
bool SubstanceImporter::LoadSbsarFile(const std::string&) { return false; }
void SubstanceImporter::RenderTab() {}

#endif // HAS_SUBSTANCE_SDK

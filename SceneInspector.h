#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

namespace SceneInspector
{

static constexpr uint32_t MAGIC   = 0x42534E44; // "BSND"
static constexpr uint32_t VERSION = 2;

static constexpr uint32_t CMD_MAGIC   = 0x434D4432; // "CMD2"
static constexpr uint32_t CMD_VERSION = 1;

static constexpr float RAD2DEG = 57.2957795f;
static constexpr float DEG2RAD = 0.0174532925f;
static constexpr float FLT_BIG = 3.402823e+38f;

// ===========================================================================
// Command types (must match JS constants)
// ===========================================================================
enum InspectorCmd : uint8_t
{
    CMD_SET_POSITION            = 0x01,
    CMD_SET_ROTATION            = 0x02,
    CMD_SET_SCALING             = 0x03,
    CMD_SET_ENABLED             = 0x10,
    CMD_SET_VISIBLE             = 0x11,
    CMD_SET_WIREFRAME           = 0x12,
    CMD_SET_BACK_FACE_CULLING   = 0x13,
    CMD_SET_RECEIVE_SHADOWS     = 0x14,
    CMD_SET_POINTS_CLOUD_MAT    = 0x15,
    CMD_SET_DISABLE_DEPTH_WRITE = 0x16,
    CMD_SET_FORCE_DEPTH_WRITE   = 0x17,
    CMD_SET_AUTO_CLEAR          = 0x18,
    CMD_SET_SHADOW_ENABLED      = 0x19,
    CMD_SET_ALPHA               = 0x20,
    CMD_SET_VISIBILITY          = 0x21,
    CMD_SET_INTENSITY           = 0x22,
    CMD_SET_FOV                 = 0x23,
    CMD_SET_MIN_Z               = 0x24,
    CMD_SET_MAX_Z               = 0x25,
    CMD_SET_INERTIA             = 0x26,
    CMD_SET_SPEED               = 0x27,
    CMD_SET_RANGE               = 0x28,
    CMD_SET_SPOT_ANGLE          = 0x29,
    CMD_SET_INNER_ANGLE         = 0x2A,
    CMD_SET_EXPONENT            = 0x2B,
    CMD_SET_ALPHA_CUTOFF        = 0x2C,
    CMD_SET_Z_OFFSET            = 0x2D,
    CMD_SET_Z_OFFSET_UNITS      = 0x2E,
    CMD_SET_METALLIC            = 0x2F,
    CMD_SET_ROUGHNESS           = 0x30,
    CMD_SET_ENV_INTENSITY       = 0x31,
    CMD_SET_CONTRAST            = 0x32,
    CMD_SET_EXPOSURE            = 0x33,
    CMD_SET_SPEED_RATIO         = 0x34,
    CMD_SET_ARC_ALPHA           = 0x35,
    CMD_SET_ARC_BETA            = 0x36,
    CMD_SET_ARC_RADIUS          = 0x37,
    CMD_SET_FOG_DENSITY         = 0x38,
    CMD_SET_FOG_START           = 0x39,
    CMD_SET_FOG_END             = 0x3A,
    CMD_SET_LAYER_MASK          = 0x3B,
    CMD_SET_TEXTURE_LEVEL       = 0x3C,
    CMD_SET_MORPH_INFLUENCE     = 0x3D,
    CMD_SET_SHADOW_BIAS         = 0x3E,
    CMD_SET_SHADOW_NORMAL_BIAS  = 0x3F,
    CMD_SET_DIFFUSE_COLOR       = 0x40,
    CMD_SET_SPECULAR_COLOR      = 0x41,
    CMD_SET_EMISSIVE_COLOR      = 0x42,
    CMD_SET_AMBIENT_COLOR       = 0x43,
    CMD_SET_LIGHT_DIFFUSE       = 0x44,
    CMD_SET_LIGHT_SPECULAR      = 0x45,
    CMD_SET_FOG_COLOR           = 0x46,
    CMD_SET_SCENE_AMBIENT       = 0x47,
    CMD_SET_CLEAR_COLOR         = 0x50,
    CMD_SET_DIRECTION           = 0x60,
    CMD_SET_TARGET              = 0x61,
    CMD_SET_GRAVITY             = 0x62,
    CMD_SET_TRANSPARENCY_MODE   = 0x70,
    CMD_SET_FOG_MODE            = 0x71,
    CMD_SET_TONE_MAPPING_TYPE   = 0x72,
    CMD_SET_CAMERA_MODE         = 0x73,
    CMD_SET_NAME                = 0x80,
    CMD_DISPOSE_ENTITY          = 0x90,
    CMD_ANIM_PLAY               = 0x91,
    CMD_ANIM_PAUSE              = 0x92,
    CMD_ANIM_STOP               = 0x93,
    CMD_SKELETON_RETURN_TO_REST = 0x94,
    CMD_SET_FORCE_WIREFRAME     = 0x95,
    CMD_SET_FORCE_POINTS_CLOUD  = 0x96,
    CMD_DEBUG_TOGGLE_GRID       = 0xA0,
    CMD_DEBUG_TOGGLE_NAMES      = 0xA1,
    CMD_DEBUG_TOGGLE_PHYSICS    = 0xA2,
    CMD_DEBUG_TOGGLE_BBOX       = 0xA4,
    CMD_DEBUG_TOGGLE_AXES       = 0xA5,
    CMD_DEBUG_SET_TEX_CHANNEL   = 0xA6,
    CMD_DEBUG_TOGGLE_ANIMATIONS = 0xA7,
    CMD_DEBUG_TOGGLE_PARTICLES  = 0xA8,
    CMD_DEBUG_TOGGLE_FOG        = 0xA9,
    CMD_DEBUG_TOGGLE_SHADOWS    = 0xAA,
    CMD_DEBUG_TOGGLE_LIGHTS     = 0xAB,
    CMD_DEBUG_TOGGLE_POSTPROC   = 0xAC,
    CMD_DEBUG_TOGGLE_SKELETONS  = 0xAD,
    CMD_SET_ANIM_FRAME          = 0xB0,
    CMD_SET_ANIM_LOOP           = 0xB1,
    CMD_SELECT_ENTITY           = 0xC0,
};

// ===========================================================================
// Binary reader with bounds checking
// ===========================================================================
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

    void readColor4(float out[4])
    {
        out[0] = readF32();
        out[1] = readF32();
        out[2] = readF32();
        out[3] = readF32();
    }
};

// ===========================================================================
// Binary writer for command buffer
// ===========================================================================
class BinaryWriter
{
    std::vector<uint8_t> m_buf;

public:
    BinaryWriter() { m_buf.reserve(16384); }

    void clear() { m_buf.clear(); }
    const uint8_t* data() const { return m_buf.data(); }
    size_t size() const { return m_buf.size(); }
    bool empty() const { return m_buf.empty(); }

    void writeU8(uint8_t v) { m_buf.push_back(v); }

    void writeU16(uint16_t v)
    {
        m_buf.resize(m_buf.size() + 2);
        std::memcpy(m_buf.data() + m_buf.size() - 2, &v, 2);
    }

    void writeU32(uint32_t v)
    {
        m_buf.resize(m_buf.size() + 4);
        std::memcpy(m_buf.data() + m_buf.size() - 4, &v, 4);
    }

    void writeI32(int32_t v)
    {
        m_buf.resize(m_buf.size() + 4);
        std::memcpy(m_buf.data() + m_buf.size() - 4, &v, 4);
    }

    void writeF32(float v)
    {
        m_buf.resize(m_buf.size() + 4);
        std::memcpy(m_buf.data() + m_buf.size() - 4, &v, 4);
    }

    void writeVec3(const float v[3])
    {
        writeF32(v[0]); writeF32(v[1]); writeF32(v[2]);
    }

    void writeColor3(const float c[3])
    {
        writeF32(c[0]); writeF32(c[1]); writeF32(c[2]);
    }

    void writeColor4(const float c[4])
    {
        writeF32(c[0]); writeF32(c[1]); writeF32(c[2]); writeF32(c[3]);
    }

    void writeString(const std::string& s)
    {
        uint16_t len = static_cast<uint16_t>(s.size() > 65535 ? 65535 : s.size());
        writeU16(len);
        m_buf.insert(m_buf.end(), s.begin(), s.begin() + len);
    }

    // Write placeholder at position, return position for later patching
    size_t writePlaceholderU32()
    {
        size_t pos = m_buf.size();
        writeU32(0);
        return pos;
    }

    void patchU32(size_t pos, uint32_t v)
    {
        std::memcpy(m_buf.data() + pos, &v, 4);
    }
};

// ===========================================================================
// Data structures (v2)
// ===========================================================================
struct CameraInfo
{
    uint32_t uniqueId{};
    uint32_t parentId{};
    std::string name;
    std::string className;
    uint8_t type{}; // 0=free, 1=arc, 2=follow, 3=other
    float position[3]{};
    float target[3]{};
    float fov{};
    float minZ{}, maxZ{};
    bool isActive{};
    float inertia{};
    float speed{};
    uint8_t mode{}; // 0=perspective, 1=ortho
    bool isArcRotate{};
    float alpha{}, beta{}, radius{};
    float lowerAlphaLimit{-FLT_BIG}, upperAlphaLimit{FLT_BIG};
    float lowerBetaLimit{0.01f}, upperBetaLimit{3.1316f};
    float lowerRadiusLimit{0}, upperRadiusLimit{FLT_BIG};
};

struct NodeInfo
{
    uint32_t uniqueId{};
    uint32_t parentId{};
    std::string name;
    std::string className;
    float position[3]{};
    bool hasQuaternion{};
    float rotation[4]{};
    float scaling[3]{};
    bool isEnabled{};
};

struct MorphTarget
{
    std::string name;
    float influence{};
};

struct MeshInfo
{
    uint32_t uniqueId{};
    uint32_t parentId{};
    std::string name;
    std::string className;
    float position[3]{};
    bool hasQuaternion{};
    float rotation[4]{};
    float scaling[3]{};
    bool isEnabled{};
    bool isVisible{};
    float visibility{1.0f};
    bool receiveShadows{};
    uint32_t layerMask{0x0FFFFFFF};
    uint32_t totalVertices{};
    uint32_t totalIndices{};
    bool hasPositions{}, hasNormals{}, hasTangents{};
    bool hasUVs{}, hasUV2s{}, hasColors{};
    bool hasBoundingBox{};
    float bbMin[3]{}, bbMax[3]{};
    int32_t materialUniqueId{-1};
    std::vector<MorphTarget> morphTargets;
};

struct TextureSlot
{
    std::string slotName;
    std::string textureName;
    int32_t textureUniqueId{-1};
};

struct MaterialInfo
{
    uint32_t uniqueId{};
    std::string name;
    std::string typeName;
    float alpha{1.0f};
    bool backFaceCulling{}, wireframe{}, pointsCloud{};
    bool disableDepthWrite{}, forceDepthWrite{};
    float zOffset{}, zOffsetUnits{};
    int32_t transparencyMode{-1};
    float alphaCutOff{0.4f};
    float diffuseColor[3]{};
    float specularColor[3]{};
    float emissiveColor[3]{};
    float ambientColor[3]{};
    bool isPBR{};
    float metallic{}, roughness{1.0f}, environmentIntensity{1.0f};
    std::vector<TextureSlot> textureSlots;
};

struct TextureInfo
{
    uint32_t uniqueId{};
    std::string name;
    std::string className;
    std::string url;
    int32_t width{-1}, height{-1};
    bool hasAlpha{}, isCube{}, gammaSpace{};
    uint8_t coordinatesMode{};
    uint8_t samplingMode{};
    float uOffset{}, vOffset{};
    float uScale{1}, vScale{1};
    float level{1};
};

struct LightInfo
{
    uint32_t uniqueId{};
    uint32_t parentId{};
    std::string name;
    std::string className;
    uint8_t type{}; // 0=point, 1=directional, 2=spot, 3=hemispheric
    float position[3]{};
    float direction[3]{};
    float intensity{};
    float diffuse[3]{};
    float specular[3]{};
    bool isEnabled{};
    float range{};
    bool isSpot{};
    float spotAngle{}, innerAngle{}, exponent{};
    bool hasShadowGen{};
    float shadowCasterCount{};
    float shadowBias{}, shadowNormalBias{};
};

struct AnimGroupInfo
{
    uint32_t index{};
    std::string name;
    bool isPlaying{};
    float speedRatio{1.0f};
    float from{}, to{};
    bool loopAnimation{};
    float currentFrame{};
};

struct SkeletonInfo
{
    uint32_t index{};
    std::string name;
    uint32_t boneCount{};
    bool useTextureForBones{};
};

struct SceneProperties
{
    float clearColor[4]{0, 0, 0, 1};
    float ambientColor[3]{};
    uint8_t fogMode{}; // 0=none, 1=linear, 2=exp, 3=exp2
    float fogColor[3]{};
    float fogDensity{};
    float fogStart{};
    float fogEnd{};
    bool forceWireframe{};
    bool forcePointsCloud{};
    bool autoClear{true};
    float gravity[3]{};
    bool hasImageProcessing{};
    float contrast{1.0f};
    float exposure{1.0f};
    bool toneMappingEnabled{};
    uint8_t toneMappingType{};
    bool vignetteEnabled{};
};

struct StatsData
{
    float fps{};
    uint32_t totalVertices{};
    uint32_t activeIndices{};
    uint32_t activeMeshes{};
    uint32_t activeParticles{};
    uint32_t activeBones{};
    uint32_t drawCalls{};
    float frameTime{};
    float interFrameTime{};
    float renderTime{};
    float cameraRenderTime{};
    float activeMeshesEvalTime{};
    float renderTargetsTime{};
    float particlesTime{};
    float spritesTime{};
    float animationsTime{};
    float physicsTime{};
    std::string engineVersion;
    float hardwareScaling{1.0f};
    int32_t renderWidth{}, renderHeight{};
};

struct SceneData
{
    bool valid = false;
    SceneProperties scene;
    StatsData stats;
    std::vector<CameraInfo>    cameras;
    std::vector<NodeInfo>      nodes;
    std::vector<MeshInfo>      meshes;
    std::vector<MaterialInfo>  materials;
    std::vector<TextureInfo>   textures;
    std::vector<LightInfo>     lights;
    std::vector<AnimGroupInfo> animGroups;
    std::vector<SkeletonInfo>  skeletons;
};

// ===========================================================================
// Parse binary buffer into SceneData (v2)
// ===========================================================================
inline SceneData Parse(const uint8_t* data, size_t size)
{
    SceneData sd;
    if (!data || size < 40) return sd;

    BinaryReader r(data, size);

    uint32_t magic   = r.readU32();
    uint32_t version = r.readU32();
    if (!r.valid() || magic != MAGIC || version != VERSION) return sd;

    uint32_t numCameras   = r.readU32();
    uint32_t numNodes     = r.readU32();
    uint32_t numMeshes    = r.readU32();
    uint32_t numMaterials = r.readU32();
    uint32_t numTextures  = r.readU32();
    uint32_t numLights    = r.readU32();
    uint32_t numAnimGroups= r.readU32();
    uint32_t numSkeletons = r.readU32();
    if (!r.valid()) return sd;

    // Scene properties
    auto& sp = sd.scene;
    r.readColor4(sp.clearColor);
    r.readVec3(sp.ambientColor);
    sp.fogMode = r.readU8();
    r.readVec3(sp.fogColor);
    sp.fogDensity = r.readF32();
    sp.fogStart   = r.readF32();
    sp.fogEnd     = r.readF32();
    sp.forceWireframe  = r.readU8() != 0;
    sp.forcePointsCloud = r.readU8() != 0;
    sp.autoClear       = r.readU8() != 0;
    r.readVec3(sp.gravity);

    sp.hasImageProcessing = r.readU8() != 0;
    if (sp.hasImageProcessing)
    {
        sp.contrast           = r.readF32();
        sp.exposure           = r.readF32();
        sp.toneMappingEnabled = r.readU8() != 0;
        sp.toneMappingType    = r.readU8();
        sp.vignetteEnabled    = r.readU8() != 0;
    }

    // Stats
    auto& st = sd.stats;
    st.fps             = r.readF32();
    st.totalVertices   = r.readU32();
    st.activeIndices   = r.readU32();
    st.activeMeshes    = r.readU32();
    st.activeParticles = r.readU32();
    st.activeBones     = r.readU32();
    st.drawCalls       = r.readU32();
    st.frameTime            = r.readF32();
    st.interFrameTime       = r.readF32();
    st.renderTime           = r.readF32();
    st.cameraRenderTime     = r.readF32();
    st.activeMeshesEvalTime = r.readF32();
    st.renderTargetsTime    = r.readF32();
    st.particlesTime        = r.readF32();
    st.spritesTime          = r.readF32();
    st.animationsTime       = r.readF32();
    st.physicsTime          = r.readF32();
    st.engineVersion   = r.readString();
    st.hardwareScaling = r.readF32();
    st.renderWidth     = r.readI32();
    st.renderHeight    = r.readI32();

    if (!r.valid()) return sd;

    // --- Cameras ---
    sd.cameras.reserve(numCameras);
    for (uint32_t i = 0; i < numCameras && r.valid(); i++)
    {
        CameraInfo cam{};
        cam.uniqueId  = r.readU32();
        cam.parentId  = r.readU32();
        cam.name      = r.readString();
        cam.className = r.readString();
        cam.type      = r.readU8();
        r.readVec3(cam.position);
        r.readVec3(cam.target);
        cam.fov       = r.readF32();
        cam.minZ      = r.readF32();
        cam.maxZ      = r.readF32();
        cam.isActive  = r.readU8() != 0;
        cam.inertia   = r.readF32();
        cam.speed     = r.readF32();
        cam.mode      = r.readU8();
        cam.isArcRotate = r.readU8() != 0;
        if (cam.isArcRotate)
        {
            cam.alpha  = r.readF32();
            cam.beta   = r.readF32();
            cam.radius = r.readF32();
            cam.lowerAlphaLimit  = r.readF32();
            cam.upperAlphaLimit  = r.readF32();
            cam.lowerBetaLimit   = r.readF32();
            cam.upperBetaLimit   = r.readF32();
            cam.lowerRadiusLimit = r.readF32();
            cam.upperRadiusLimit = r.readF32();
        }
        sd.cameras.push_back(std::move(cam));
    }

    // --- Transform Nodes ---
    sd.nodes.reserve(numNodes);
    for (uint32_t i = 0; i < numNodes && r.valid(); i++)
    {
        NodeInfo node{};
        node.uniqueId  = r.readU32();
        node.parentId  = r.readU32();
        node.name      = r.readString();
        node.className = r.readString();
        r.readVec3(node.position);
        node.hasQuaternion = r.readU8() != 0;
        if (node.hasQuaternion) r.readVec4(node.rotation);
        else { node.rotation[0] = r.readF32(); node.rotation[1] = r.readF32(); node.rotation[2] = r.readF32(); node.rotation[3] = 0.0f; }
        r.readVec3(node.scaling);
        node.isEnabled = r.readU8() != 0;
        sd.nodes.push_back(std::move(node));
    }

    // --- Meshes ---
    sd.meshes.reserve(numMeshes);
    for (uint32_t i = 0; i < numMeshes && r.valid(); i++)
    {
        MeshInfo mesh{};
        mesh.uniqueId  = r.readU32();
        mesh.parentId  = r.readU32();
        mesh.name      = r.readString();
        mesh.className = r.readString();
        r.readVec3(mesh.position);
        mesh.hasQuaternion = r.readU8() != 0;
        if (mesh.hasQuaternion) r.readVec4(mesh.rotation);
        else { mesh.rotation[0] = r.readF32(); mesh.rotation[1] = r.readF32(); mesh.rotation[2] = r.readF32(); mesh.rotation[3] = 0.0f; }
        r.readVec3(mesh.scaling);
        mesh.isEnabled      = r.readU8() != 0;
        mesh.isVisible       = r.readU8() != 0;
        mesh.visibility      = r.readF32();
        mesh.receiveShadows  = r.readU8() != 0;
        mesh.layerMask       = r.readU32();
        mesh.totalVertices   = r.readU32();
        mesh.totalIndices    = r.readU32();
        mesh.hasPositions    = r.readU8() != 0;
        mesh.hasNormals      = r.readU8() != 0;
        mesh.hasTangents     = r.readU8() != 0;
        mesh.hasUVs          = r.readU8() != 0;
        mesh.hasUV2s         = r.readU8() != 0;
        mesh.hasColors       = r.readU8() != 0;
        mesh.hasBoundingBox  = r.readU8() != 0;
        r.readVec3(mesh.bbMin);
        r.readVec3(mesh.bbMax);
        mesh.materialUniqueId = r.readI32();

        uint8_t numMorphs = r.readU8();
        mesh.morphTargets.reserve(numMorphs);
        for (uint8_t j = 0; j < numMorphs && r.valid(); j++)
        {
            MorphTarget mt;
            mt.name      = r.readString();
            mt.influence = r.readF32();
            mesh.morphTargets.push_back(std::move(mt));
        }
        sd.meshes.push_back(std::move(mesh));
    }

    // --- Materials ---
    sd.materials.reserve(numMaterials);
    for (uint32_t i = 0; i < numMaterials && r.valid(); i++)
    {
        MaterialInfo mat{};
        mat.uniqueId         = r.readU32();
        mat.name             = r.readString();
        mat.typeName         = r.readString();
        mat.alpha            = r.readF32();
        mat.backFaceCulling  = r.readU8() != 0;
        mat.wireframe        = r.readU8() != 0;
        mat.pointsCloud      = r.readU8() != 0;
        mat.disableDepthWrite = r.readU8() != 0;
        mat.forceDepthWrite  = r.readU8() != 0;
        mat.zOffset          = r.readF32();
        mat.zOffsetUnits     = r.readF32();
        mat.transparencyMode = r.readI32();
        mat.alphaCutOff      = r.readF32();
        r.readVec3(mat.diffuseColor);
        r.readVec3(mat.specularColor);
        r.readVec3(mat.emissiveColor);
        r.readVec3(mat.ambientColor);
        mat.isPBR = r.readU8() != 0;
        if (mat.isPBR)
        {
            mat.metallic             = r.readF32();
            mat.roughness            = r.readF32();
            mat.environmentIntensity = r.readF32();
        }

        uint8_t numSlots = r.readU8();
        mat.textureSlots.reserve(numSlots);
        for (uint8_t j = 0; j < numSlots && r.valid(); j++)
        {
            TextureSlot slot;
            slot.slotName        = r.readString();
            slot.textureName     = r.readString();
            slot.textureUniqueId = r.readI32();
            mat.textureSlots.push_back(std::move(slot));
        }
        sd.materials.push_back(std::move(mat));
    }

    // --- Textures ---
    sd.textures.reserve(numTextures);
    for (uint32_t i = 0; i < numTextures && r.valid(); i++)
    {
        TextureInfo tex{};
        tex.uniqueId       = r.readU32();
        tex.name           = r.readString();
        tex.className      = r.readString();
        tex.url            = r.readString();
        tex.width          = r.readI32();
        tex.height         = r.readI32();
        tex.hasAlpha       = r.readU8() != 0;
        tex.isCube         = r.readU8() != 0;
        tex.gammaSpace     = r.readU8() != 0;
        tex.coordinatesMode = r.readU8();
        tex.samplingMode   = r.readU8();
        tex.uOffset        = r.readF32();
        tex.vOffset        = r.readF32();
        tex.uScale         = r.readF32();
        tex.vScale         = r.readF32();
        tex.level          = r.readF32();
        sd.textures.push_back(std::move(tex));
    }

    // --- Lights ---
    sd.lights.reserve(numLights);
    for (uint32_t i = 0; i < numLights && r.valid(); i++)
    {
        LightInfo light{};
        light.uniqueId  = r.readU32();
        light.parentId  = r.readU32();
        light.name      = r.readString();
        light.className = r.readString();
        light.type      = r.readU8();
        r.readVec3(light.position);
        r.readVec3(light.direction);
        light.intensity = r.readF32();
        r.readVec3(light.diffuse);
        r.readVec3(light.specular);
        light.isEnabled = r.readU8() != 0;
        light.range     = r.readF32();
        light.isSpot    = r.readU8() != 0;
        if (light.isSpot)
        {
            light.spotAngle  = r.readF32();
            light.innerAngle = r.readF32();
            light.exponent   = r.readF32();
        }
        light.hasShadowGen = r.readU8() != 0;
        if (light.hasShadowGen)
        {
            light.shadowCasterCount = r.readF32();
            light.shadowBias        = r.readF32();
            light.shadowNormalBias  = r.readF32();
        }
        sd.lights.push_back(std::move(light));
    }

    // --- Animation Groups ---
    sd.animGroups.reserve(numAnimGroups);
    for (uint32_t i = 0; i < numAnimGroups && r.valid(); i++)
    {
        AnimGroupInfo ag{};
        ag.index         = r.readU32();
        ag.name          = r.readString();
        ag.isPlaying     = r.readU8() != 0;
        ag.speedRatio    = r.readF32();
        ag.from          = r.readF32();
        ag.to            = r.readF32();
        ag.loopAnimation = r.readU8() != 0;
        ag.currentFrame  = r.readF32();
        sd.animGroups.push_back(std::move(ag));
    }

    // --- Skeletons ---
    sd.skeletons.reserve(numSkeletons);
    for (uint32_t i = 0; i < numSkeletons && r.valid(); i++)
    {
        SkeletonInfo sk{};
        sk.index             = r.readU32();
        sk.name              = r.readString();
        sk.boneCount         = r.readU32();
        sk.useTextureForBones = r.readU8() != 0;
        sd.skeletons.push_back(std::move(sk));
    }

    sd.valid = r.valid();
    return sd;
}

// ===========================================================================
// Inspector UI State
// ===========================================================================
enum class EntityType : uint8_t { None, Scene, Camera, Node, Mesh, Material, Texture, Light, AnimGroup, Skeleton };

struct SelectedEntity
{
    EntityType type = EntityType::None;
    uint32_t   uniqueId = 0;
    size_t     index = 0;
};

struct InspectorState
{
    SelectedEntity selected;
    int activeTab = 0; // 0=Explorer, 1=Properties, 2=Stats, 3=Debug
    char searchBuf[128] = "";
    bool autoSwitchToProps = true;

    // Debug toggles (local UI state)
    bool debugGrid = false;
    bool debugNames = false;
    bool debugPhysics = false;
    bool debugBBox = false;
    bool debugAxes = false;
    bool debugAnimations = true;
    bool debugParticles = true;
    bool debugFog = true;
    bool debugShadows = true;
    bool debugLights = true;
    bool debugPostProc = true;
    bool debugSkeletons = true;
    bool texChannels[9] = {true, true, true, true, true, true, true, true, true};

    // FPS graph
    float fpsHistory[120] = {};
    int fpsHistoryIdx = 0;
};

// Global state
static InspectorState s_state;
static BinaryWriter s_cmdWriter;
static uint32_t s_cmdCount = 0;

// ===========================================================================
// Command helpers
// ===========================================================================
inline void BeginCommands()
{
    s_cmdWriter.clear();
    s_cmdCount = 0;
    s_cmdWriter.writeU32(CMD_MAGIC);
    s_cmdWriter.writeU32(CMD_VERSION);
    s_cmdWriter.writePlaceholderU32(); // command count placeholder
}

inline void EndCommands()
{
    if (s_cmdCount > 0)
        s_cmdWriter.patchU32(8, s_cmdCount);
}

inline bool HasPendingCommands()
{
    return s_cmdCount > 0;
}

inline void EmitCmd(InspectorCmd cmd, uint32_t uid)
{
    s_cmdWriter.writeU8(static_cast<uint8_t>(cmd));
    s_cmdWriter.writeU32(uid);
    s_cmdCount++;
}

inline void EmitCmdBool(InspectorCmd cmd, uint32_t uid, bool val)
{
    EmitCmd(cmd, uid);
    s_cmdWriter.writeU8(val ? 1 : 0);
}

inline void EmitCmdFloat(InspectorCmd cmd, uint32_t uid, float val)
{
    EmitCmd(cmd, uid);
    s_cmdWriter.writeF32(val);
}

inline void EmitCmdVec3(InspectorCmd cmd, uint32_t uid, const float v[3])
{
    EmitCmd(cmd, uid);
    s_cmdWriter.writeVec3(v);
}

inline void EmitCmdColor3(InspectorCmd cmd, uint32_t uid, const float c[3])
{
    EmitCmd(cmd, uid);
    s_cmdWriter.writeColor3(c);
}

inline void EmitCmdColor4(InspectorCmd cmd, uint32_t uid, const float c[4])
{
    EmitCmd(cmd, uid);
    s_cmdWriter.writeColor4(c);
}

inline void EmitCmdInt(InspectorCmd cmd, uint32_t uid, int32_t val)
{
    EmitCmd(cmd, uid);
    s_cmdWriter.writeI32(val);
}

// ===========================================================================
// Name helpers
// ===========================================================================
inline const char* CameraTypeName(uint8_t type)
{
    switch (type)
    {
    case 0:  return "Free/Universal";
    case 1:  return "ArcRotate";
    case 2:  return "Follow";
    default: return "Camera";
    }
}

inline const char* LightTypeName(uint8_t type)
{
    switch (type)
    {
    case 0:  return "Point";
    case 1:  return "Directional";
    case 2:  return "Spot";
    case 3:  return "Hemispheric";
    default: return "Light";
    }
}

inline const char* FogModeName(uint8_t mode)
{
    switch (mode)
    {
    case 0:  return "None";
    case 1:  return "Linear";
    case 2:  return "Exponential";
    case 3:  return "Exponential2";
    default: return "Unknown";
    }
}

inline const char* TransparencyModeName(int32_t mode)
{
    switch (mode)
    {
    case -1: return "Engine Default";
    case 0:  return "Opaque";
    case 1:  return "Alpha Test";
    case 2:  return "Alpha Blend";
    case 3:  return "Alpha Test + Blend";
    default: return "Unknown";
    }
}

inline const char* CoordinatesModeName(uint8_t mode)
{
    const char* names[] = {"Explicit", "Spherical", "Planar", "Cubic",
                           "Projection", "Skybox", "InvCubic", "Equirect", "FixedEquirect"};
    return mode < 9 ? names[mode] : "Unknown";
}

// ===========================================================================
// UI Widget helpers
// ===========================================================================
inline void AttributeBadge(const char* name, bool has)
{
    if (has)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", name);
    else
        ImGui::TextDisabled("%s", name);
    ImGui::SameLine();
}

inline bool CaseInsensitiveContains(const std::string& haystack, const char* needle)
{
    if (!needle || needle[0] == '\0') return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// Editable DragFloat3 that emits a command on change
inline bool EditVec3(const char* label, float v[3], uint32_t uid, InspectorCmd cmd, float speed = 0.05f)
{
    char id[64];
    snprintf(id, sizeof(id), "###%s_%u", label, uid);
    ImGui::Text("%s", label);
    ImGui::SameLine(100);
    ImGui::PushItemWidth(-1);
    if (ImGui::DragFloat3(id, v, speed, 0, 0, "%.3f"))
    {
        EmitCmdVec3(cmd, uid, v);
        ImGui::PopItemWidth();
        return true;
    }
    ImGui::PopItemWidth();
    return false;
}

inline bool EditFloat(const char* label, float* v, uint32_t uid, InspectorCmd cmd,
                       float speed = 0.01f, float mn = 0, float mx = 0, const char* fmt = "%.3f")
{
    char id[64];
    snprintf(id, sizeof(id), "###%s_%u", label, uid);
    ImGui::Text("%s", label);
    ImGui::SameLine(100);
    ImGui::PushItemWidth(-1);
    if (ImGui::DragFloat(id, v, speed, mn, mx, fmt))
    {
        EmitCmdFloat(cmd, uid, *v);
        ImGui::PopItemWidth();
        return true;
    }
    ImGui::PopItemWidth();
    return false;
}

inline bool EditSlider(const char* label, float* v, uint32_t uid, InspectorCmd cmd,
                        float mn, float mx, const char* fmt = "%.3f")
{
    char id[64];
    snprintf(id, sizeof(id), "###%s_%u", label, uid);
    ImGui::Text("%s", label);
    ImGui::SameLine(100);
    ImGui::PushItemWidth(-1);
    if (ImGui::SliderFloat(id, v, mn, mx, fmt))
    {
        EmitCmdFloat(cmd, uid, *v);
        ImGui::PopItemWidth();
        return true;
    }
    ImGui::PopItemWidth();
    return false;
}

inline bool EditColor3(const char* label, float c[3], uint32_t uid, InspectorCmd cmd)
{
    char id[64];
    snprintf(id, sizeof(id), "###%s_%u", label, uid);
    ImGui::Text("%s", label);
    ImGui::SameLine(100);
    ImGui::PushItemWidth(-1);
    if (ImGui::ColorEdit3(id, c, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
    {
        EmitCmdColor3(cmd, uid, c);
        ImGui::PopItemWidth();
        return true;
    }
    ImGui::PopItemWidth();
    return false;
}

inline bool EditColor4(const char* label, float c[4], uint32_t uid, InspectorCmd cmd)
{
    char id[64];
    snprintf(id, sizeof(id), "###%s_%u", label, uid);
    ImGui::Text("%s", label);
    ImGui::SameLine(100);
    ImGui::PushItemWidth(-1);
    if (ImGui::ColorEdit4(id, c, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
    {
        EmitCmdColor4(cmd, uid, c);
        ImGui::PopItemWidth();
        return true;
    }
    ImGui::PopItemWidth();
    return false;
}

inline bool EditBool(const char* label, bool* v, uint32_t uid, InspectorCmd cmd)
{
    char id[64];
    snprintf(id, sizeof(id), "###%s_%u", label, uid);
    if (ImGui::Checkbox(id, v))
    {
        EmitCmdBool(cmd, uid, *v);
        return true;
    }
    ImGui::SameLine();
    ImGui::Text("%s", label);
    return false;
}

// ===========================================================================
// Selection helpers
// ===========================================================================
inline void SelectEntity(EntityType type, uint32_t uid, size_t idx)
{
    s_state.selected = {type, uid, idx};
    if (s_state.autoSwitchToProps)
        s_state.activeTab = 1;
}

inline bool IsSelected(EntityType type, uint32_t uid)
{
    return s_state.selected.type == type && s_state.selected.uniqueId == uid;
}

// Flat list item (for Materials, Textures, AnimGroups, Skeletons)
inline void RenderTreeItem(const char* label, EntityType type, uint32_t uid, size_t idx)
{
    ImGui::PushID(static_cast<int>(uid));
    bool selected = IsSelected(type, uid);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::TreeNodeEx(label, flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        SelectEntity(type, uid, idx);
    ImGui::PopID();
}

// ===========================================================================
// Scene Graph Tree — build unified hierarchy from all entity types
// ===========================================================================
struct SceneGraphNode
{
    EntityType type;
    uint32_t uniqueId;
    uint32_t parentId;
    std::string name;
    std::string typeName;
    std::vector<size_t> children;
};

struct SceneGraph
{
    std::vector<SceneGraphNode> nodes;
    std::vector<size_t> roots;
    std::unordered_map<uint32_t, size_t> idMap;
};

inline SceneGraph BuildSceneGraph(const SceneData& data)
{
    SceneGraph g;

    auto addNode = [&](EntityType type, uint32_t uid, uint32_t pid, const std::string& name, const std::string& typeName) {
        if (uid == 0 || g.idMap.count(uid)) return;
        size_t idx = g.nodes.size();
        g.nodes.push_back({type, uid, pid, name, typeName, {}});
        g.idMap[uid] = idx;
    };

    for (auto& cam : data.cameras)
        addNode(EntityType::Camera, cam.uniqueId, cam.parentId, cam.name, CameraTypeName(cam.type));
    for (auto& node : data.nodes)
        addNode(EntityType::Node, node.uniqueId, node.parentId, node.name, node.className);
    for (auto& mesh : data.meshes)
        addNode(EntityType::Mesh, mesh.uniqueId, mesh.parentId, mesh.name, mesh.className);
    for (auto& light : data.lights)
        addNode(EntityType::Light, light.uniqueId, light.parentId, light.name, LightTypeName(light.type));

    for (size_t i = 0; i < g.nodes.size(); i++)
    {
        auto& n = g.nodes[i];
        auto pit = g.idMap.find(n.parentId);
        if (n.parentId == 0 || pit == g.idMap.end())
            g.roots.push_back(i);
        else
            g.nodes[pit->second].children.push_back(i);
    }
    return g;
}

// Check if node or any descendant matches the search filter
inline bool HasFilterMatch(const SceneGraph& g, size_t idx, const char* filter)
{
    if (CaseInsensitiveContains(g.nodes[idx].name, filter)) return true;
    for (size_t ci : g.nodes[idx].children)
        if (HasFilterMatch(g, ci, filter)) return true;
    return false;
}

inline void RenderGraphNode(const SceneGraph& g, size_t idx, const char* filter, SceneData& data)
{
    auto& node = g.nodes[idx];

    if (filter[0] != '\0' && !HasFilterMatch(g, idx, filter))
        return;

    bool hasChildren = !node.children.empty();
    bool selected = IsSelected(node.type, node.uniqueId);

    const char* typeTag = "";
    switch (node.type) {
    case EntityType::Camera: typeTag = "[Cam] "; break;
    case EntityType::Light:  typeTag = "[Light] "; break;
    case EntityType::Mesh:   typeTag = "[Mesh] "; break;
    case EntityType::Node:   typeTag = "[Node] "; break;
    default: break;
    }

    char label[256];
    snprintf(label, sizeof(label), "%s%s  %s###sg_%u", typeTag, node.name.c_str(), node.typeName.c_str(), node.uniqueId);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (filter[0] != '\0') flags |= ImGuiTreeNodeFlags_DefaultOpen;

    bool open = ImGui::TreeNodeEx(label, flags);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        SelectEntity(node.type, node.uniqueId, 0);
        EmitCmd(CMD_SELECT_ENTITY, node.uniqueId);
    }

    if (open && hasChildren)
    {
        for (size_t ci : node.children)
            RenderGraphNode(g, ci, filter, data);
        ImGui::TreePop();
    }
}

// ===========================================================================
// Scene Explorer — hierarchical scene graph + resource lists
// ===========================================================================
inline void RenderSceneExplorer(SceneData& data)
{
    const char* filter = s_state.searchBuf;

    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("###search", "Search...", s_state.searchBuf, sizeof(s_state.searchBuf));
    ImGui::PopItemWidth();
    ImGui::Separator();

    // Scene root
    {
        bool sceneSelected = IsSelected(EntityType::Scene, 0);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (sceneSelected) flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::TreeNodeEx("Scene", flags);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            SelectEntity(EntityType::Scene, 0, 0);
            EmitCmd(CMD_SELECT_ENTITY, 0); // clear highlight
        }
    }

    // Build and render the hierarchical scene graph
    SceneGraph graph = BuildSceneGraph(data);

    char hdr[128];
    snprintf(hdr, sizeof(hdr), "Scene Graph (%zu)###sg_sec", graph.nodes.size());
    if (ImGui::TreeNodeEx(hdr, ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t ri : graph.roots)
            RenderGraphNode(graph, ri, filter, data);
        ImGui::TreePop();
    }

    // --- Resource lists (not part of node hierarchy) ---

    // Materials
    snprintf(hdr, sizeof(hdr), "Materials (%zu)###mat_sec", data.materials.size());
    if (ImGui::TreeNode(hdr))
    {
        for (size_t i = 0; i < data.materials.size(); i++)
        {
            auto& mat = data.materials[i];
            if (!CaseInsensitiveContains(mat.name, filter)) continue;
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###mt%u",
                mat.name.c_str(), mat.typeName.c_str(), mat.uniqueId);
            RenderTreeItem(label, EntityType::Material, mat.uniqueId, i);
        }
        ImGui::TreePop();
    }

    // Textures
    snprintf(hdr, sizeof(hdr), "Textures (%zu)###tex_sec", data.textures.size());
    if (ImGui::TreeNode(hdr))
    {
        for (size_t i = 0; i < data.textures.size(); i++)
        {
            auto& tex = data.textures[i];
            if (!CaseInsensitiveContains(tex.name, filter)) continue;
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]###tx%u",
                tex.name.c_str(), tex.className.c_str(), tex.uniqueId);
            RenderTreeItem(label, EntityType::Texture, tex.uniqueId, i);
        }
        ImGui::TreePop();
    }

    // Animation Groups
    if (!data.animGroups.empty())
    {
        snprintf(hdr, sizeof(hdr), "Animations (%zu)###anim_sec", data.animGroups.size());
        if (ImGui::TreeNode(hdr))
        {
            for (size_t i = 0; i < data.animGroups.size(); i++)
            {
                auto& ag = data.animGroups[i];
                if (!CaseInsensitiveContains(ag.name, filter)) continue;
                char label[256];
                snprintf(label, sizeof(label), "%s %s###ag%u",
                    ag.isPlaying ? ">" : " ", ag.name.c_str(), ag.index);
                RenderTreeItem(label, EntityType::AnimGroup, ag.index, i);
            }
            ImGui::TreePop();
        }
    }

    // Skeletons
    if (!data.skeletons.empty())
    {
        snprintf(hdr, sizeof(hdr), "Skeletons (%zu)###skel_sec", data.skeletons.size());
        if (ImGui::TreeNode(hdr))
        {
            for (size_t i = 0; i < data.skeletons.size(); i++)
            {
                auto& sk = data.skeletons[i];
                if (!CaseInsensitiveContains(sk.name, filter)) continue;
                char label[256];
                snprintf(label, sizeof(label), "%s (%u bones)###sk%u",
                    sk.name.c_str(), sk.boneCount, sk.index);
                RenderTreeItem(label, EntityType::Skeleton, sk.index, i);
            }
            ImGui::TreePop();
        }
    }
}

// ===========================================================================
// Properties panel rendering
// ===========================================================================
inline void RenderSceneProps(SceneData& data)
{
    auto& sp = data.scene;

    if (ImGui::CollapsingHeader("General###sp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditColor4("Clear Color", sp.clearColor, 0, CMD_SET_CLEAR_COLOR);
        EditColor3("Ambient Color", sp.ambientColor, 0, CMD_SET_SCENE_AMBIENT);
        EditBool("Auto Clear", &sp.autoClear, 0, CMD_SET_AUTO_CLEAR);
        EditBool("Force Wireframe", &sp.forceWireframe, 0, CMD_SET_FORCE_WIREFRAME);
        EditBool("Force Points Cloud", &sp.forcePointsCloud, 0, CMD_SET_FORCE_POINTS_CLOUD);
    }

    if (ImGui::CollapsingHeader("Fog###sp_fog"))
    {
        ImGui::Text("Fog Mode");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        int fogMode = sp.fogMode;
        const char* fogModes[] = {"None", "Linear", "Exponential", "Exponential2"};
        if (ImGui::Combo("###fogmode", &fogMode, fogModes, 4))
        {
            sp.fogMode = static_cast<uint8_t>(fogMode);
            EmitCmdInt(CMD_SET_FOG_MODE, 0, fogMode);
        }
        ImGui::PopItemWidth();
        if (sp.fogMode > 0)
        {
            EditColor3("Fog Color", sp.fogColor, 0, CMD_SET_FOG_COLOR);
            if (sp.fogMode == 1)
            {
                EditFloat("Fog Start", &sp.fogStart, 0, CMD_SET_FOG_START);
                EditFloat("Fog End", &sp.fogEnd, 0, CMD_SET_FOG_END);
            }
            else
            {
                EditFloat("Fog Density", &sp.fogDensity, 0, CMD_SET_FOG_DENSITY, 0.001f);
            }
        }
    }

    if (sp.hasImageProcessing && ImGui::CollapsingHeader("Image Processing###sp_ip"))
    {
        EditSlider("Contrast", &sp.contrast, 0, CMD_SET_CONTRAST, 0, 4);
        EditSlider("Exposure", &sp.exposure, 0, CMD_SET_EXPOSURE, 0, 4);

        ImGui::Text("Tone Mapping");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        int tmType = sp.toneMappingType;
        const char* tmTypes[] = {"Standard", "ACES", "KHR PBR Neutral"};
        if (ImGui::Combo("###tm_type", &tmType, tmTypes, 3))
        {
            sp.toneMappingType = static_cast<uint8_t>(tmType);
            EmitCmdInt(CMD_SET_TONE_MAPPING_TYPE, 0, tmType);
        }
        ImGui::PopItemWidth();
    }

    if (ImGui::CollapsingHeader("Physics###sp_phys"))
    {
        EditVec3("Gravity", sp.gravity, 0, CMD_SET_GRAVITY);
    }
}

inline void RenderCameraProps(CameraInfo& cam)
{
    uint32_t uid = cam.uniqueId;

    if (ImGui::CollapsingHeader("General###cp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", cam.name.c_str());
        ImGui::Text("Type: %s", cam.className.c_str());
        ImGui::Text("ID: %u", uid);
        if (cam.isActive)
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Active Camera");
    }

    if (ImGui::CollapsingHeader("Transform###cp_xform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditVec3("Position", cam.position, uid, CMD_SET_POSITION);
        EditVec3("Target", cam.target, uid, CMD_SET_TARGET);
    }

    if (ImGui::CollapsingHeader("Camera###cp_cam", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float fovDeg = cam.fov * RAD2DEG;
        if (EditFloat("FOV (deg)", &fovDeg, uid, CMD_SET_FOV, 0.5f, 1, 179))
            cam.fov = fovDeg * DEG2RAD;
        // Emit actual radian value
        if (ImGui::IsItemDeactivatedAfterEdit())
            EmitCmdFloat(CMD_SET_FOV, uid, cam.fov);

        EditFloat("Near Clip", &cam.minZ, uid, CMD_SET_MIN_Z, 0.01f);
        EditFloat("Far Clip", &cam.maxZ, uid, CMD_SET_MAX_Z, 1.0f);
        EditSlider("Inertia", &cam.inertia, uid, CMD_SET_INERTIA, 0, 1);
        EditFloat("Speed", &cam.speed, uid, CMD_SET_SPEED, 0.1f);
    }

    if (cam.isArcRotate && ImGui::CollapsingHeader("Arc Rotate###cp_arc", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float alphaDeg = cam.alpha * RAD2DEG;
        float betaDeg  = cam.beta * RAD2DEG;
        ImGui::Text("Alpha (deg)");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        char aid[32]; snprintf(aid, sizeof(aid), "###arc_a_%u", uid);
        if (ImGui::DragFloat(aid, &alphaDeg, 0.5f))
        {
            cam.alpha = alphaDeg * DEG2RAD;
            EmitCmdFloat(CMD_SET_ARC_ALPHA, uid, cam.alpha);
        }
        ImGui::PopItemWidth();

        ImGui::Text("Beta (deg)");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        char bid[32]; snprintf(bid, sizeof(bid), "###arc_b_%u", uid);
        if (ImGui::DragFloat(bid, &betaDeg, 0.5f))
        {
            cam.beta = betaDeg * DEG2RAD;
            EmitCmdFloat(CMD_SET_ARC_BETA, uid, cam.beta);
        }
        ImGui::PopItemWidth();

        EditFloat("Radius", &cam.radius, uid, CMD_SET_ARC_RADIUS, 0.1f);
    }
}

inline void RenderTransformProps(float pos[3], float rot[4], float scl[3], bool hasQuat, uint32_t uid)
{
    if (ImGui::CollapsingHeader("Transform###xf", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditVec3("Position", pos, uid, CMD_SET_POSITION);

        // Show euler rotation even for quaternion (convert for display)
        float euler[3];
        if (hasQuat)
        {
            // Quaternion to euler (approximate)
            float qx = rot[0], qy = rot[1], qz = rot[2], qw = rot[3];
            float sinr = 2.0f * (qw * qx + qy * qz);
            float cosr = 1.0f - 2.0f * (qx * qx + qy * qy);
            euler[0] = std::atan2(sinr, cosr) * RAD2DEG;
            float sinp = 2.0f * (qw * qy - qz * qx);
            euler[1] = (std::fabs(sinp) >= 1.0f ? std::copysign(90.0f, sinp) : std::asin(sinp) * RAD2DEG);
            float siny = 2.0f * (qw * qz + qx * qy);
            float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
            euler[2] = std::atan2(siny, cosy) * RAD2DEG;
        }
        else
        {
            euler[0] = rot[0] * RAD2DEG;
            euler[1] = rot[1] * RAD2DEG;
            euler[2] = rot[2] * RAD2DEG;
        }

        char rid[64]; snprintf(rid, sizeof(rid), "###rot_%u", uid);
        ImGui::Text("Rotation");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        if (ImGui::DragFloat3(rid, euler, 0.5f, 0, 0, "%.1f deg"))
        {
            float rad[3] = {euler[0] * DEG2RAD, euler[1] * DEG2RAD, euler[2] * DEG2RAD};
            EmitCmdVec3(CMD_SET_ROTATION, uid, rad);
        }
        ImGui::PopItemWidth();

        EditVec3("Scaling", scl, uid, CMD_SET_SCALING);
    }
}

inline void RenderNodeProps(NodeInfo& node)
{
    uint32_t uid = node.uniqueId;
    if (ImGui::CollapsingHeader("General###np_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", node.name.c_str());
        ImGui::Text("Type: %s", node.className.c_str());
        ImGui::Text("ID: %u", uid);
        EditBool("Enabled", &node.isEnabled, uid, CMD_SET_ENABLED);
    }
    RenderTransformProps(node.position, node.rotation, node.scaling, node.hasQuaternion, uid);
}

inline void RenderMeshProps(MeshInfo& mesh, const SceneData& data)
{
    uint32_t uid = mesh.uniqueId;

    if (ImGui::CollapsingHeader("General###mp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", mesh.name.c_str());
        ImGui::Text("Type: %s", mesh.className.c_str());
        ImGui::Text("ID: %u", uid);
        EditBool("Enabled", &mesh.isEnabled, uid, CMD_SET_ENABLED);
        EditBool("Visible", &mesh.isVisible, uid, CMD_SET_VISIBLE);
    }

    RenderTransformProps(mesh.position, mesh.rotation, mesh.scaling, mesh.hasQuaternion, uid);

    if (ImGui::CollapsingHeader("Display###mp_disp", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditSlider("Visibility", &mesh.visibility, uid, CMD_SET_VISIBILITY, 0, 1);
        EditBool("Receive Shadows", &mesh.receiveShadows, uid, CMD_SET_RECEIVE_SHADOWS);

        char lmid[32]; snprintf(lmid, sizeof(lmid), "###lm_%u", uid);
        ImGui::Text("Layer Mask");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        int lm = static_cast<int>(mesh.layerMask);
        if (ImGui::InputInt(lmid, &lm, 0, 0, ImGuiInputTextFlags_CharsHexadecimal))
        {
            mesh.layerMask = static_cast<uint32_t>(lm);
            EmitCmd(CMD_SET_LAYER_MASK, uid);
            s_cmdWriter.writeU32(mesh.layerMask);
        }
        ImGui::PopItemWidth();
    }

    if (ImGui::CollapsingHeader("Mesh Info###mp_info"))
    {
        ImGui::Text("Vertices: %u  |  Indices: %u", mesh.totalVertices, mesh.totalIndices);
        ImGui::Text("Faces: %u", mesh.totalIndices / 3);

        ImGui::Text("Attributes: ");
        ImGui::SameLine();
        AttributeBadge("Pos", mesh.hasPositions);
        AttributeBadge("Nrm", mesh.hasNormals);
        AttributeBadge("Tan", mesh.hasTangents);
        AttributeBadge("UV",  mesh.hasUVs);
        AttributeBadge("UV2", mesh.hasUV2s);
        AttributeBadge("Col", mesh.hasColors);
        ImGui::NewLine();

        if (mesh.hasBoundingBox)
        {
            ImGui::Text("BBox Min: (%.2f, %.2f, %.2f)", mesh.bbMin[0], mesh.bbMin[1], mesh.bbMin[2]);
            ImGui::Text("BBox Max: (%.2f, %.2f, %.2f)", mesh.bbMax[0], mesh.bbMax[1], mesh.bbMax[2]);
        }

        // Material reference
        if (mesh.materialUniqueId >= 0)
        {
            for (const auto& mat : data.materials)
            {
                if (static_cast<int32_t>(mat.uniqueId) == mesh.materialUniqueId)
                {
                    ImGui::Text("Material:");
                    ImGui::SameLine();
                    char matBtn[128];
                    snprintf(matBtn, sizeof(matBtn), "%s###matlink_%u", mat.name.c_str(), uid);
                    if (ImGui::SmallButton(matBtn))
                    {
                        // Find material index
                        for (size_t mi = 0; mi < data.materials.size(); mi++)
                        {
                            if (data.materials[mi].uniqueId == mat.uniqueId)
                            {
                                SelectEntity(EntityType::Material, mat.uniqueId, mi);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
        else
        {
            ImGui::TextDisabled("Material: (none)");
        }
    }

    // Morph targets
    if (!mesh.morphTargets.empty() && ImGui::CollapsingHeader("Morph Targets###mp_morph"))
    {
        for (size_t i = 0; i < mesh.morphTargets.size(); i++)
        {
            auto& mt = mesh.morphTargets[i];
            char mtid[64]; snprintf(mtid, sizeof(mtid), "###morph_%zu_%u", i, uid);
            ImGui::Text("%s", mt.name.c_str());
            ImGui::SameLine(100);
            ImGui::PushItemWidth(-1);
            if (ImGui::SliderFloat(mtid, &mt.influence, 0, 1, "%.3f"))
            {
                EmitCmd(CMD_SET_MORPH_INFLUENCE, uid);
                s_cmdWriter.writeU8(static_cast<uint8_t>(i));
                s_cmdWriter.writeF32(mt.influence);
            }
            ImGui::PopItemWidth();
        }
    }
}

inline void RenderLightProps(LightInfo& light)
{
    uint32_t uid = light.uniqueId;

    if (ImGui::CollapsingHeader("General###lp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", light.name.c_str());
        ImGui::Text("Type: %s (%s)", light.className.c_str(), LightTypeName(light.type));
        ImGui::Text("ID: %u", uid);
        EditBool("Enabled", &light.isEnabled, uid, CMD_SET_ENABLED);
    }

    if (ImGui::CollapsingHeader("Properties###lp_props", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditFloat("Intensity", &light.intensity, uid, CMD_SET_INTENSITY, 0.01f);
        EditColor3("Diffuse", light.diffuse, uid, CMD_SET_LIGHT_DIFFUSE);
        EditColor3("Specular", light.specular, uid, CMD_SET_LIGHT_SPECULAR);

        if (light.type == 0 || light.type == 2) // point or spot
        {
            EditVec3("Position", light.position, uid, CMD_SET_POSITION);
            EditFloat("Range", &light.range, uid, CMD_SET_RANGE, 0.1f);
        }

        if (light.type != 0) // directional, spot, hemispheric
        {
            EditVec3("Direction", light.direction, uid, CMD_SET_DIRECTION);
        }

        if (light.isSpot)
        {
            float angleDeg = light.spotAngle * RAD2DEG;
            float innerDeg = light.innerAngle * RAD2DEG;
            ImGui::Text("Spot Angle");
            ImGui::SameLine(100);
            ImGui::PushItemWidth(-1);
            char said[32]; snprintf(said, sizeof(said), "###sa_%u", uid);
            if (ImGui::SliderFloat(said, &angleDeg, 0, 180, "%.1f deg"))
            {
                light.spotAngle = angleDeg * DEG2RAD;
                EmitCmdFloat(CMD_SET_SPOT_ANGLE, uid, light.spotAngle);
            }
            ImGui::PopItemWidth();

            ImGui::Text("Inner Angle");
            ImGui::SameLine(100);
            ImGui::PushItemWidth(-1);
            char iaid[32]; snprintf(iaid, sizeof(iaid), "###ia_%u", uid);
            if (ImGui::SliderFloat(iaid, &innerDeg, 0, angleDeg, "%.1f deg"))
            {
                light.innerAngle = innerDeg * DEG2RAD;
                EmitCmdFloat(CMD_SET_INNER_ANGLE, uid, light.innerAngle);
            }
            ImGui::PopItemWidth();

            EditFloat("Exponent", &light.exponent, uid, CMD_SET_EXPONENT, 0.1f);
        }
    }

    if (light.hasShadowGen && ImGui::CollapsingHeader("Shadows###lp_shad"))
    {
        ImGui::Text("Shadow Casters: %.0f", light.shadowCasterCount);
        EditFloat("Bias", &light.shadowBias, uid, CMD_SET_SHADOW_BIAS, 0.00001f, 0, 0, "%.6f");
        EditFloat("Normal Bias", &light.shadowNormalBias, uid, CMD_SET_SHADOW_NORMAL_BIAS, 0.001f, 0, 0, "%.4f");
    }
}

inline void RenderMaterialProps(MaterialInfo& mat)
{
    uint32_t uid = mat.uniqueId;

    if (ImGui::CollapsingHeader("General###matp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", mat.name.c_str());
        ImGui::Text("Type: %s", mat.typeName.c_str());
        ImGui::Text("ID: %u", uid);
    }

    if (ImGui::CollapsingHeader("Properties###matp_props", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditSlider("Alpha", &mat.alpha, uid, CMD_SET_ALPHA, 0, 1);
        EditBool("Wireframe", &mat.wireframe, uid, CMD_SET_WIREFRAME);
        EditBool("Back-face Culling", &mat.backFaceCulling, uid, CMD_SET_BACK_FACE_CULLING);
        EditBool("Points Cloud", &mat.pointsCloud, uid, CMD_SET_POINTS_CLOUD_MAT);

        ImGui::Text("Transparency");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        int tm = mat.transparencyMode + 1; // shift -1..3 to 0..4
        const char* tmModes[] = {"Engine Default", "Opaque", "Alpha Test", "Alpha Blend", "Alpha Test+Blend"};
        char tmid[32]; snprintf(tmid, sizeof(tmid), "###tm_%u", uid);
        if (ImGui::Combo(tmid, &tm, tmModes, 5))
        {
            mat.transparencyMode = tm - 1;
            EmitCmdInt(CMD_SET_TRANSPARENCY_MODE, uid, mat.transparencyMode);
        }
        ImGui::PopItemWidth();

        if (mat.transparencyMode == 1 || mat.transparencyMode == 3)
            EditSlider("Alpha Cutoff", &mat.alphaCutOff, uid, CMD_SET_ALPHA_CUTOFF, 0, 1);
    }

    if (ImGui::CollapsingHeader("Colors###matp_col", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditColor3("Diffuse", mat.diffuseColor, uid, CMD_SET_DIFFUSE_COLOR);
        EditColor3("Specular", mat.specularColor, uid, CMD_SET_SPECULAR_COLOR);
        EditColor3("Emissive", mat.emissiveColor, uid, CMD_SET_EMISSIVE_COLOR);
        EditColor3("Ambient", mat.ambientColor, uid, CMD_SET_AMBIENT_COLOR);
    }

    if (mat.isPBR && ImGui::CollapsingHeader("PBR###matp_pbr", ImGuiTreeNodeFlags_DefaultOpen))
    {
        EditSlider("Metallic", &mat.metallic, uid, CMD_SET_METALLIC, 0, 1);
        EditSlider("Roughness", &mat.roughness, uid, CMD_SET_ROUGHNESS, 0, 1);
        EditFloat("Env Intensity", &mat.environmentIntensity, uid, CMD_SET_ENV_INTENSITY, 0.01f);
    }

    if (ImGui::CollapsingHeader("Depth###matp_depth"))
    {
        EditBool("Disable Depth Write", &mat.disableDepthWrite, uid, CMD_SET_DISABLE_DEPTH_WRITE);
        EditBool("Force Depth Write", &mat.forceDepthWrite, uid, CMD_SET_FORCE_DEPTH_WRITE);
        EditFloat("Z-Offset", &mat.zOffset, uid, CMD_SET_Z_OFFSET, 0.01f, -10, 10);
        EditFloat("Z-Offset Units", &mat.zOffsetUnits, uid, CMD_SET_Z_OFFSET_UNITS, 0.01f, -10, 10);
    }

    if (!mat.textureSlots.empty() && ImGui::CollapsingHeader("Textures###matp_tex"))
    {
        for (const auto& slot : mat.textureSlots)
        {
            ImGui::BulletText("%s: \"%s\"", slot.slotName.c_str(), slot.textureName.c_str());
        }
    }
}

inline void RenderTextureProps(TextureInfo& tex)
{
    uint32_t uid = tex.uniqueId;

    if (ImGui::CollapsingHeader("General###txp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", tex.name.c_str());
        ImGui::Text("Type: %s", tex.className.c_str());
        ImGui::Text("ID: %u", uid);
        if (!tex.url.empty())
            ImGui::TextWrapped("URL: %s", tex.url.c_str());
    }

    if (ImGui::CollapsingHeader("Characteristics###txp_char", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (tex.width > 0 && tex.height > 0)
            ImGui::Text("Size: %d x %d", tex.width, tex.height);
        else
            ImGui::TextDisabled("Size: unknown");

        ImGui::Text("Has Alpha: "); ImGui::SameLine();
        AttributeBadge(tex.hasAlpha ? "Yes" : "No", tex.hasAlpha); ImGui::NewLine();
        ImGui::Text("Is Cube: "); ImGui::SameLine();
        AttributeBadge(tex.isCube ? "Yes" : "No", tex.isCube); ImGui::NewLine();
        ImGui::Text("Gamma Space: %s", tex.gammaSpace ? "Yes" : "No");
        ImGui::Text("Coord Mode: %s", CoordinatesModeName(tex.coordinatesMode));
        ImGui::Text("Sampling: %u", tex.samplingMode);
    }

    if (ImGui::CollapsingHeader("Transform###txp_xf"))
    {
        EditFloat("Level", &tex.level, uid, CMD_SET_TEXTURE_LEVEL, 0.01f);
        ImGui::Text("Offset: (%.3f, %.3f)", tex.uOffset, tex.vOffset);
        ImGui::Text("Scale: (%.3f, %.3f)", tex.uScale, tex.vScale);
    }
}

inline void RenderAnimGroupProps(AnimGroupInfo& ag)
{
    uint32_t idx = ag.index;

    if (ImGui::CollapsingHeader("General###agp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", ag.name.c_str());
        ImGui::Text("Range: %.1f - %.1f", ag.from, ag.to);
        ImGui::Text("Status: %s", ag.isPlaying ? "Playing" : "Stopped");
    }

    if (ImGui::CollapsingHeader("Controls###agp_ctrl", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Play")) { EmitCmd(CMD_ANIM_PLAY, idx); }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) { EmitCmd(CMD_ANIM_PAUSE, idx); }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) { EmitCmd(CMD_ANIM_STOP, idx); }

        EditSlider("Speed Ratio", &ag.speedRatio, idx, CMD_SET_SPEED_RATIO, 0, 10);

        EditBool("Loop", &ag.loopAnimation, idx, CMD_SET_ANIM_LOOP);

        // Frame scrub
        ImGui::Text("Frame");
        ImGui::SameLine(100);
        ImGui::PushItemWidth(-1);
        char fid[32]; snprintf(fid, sizeof(fid), "###anim_frame_%u", idx);
        if (ImGui::SliderFloat(fid, &ag.currentFrame, ag.from, ag.to, "%.1f"))
        {
            EmitCmdFloat(CMD_SET_ANIM_FRAME, idx, ag.currentFrame);
        }
        ImGui::PopItemWidth();
    }
}

inline void RenderSkeletonProps(SkeletonInfo& sk)
{
    uint32_t idx = sk.index;

    if (ImGui::CollapsingHeader("General###skp_gen", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Name: %s", sk.name.c_str());
        ImGui::Text("Bones: %u", sk.boneCount);
        ImGui::Text("Texture Storage: %s", sk.useTextureForBones ? "Yes" : "No");

        if (ImGui::Button("Return to Rest"))
        {
            EmitCmd(CMD_SKELETON_RETURN_TO_REST, idx);
        }
    }
}

inline void RenderPropertiesPanel(SceneData& data)
{
    auto& sel = s_state.selected;

    if (sel.type == EntityType::None)
    {
        ImGui::TextDisabled("Select an entity in the Scene Explorer");
        return;
    }

    if (sel.type == EntityType::Scene)
    {
        RenderSceneProps(data);
        return;
    }

    if (sel.type == EntityType::Camera && sel.index < data.cameras.size())
    {
        auto& cam = data.cameras[sel.index];
        if (cam.uniqueId == sel.uniqueId) { RenderCameraProps(cam); return; }
    }
    // Search by uid if index is stale
    if (sel.type == EntityType::Camera)
    {
        for (size_t i = 0; i < data.cameras.size(); i++)
            if (data.cameras[i].uniqueId == sel.uniqueId) { sel.index = i; RenderCameraProps(data.cameras[i]); return; }
    }

    if (sel.type == EntityType::Node)
    {
        for (size_t i = 0; i < data.nodes.size(); i++)
            if (data.nodes[i].uniqueId == sel.uniqueId) { sel.index = i; RenderNodeProps(data.nodes[i]); return; }
    }

    if (sel.type == EntityType::Mesh)
    {
        for (size_t i = 0; i < data.meshes.size(); i++)
            if (data.meshes[i].uniqueId == sel.uniqueId) { sel.index = i; RenderMeshProps(data.meshes[i], data); return; }
    }

    if (sel.type == EntityType::Light)
    {
        for (size_t i = 0; i < data.lights.size(); i++)
            if (data.lights[i].uniqueId == sel.uniqueId) { sel.index = i; RenderLightProps(data.lights[i]); return; }
    }

    if (sel.type == EntityType::Material)
    {
        for (size_t i = 0; i < data.materials.size(); i++)
            if (data.materials[i].uniqueId == sel.uniqueId) { sel.index = i; RenderMaterialProps(data.materials[i]); return; }
    }

    if (sel.type == EntityType::Texture)
    {
        for (size_t i = 0; i < data.textures.size(); i++)
            if (data.textures[i].uniqueId == sel.uniqueId) { sel.index = i; RenderTextureProps(data.textures[i]); return; }
    }

    if (sel.type == EntityType::AnimGroup)
    {
        for (size_t i = 0; i < data.animGroups.size(); i++)
            if (data.animGroups[i].index == sel.uniqueId) { sel.index = i; RenderAnimGroupProps(data.animGroups[i]); return; }
    }

    if (sel.type == EntityType::Skeleton)
    {
        for (size_t i = 0; i < data.skeletons.size(); i++)
            if (data.skeletons[i].index == sel.uniqueId) { sel.index = i; RenderSkeletonProps(data.skeletons[i]); return; }
    }

    ImGui::TextDisabled("Entity no longer exists");
    sel.type = EntityType::None;
}

// ===========================================================================
// Stats panel
// ===========================================================================
inline void RenderStatsPanel(const SceneData& data)
{
    const auto& st = data.stats;

    // FPS graph
    auto& state = s_state;
    state.fpsHistory[state.fpsHistoryIdx] = st.fps;
    state.fpsHistoryIdx = (state.fpsHistoryIdx + 1) % 120;

    char fpsLabel[64];
    snprintf(fpsLabel, sizeof(fpsLabel), "%.1f FPS", st.fps);
    ImGui::PlotLines("###fps_graph", state.fpsHistory, 120, state.fpsHistoryIdx,
                     fpsLabel, 0, 120, ImVec2(-1, 60));
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Counts###st_counts", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Total Vertices:    %u", st.totalVertices);
        ImGui::Text("Active Meshes:     %u", st.activeMeshes);
        ImGui::Text("Active Indices:    %u", st.activeIndices);
        ImGui::Text("Active Faces:      %u", st.activeIndices / 3);
        ImGui::Text("Active Bones:      %u", st.activeBones);
        ImGui::Text("Active Particles:  %u", st.activeParticles);
        ImGui::Text("Draw Calls:        %u", st.drawCalls);
        ImGui::Separator();
        ImGui::Text("Materials: %zu", data.materials.size());
        ImGui::Text("Textures:  %zu", data.textures.size());
        ImGui::Text("Lights:    %zu", data.lights.size());
        ImGui::Text("Cameras:   %zu", data.cameras.size());
        ImGui::Text("Meshes:    %zu", data.meshes.size());
    }

    if (ImGui::CollapsingHeader("Frame Steps###st_steps"))
    {
        ImGui::Text("Frame Time:        %.2f ms", st.frameTime);
        ImGui::Text("Inter-Frame:       %.2f ms", st.interFrameTime);
        ImGui::Text("Render:            %.2f ms", st.renderTime);
        ImGui::Text("Camera Render:     %.2f ms", st.cameraRenderTime);
        ImGui::Text("Mesh Selection:    %.2f ms", st.activeMeshesEvalTime);
        ImGui::Text("Render Targets:    %.2f ms", st.renderTargetsTime);
        ImGui::Text("Particles:         %.2f ms", st.particlesTime);
        ImGui::Text("Sprites:           %.2f ms", st.spritesTime);
        ImGui::Text("Animations:        %.2f ms", st.animationsTime);
        ImGui::Text("Physics:           %.2f ms", st.physicsTime);
    }

    if (ImGui::CollapsingHeader("System###st_sys"))
    {
        if (!st.engineVersion.empty())
            ImGui::Text("Engine: Babylon.js %s", st.engineVersion.c_str());
        if (st.renderWidth > 0 && st.renderHeight > 0)
            ImGui::Text("Resolution: %d x %d", st.renderWidth, st.renderHeight);
        ImGui::Text("HW Scaling: %.2f", st.hardwareScaling);
    }
}

// ===========================================================================
// Debug panel
// ===========================================================================
inline void RenderDebugPanel()
{
    auto& state = s_state;

    if (ImGui::CollapsingHeader("Helpers###dbg_helpers", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Grid###dbg_grid", &state.debugGrid))
            EmitCmd(CMD_DEBUG_TOGGLE_GRID, 0);
        if (ImGui::Checkbox("Bounding Boxes###dbg_bbox", &state.debugBBox))
            EmitCmd(CMD_DEBUG_TOGGLE_BBOX, 0);
        if (ImGui::Checkbox("World Axes###dbg_axes", &state.debugAxes))
            EmitCmd(CMD_DEBUG_TOGGLE_AXES, 0);
    }

    if (ImGui::CollapsingHeader("Texture Channels###dbg_tex"))
    {
        const char* channelNames[] = {
            "Diffuse", "Ambient", "Specular", "Emissive",
            "Bump", "Opacity", "Reflection", "Refraction", "Lightmap"
        };
        for (int i = 0; i < 9; i++)
        {
            char cid[32]; snprintf(cid, sizeof(cid), "###tc_%d", i);
            if (ImGui::Checkbox(cid, &state.texChannels[i]))
            {
                EmitCmd(CMD_DEBUG_SET_TEX_CHANNEL, 0);
                s_cmdWriter.writeU8(static_cast<uint8_t>(i));
                s_cmdWriter.writeU8(state.texChannels[i] ? 1 : 0);
            }
            ImGui::SameLine();
            ImGui::Text("%s", channelNames[i]);
        }
    }

    if (ImGui::CollapsingHeader("Features###dbg_feat", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Animations###dbg_anim", &state.debugAnimations))
            EmitCmd(CMD_DEBUG_TOGGLE_ANIMATIONS, 0);
        if (ImGui::Checkbox("Particles###dbg_part", &state.debugParticles))
            EmitCmd(CMD_DEBUG_TOGGLE_PARTICLES, 0);
        if (ImGui::Checkbox("Fog###dbg_fog", &state.debugFog))
            EmitCmd(CMD_DEBUG_TOGGLE_FOG, 0);
        if (ImGui::Checkbox("Shadows###dbg_shad", &state.debugShadows))
            EmitCmd(CMD_DEBUG_TOGGLE_SHADOWS, 0);
        if (ImGui::Checkbox("Lights###dbg_lights", &state.debugLights))
            EmitCmd(CMD_DEBUG_TOGGLE_LIGHTS, 0);
        if (ImGui::Checkbox("Post-Processes###dbg_pp", &state.debugPostProc))
            EmitCmd(CMD_DEBUG_TOGGLE_POSTPROC, 0);
        if (ImGui::Checkbox("Skeletons###dbg_skel", &state.debugSkeletons))
            EmitCmd(CMD_DEBUG_TOGGLE_SKELETONS, 0);
    }
}

// ===========================================================================
// Main render function — fixed right-side panel with tabs
// ===========================================================================
inline void RenderInspector(SceneData& data)
{
    const ImGuiIO& io = ImGui::GetIO();

    float panelWidth = io.DisplaySize.x * 0.2f;
    if (panelWidth < 280) panelWidth = 280;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("Inspector###inspector_panel", nullptr, flags);

    if (!data.valid)
    {
        ImGui::TextDisabled("No scene data available");
        ImGui::End();
        return;
    }

    // Tab bar
    if (ImGui::BeginTabBar("###inspector_tabs"))
    {
        if (ImGui::BeginTabItem("Explorer"))
        {
            s_state.activeTab = 0;
            RenderSceneExplorer(data);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Properties"))
        {
            s_state.activeTab = 1;
            RenderPropertiesPanel(data);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Stats"))
        {
            s_state.activeTab = 2;
            RenderStatsPanel(data);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debug"))
        {
            s_state.activeTab = 3;
            RenderDebugPanel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace SceneInspector

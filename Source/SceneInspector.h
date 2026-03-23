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

    static constexpr uint32_t MAGIC = 0x42534E44; // "BSND"
    static constexpr uint32_t VERSION = 2;

    static constexpr uint32_t CMD_MAGIC = 0x434D4432; // "CMD2"
    static constexpr uint32_t CMD_VERSION = 1;

    static constexpr float RAD2DEG = 57.2957795f;
    static constexpr float DEG2RAD = 0.0174532925f;
    static constexpr float FLT_BIG = 3.402823e+38f;

    // ===========================================================================
    // Command types (must match JS constants)
    // ===========================================================================
    enum InspectorCmd : uint8_t
    {
        CMD_SET_POSITION = 0x01,
        CMD_SET_ROTATION = 0x02,
        CMD_SET_SCALING = 0x03,
        CMD_SET_ENABLED = 0x10,
        CMD_SET_VISIBLE = 0x11,
        CMD_SET_WIREFRAME = 0x12,
        CMD_SET_BACK_FACE_CULLING = 0x13,
        CMD_SET_RECEIVE_SHADOWS = 0x14,
        CMD_SET_POINTS_CLOUD_MAT = 0x15,
        CMD_SET_DISABLE_DEPTH_WRITE = 0x16,
        CMD_SET_FORCE_DEPTH_WRITE = 0x17,
        CMD_SET_AUTO_CLEAR = 0x18,
        CMD_SET_SHADOW_ENABLED = 0x19,
        CMD_SET_ALPHA = 0x20,
        CMD_SET_VISIBILITY = 0x21,
        CMD_SET_INTENSITY = 0x22,
        CMD_SET_FOV = 0x23,
        CMD_SET_MIN_Z = 0x24,
        CMD_SET_MAX_Z = 0x25,
        CMD_SET_INERTIA = 0x26,
        CMD_SET_SPEED = 0x27,
        CMD_SET_RANGE = 0x28,
        CMD_SET_SPOT_ANGLE = 0x29,
        CMD_SET_INNER_ANGLE = 0x2A,
        CMD_SET_EXPONENT = 0x2B,
        CMD_SET_ALPHA_CUTOFF = 0x2C,
        CMD_SET_Z_OFFSET = 0x2D,
        CMD_SET_Z_OFFSET_UNITS = 0x2E,
        CMD_SET_METALLIC = 0x2F,
        CMD_SET_ROUGHNESS = 0x30,
        CMD_SET_ENV_INTENSITY = 0x31,
        CMD_SET_CONTRAST = 0x32,
        CMD_SET_EXPOSURE = 0x33,
        CMD_SET_SPEED_RATIO = 0x34,
        CMD_SET_ARC_ALPHA = 0x35,
        CMD_SET_ARC_BETA = 0x36,
        CMD_SET_ARC_RADIUS = 0x37,
        CMD_SET_FOG_DENSITY = 0x38,
        CMD_SET_FOG_START = 0x39,
        CMD_SET_FOG_END = 0x3A,
        CMD_SET_LAYER_MASK = 0x3B,
        CMD_SET_TEXTURE_LEVEL = 0x3C,
        CMD_SET_MORPH_INFLUENCE = 0x3D,
        CMD_SET_SHADOW_BIAS = 0x3E,
        CMD_SET_SHADOW_NORMAL_BIAS = 0x3F,
        CMD_SET_DIFFUSE_COLOR = 0x40,
        CMD_SET_SPECULAR_COLOR = 0x41,
        CMD_SET_EMISSIVE_COLOR = 0x42,
        CMD_SET_AMBIENT_COLOR = 0x43,
        CMD_SET_LIGHT_DIFFUSE = 0x44,
        CMD_SET_LIGHT_SPECULAR = 0x45,
        CMD_SET_FOG_COLOR = 0x46,
        CMD_SET_SCENE_AMBIENT = 0x47,
        CMD_SET_CLEAR_COLOR = 0x50,
        CMD_SET_DIRECTION = 0x60,
        CMD_SET_TARGET = 0x61,
        CMD_SET_GRAVITY = 0x62,
        CMD_SET_TRANSPARENCY_MODE = 0x70,
        CMD_SET_FOG_MODE = 0x71,
        CMD_SET_TONE_MAPPING_TYPE = 0x72,
        CMD_SET_CAMERA_MODE = 0x73,
        CMD_SET_NAME = 0x80,
        CMD_DISPOSE_ENTITY = 0x90,
        CMD_ANIM_PLAY = 0x91,
        CMD_ANIM_PAUSE = 0x92,
        CMD_ANIM_STOP = 0x93,
        CMD_SKELETON_RETURN_TO_REST = 0x94,
        CMD_SET_FORCE_WIREFRAME = 0x95,
        CMD_SET_FORCE_POINTS_CLOUD = 0x96,
        CMD_DEBUG_TOGGLE_GRID = 0xA0,
        CMD_DEBUG_TOGGLE_NAMES = 0xA1,
        CMD_DEBUG_TOGGLE_PHYSICS = 0xA2,
        CMD_DEBUG_TOGGLE_BBOX = 0xA4,
        CMD_DEBUG_TOGGLE_AXES = 0xA5,
        CMD_DEBUG_SET_TEX_CHANNEL = 0xA6,
        CMD_DEBUG_TOGGLE_ANIMATIONS = 0xA7,
        CMD_DEBUG_TOGGLE_PARTICLES = 0xA8,
        CMD_DEBUG_TOGGLE_FOG = 0xA9,
        CMD_DEBUG_TOGGLE_SHADOWS = 0xAA,
        CMD_DEBUG_TOGGLE_LIGHTS = 0xAB,
        CMD_DEBUG_TOGGLE_POSTPROC = 0xAC,
        CMD_DEBUG_TOGGLE_SKELETONS = 0xAD,
        CMD_SET_ANIM_FRAME = 0xB0,
        CMD_SET_ANIM_LOOP = 0xB1,
        CMD_SELECT_ENTITY = 0xC0,
    };

    // ===========================================================================
    // Binary reader with bounds checking
    // ===========================================================================
    class BinaryReader
    {
        const uint8_t* m_data;
        size_t m_size;
        size_t m_pos = 0;
        bool m_error = false;

        void ensureAvailable(size_t n)
        {
            if (m_pos + n > m_size)
                m_error = true;
        }

    public:
        BinaryReader(const uint8_t* data, size_t size)
            : m_data(data)
            , m_size(size)
        {
        }

        bool valid() const { return !m_error; }

        uint8_t readU8()
        {
            ensureAvailable(1);
            if (m_error)
                return 0;
            return m_data[m_pos++];
        }

        uint16_t readU16()
        {
            ensureAvailable(2);
            if (m_error)
                return 0;
            uint16_t val;
            std::memcpy(&val, m_data + m_pos, 2);
            m_pos += 2;
            return val;
        }

        uint32_t readU32()
        {
            ensureAvailable(4);
            if (m_error)
                return 0;
            uint32_t val;
            std::memcpy(&val, m_data + m_pos, 4);
            m_pos += 4;
            return val;
        }

        int32_t readI32()
        {
            ensureAvailable(4);
            if (m_error)
                return 0;
            int32_t val;
            std::memcpy(&val, m_data + m_pos, 4);
            m_pos += 4;
            return val;
        }

        float readF32()
        {
            ensureAvailable(4);
            if (m_error)
                return 0.0f;
            float val;
            std::memcpy(&val, m_data + m_pos, 4);
            m_pos += 4;
            return val;
        }

        std::string readString()
        {
            uint16_t len = readU16();
            if (m_error)
                return "";
            ensureAvailable(len);
            if (m_error)
                return "";
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
            writeF32(v[0]);
            writeF32(v[1]);
            writeF32(v[2]);
        }

        void writeColor3(const float c[3])
        {
            writeF32(c[0]);
            writeF32(c[1]);
            writeF32(c[2]);
        }

        void writeColor4(const float c[4])
        {
            writeF32(c[0]);
            writeF32(c[1]);
            writeF32(c[2]);
            writeF32(c[3]);
        }

        void writeString(const std::string& s)
        {
            uint16_t len = static_cast<uint16_t>(s.size() > 65535 ? 65535 : s.size());
            writeU16(len);
            m_buf.insert(m_buf.end(), s.begin(), s.begin() + len);
        }

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
        std::vector<CameraInfo> cameras;
        std::vector<NodeInfo> nodes;
        std::vector<MeshInfo> meshes;
        std::vector<MaterialInfo> materials;
        std::vector<TextureInfo> textures;
        std::vector<LightInfo> lights;
        std::vector<AnimGroupInfo> animGroups;
        std::vector<SkeletonInfo> skeletons;
    };

    // ===========================================================================
    // Parse binary buffer into SceneData
    // ===========================================================================
    SceneData Parse(const uint8_t* data, size_t size);

    // ===========================================================================
    // Inspector UI — class-based, no static global state
    // ===========================================================================
    enum class EntityType : uint8_t
    {
        None,
        Scene,
        Camera,
        Node,
        Mesh,
        Material,
        Texture,
        Light,
        AnimGroup,
        Skeleton
    };

    struct SelectedEntity
    {
        EntityType type = EntityType::None;
        uint32_t uniqueId = 0;
        size_t index = 0;
    };

    class Inspector
    {
    public:
        void BeginCommands();
        void EndCommands();
        bool HasPendingCommands() const;
        const uint8_t* GetCommandData() const;
        size_t GetCommandSize() const;

        void RenderInspector(SceneData& data);

    private:
        // UI state
        SelectedEntity m_selected;
        int m_activeTab = 0;
        char m_searchBuf[128] = "";
        bool m_autoSwitchToProps = true;

        // Debug toggles
        bool m_debugGrid = false;
        bool m_debugNames = false;
        bool m_debugPhysics = false;
        bool m_debugBBox = false;
        bool m_debugAxes = false;
        bool m_debugAnimations = true;
        bool m_debugParticles = true;
        bool m_debugFog = true;
        bool m_debugShadows = true;
        bool m_debugLights = true;
        bool m_debugPostProc = true;
        bool m_debugSkeletons = true;
        bool m_texChannels[9] = {true, true, true, true, true, true, true, true, true};

        // FPS graph
        float m_fpsHistory[120] = {};
        int m_fpsHistoryIdx = 0;

        // Command buffer
        BinaryWriter m_cmdWriter;
        uint32_t m_cmdCount = 0;

        // Command helpers
        void EmitCmd(InspectorCmd cmd, uint32_t uid);
        void EmitCmdBool(InspectorCmd cmd, uint32_t uid, bool val);
        void EmitCmdFloat(InspectorCmd cmd, uint32_t uid, float val);
        void EmitCmdVec3(InspectorCmd cmd, uint32_t uid, const float v[3]);
        void EmitCmdColor3(InspectorCmd cmd, uint32_t uid, const float c[3]);
        void EmitCmdColor4(InspectorCmd cmd, uint32_t uid, const float c[4]);
        void EmitCmdInt(InspectorCmd cmd, uint32_t uid, int32_t val);

        // Selection helpers
        void SelectEntity(EntityType type, uint32_t uid, size_t idx);
        bool IsSelected(EntityType type, uint32_t uid) const;

        // UI widget helpers
        bool EditVec3(const char* label, float v[3], uint32_t uid, InspectorCmd cmd, float speed = 0.05f);
        bool EditFloat(const char* label, float* v, uint32_t uid, InspectorCmd cmd,
            float speed = 0.01f, float mn = 0, float mx = 0, const char* fmt = "%.3f");
        bool EditSlider(const char* label, float* v, uint32_t uid, InspectorCmd cmd,
            float mn, float mx, const char* fmt = "%.3f");
        bool EditColor3(const char* label, float c[3], uint32_t uid, InspectorCmd cmd);
        bool EditColor4(const char* label, float c[4], uint32_t uid, InspectorCmd cmd);
        bool EditBool(const char* label, bool* v, uint32_t uid, InspectorCmd cmd);

        // Tree/graph helpers
        void RenderTreeItem(const char* label, EntityType type, uint32_t uid, size_t idx);

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

        SceneGraph BuildSceneGraph(const SceneData& data);
        bool HasFilterMatch(const SceneGraph& g, size_t idx, const char* filter);
        void RenderGraphNode(const SceneGraph& g, size_t idx, const char* filter, SceneData& data);

        // Panel renderers
        void RenderSceneExplorer(SceneData& data);
        void RenderPropertiesPanel(SceneData& data);
        void RenderStatsPanel(const SceneData& data);
        void RenderDebugPanel();

        // Property renderers
        void RenderSceneProps(SceneData& data);
        void RenderCameraProps(CameraInfo& cam);
        void RenderTransformProps(float pos[3], float rot[4], float scl[3], bool hasQuat, uint32_t uid);
        void RenderNodeProps(NodeInfo& node);
        void RenderMeshProps(MeshInfo& mesh, const SceneData& data);
        void RenderLightProps(LightInfo& light);
        void RenderMaterialProps(MaterialInfo& mat);
        void RenderTextureProps(TextureInfo& tex);
        void RenderAnimGroupProps(AnimGroupInfo& ag);
        void RenderSkeletonProps(SkeletonInfo& sk);
    };

    // Name helpers (stateless utilities)
    const char* CameraTypeName(uint8_t type);
    const char* LightTypeName(uint8_t type);
    const char* FogModeName(uint8_t mode);
    const char* TransparencyModeName(int32_t mode);
    const char* CoordinatesModeName(uint8_t mode);

    // Widget helpers (stateless utilities)
    void AttributeBadge(const char* name, bool has);
    bool CaseInsensitiveContains(const std::string& haystack, const char* needle);

} // namespace SceneInspector

#include "PlaygroundPanel.h"

#include <cstring>

static const char* s_defaultCode =
    "var createScene = function (engine) {\n"
    "    var scene = new BABYLON.Scene(engine);\n"
    "    scene.createDefaultCamera(true, true, true);\n"
    "    var camera = scene.activeCamera;\n"
    "    camera.setTarget(BABYLON.Vector3.Zero());\n"
    "    camera.position = new BABYLON.Vector3(0, 5, -10);\n"
    "\n"
    "    var light = new BABYLON.HemisphericLight(\n"
    "        \"light\", new BABYLON.Vector3(0, 1, 0), scene);\n"
    "    light.intensity = 0.7;\n"
    "\n"
    "    var sphere = BABYLON.MeshBuilder.CreateSphere(\n"
    "        \"sphere\", { diameter: 2, segments: 32 }, scene);\n"
    "    sphere.position.y = 1;\n"
    "\n"
    "    BABYLON.MeshBuilder.CreateGround(\n"
    "        \"ground\", { width: 6, height: 6 }, scene);\n"
    "\n"
    "    return scene;\n"
    "};\n";

PlaygroundPanel::PlaygroundPanel()
{
    std::snprintf(m_codeBuf, CODE_BUF_SIZE, "%s", s_defaultCode);
}

void PlaygroundPanel::SyncCode(const std::string& code)
{
    m_syncCode = code;
    m_pendingSync = true;
}

void PlaygroundPanel::Render(const ImGuiIO& io, const PlaygroundCallbacks& callbacks)
{
    float leftPanelWidth = io.DisplaySize.x * 0.2f;
    if (leftPanelWidth < 280)
        leftPanelWidth = 280;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, io.DisplaySize.y));
    ImGui::Begin("Babylon Native Playground", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse);

    // Load Assets section
    if (ImGui::CollapsingHeader("Load Assets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Open GLB File..."))
        {
            if (callbacks.openGLBFile)
                callbacks.openGLBFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Open ENV File..."))
        {
            if (callbacks.openENVFile)
                callbacks.openENVFile();
        }
        ImGui::TextDisabled("or drag & drop .glb / .obj / .env files");
    }

    // Load Playground section
    if (ImGui::CollapsingHeader("Load Playground", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Playground Hash:");
        ImGui::InputText("##hash", m_hashBuf, sizeof(m_hashBuf));
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            std::string hash(m_hashBuf);
            if (!hash.empty() && callbacks.loadPlayground)
                callbacks.loadPlayground(hash);
        }
    }

    ImGui::Separator();

    // Sync code from JS if pending
    if (m_pendingSync)
    {
        std::snprintf(m_codeBuf, CODE_BUF_SIZE, "%s", m_syncCode.c_str());
        m_pendingSync = false;
    }

    // Code Editor section
    ImGui::Separator();
    ImGui::Text("Code Editor:");
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float editorHeight = avail.y - 80.0f;
    if (editorHeight < 100.0f)
        editorHeight = 100.0f;
    ImGui::InputTextMultiline("##code", m_codeBuf, CODE_BUF_SIZE,
        ImVec2(-1.0f, editorHeight),
        ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Run Code"))
    {
        std::string code(m_codeBuf);
        if (!code.empty() && callbacks.runPlaygroundCode)
            callbacks.runPlaygroundCode(code);
    }
    ImGui::SameLine();
    ImGui::Text("%.1f FPS", io.Framerate);
    ImGui::SameLine();
    ImGui::TextDisabled("(F1: toggle | Ctrl+R: reset)");

    ImGui::End();
}

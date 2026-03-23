#pragma once

#include <cstdio>
#include <functional>
#include <string>

#include "imgui.h"

// Callbacks the panel uses to communicate with the application layer
struct PlaygroundCallbacks
{
    std::function<void(const std::string&)> loadPlayground;
    std::function<void(const std::string&)> runPlaygroundCode;
    std::function<void()> openGLBFile;
    std::function<void()> openENVFile;
#ifdef HAS_SUBSTANCE_SDK
    std::function<void()> openSBSARFile;
#endif
};

class PlaygroundPanel
{
public:
    PlaygroundPanel();

    // Call once per frame to render the left-side panel
    void Render(const ImGuiIO& io, const PlaygroundCallbacks& callbacks);

    // Called when JS sends new playground code to display in the editor
    void SyncCode(const std::string& code);

private:
    static constexpr size_t CODE_BUF_SIZE = 64 * 1024;

    char m_hashBuf[256] = "";
    char m_codeBuf[CODE_BUF_SIZE];
    bool m_pendingSync = false;
    std::string m_syncCode;
};

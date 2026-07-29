#pragma once

// Requires MinHook: https://github.com/TsudaKageyu/minhook
// Link against libMinHook.x64.lib (or build the source into the project).

namespace hooks
{
    // Init MinHook + install D3D11 Present hook. Returns true on success.
    bool Install();
    void Uninstall();

    // High-frequency input pump — call from a dedicated thread (~5ms).
    // Polls all VKs, handles INSERT/menu hotkeys and syncs UI -> features.
    void PollInput();

    // Backbuffer size (from swapchain desc). (0,0) until hooks init.
    void GetScreenSize(float& w, float& h);
}

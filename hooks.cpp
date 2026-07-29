#include "hooks.h"
#include "features.h"
#include "game.h"
#include "ctx.hpp"
#include "render.hpp"
#include "log.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>
#include <atomic>
#include <thread>
#include <chrono>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ctx.hpp already provides `inline ctx_t ctx;`, `inline bool keys[256];`,
// `inline bool oldkeys[256];` — do NOT redefine here.
//
// Fonts declared extern in tab.hpp — define storage exactly once here.
ImFont* fnrm = nullptr;
ImFont* fttl = nullptr;

// Sample-menu helpers used by render/tab code (originally in main.cpp).
bool presschk(int vk) { return keys[vk] && !oldkeys[vk]; }
bool downchk(int vk)  { return keys[vk]; }
bool latchchk(int vk)
{
    if (!keyslatch[vk]) return false;
    keyslatch[vk] = false;   // consume
    return true;
}

void bind_t::updbind()
{
    if (mode == 2 && presschk(key) && key != 0) active = !active;
}
bool bind_t::isact() const
{
    if (mode == 0) return true;
    if (!key)      return false;
    return (mode == 1) ? downchk(key) : active;
}

std::string_view keynm(int vk)
{
    if (!vk) return "-";
    static char n[128];
    UINT sc = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    switch (vk)
    {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        case VK_XBUTTON1: case VK_XBUTTON2:
            sc |= KF_EXTENDED; break;
    }
    if (vk == VK_LBUTTON)  return "M1";
    if (vk == VK_RBUTTON)  return "M2";
    if (vk == VK_MBUTTON)  return "M3";
    if (vk == VK_XBUTTON1) return "M4";
    if (vk == VK_XBUTTON2) return "M5";
    return (!GetKeyNameTextA(sc << 16, n, sizeof(n)))
        ? std::string_view(std::to_string(vk))
        : std::string_view(n);
}

// ------------------------------------------------------------------
namespace {
    using PresentFn      = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBufFn    = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    PresentFn      oPresent       = nullptr;
    ResizeBufFn    oResizeBuffers = nullptr;

    // Hook targets — kept for MH_DisableHook/MH_RemoveHook during unhook.
    void* g_PresentTarget = nullptr;
    void* g_ResizeTarget  = nullptr;

    // In-flight guard: how many threads are currently executing inside our
    // hooked functions. Unhook MUST wait for this to reach zero before the
    // DLL's code pages may be freed.
    std::atomic<LONG> g_InFlight{ 0 };
    struct InFlightGuard {
        InFlightGuard()  { ++g_InFlight; }
        ~InFlightGuard() { --g_InFlight; }
    };
    // Hard kill-switch: once uninstall begins, hooks still in flight render
    // nothing and exit ASAP.
    std::atomic<bool> g_ShuttingDown{ false };

    WNDPROC   oWndProc = nullptr;
    HWND      g_Hwnd   = nullptr;
    ID3D11Device*        g_Dev  = nullptr;
    ID3D11DeviceContext* g_Ctx  = nullptr;
    ID3D11RenderTargetView* g_RTV = nullptr;
    bool g_ImGuiReady = false;
    float g_MenuAnim  = 0.f;   // 0..1  eased when ctx.open toggles
    std::atomic<float> g_ScreenW{ 0.f };
    std::atomic<float> g_ScreenH{ 0.f };
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
    void CreateRTV(IDXGISwapChain* sc)
    {
        if (g_RTV) return;
        ID3D11Texture2D* backBuf = nullptr;
        if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuf)) && backBuf)
        {
            g_Dev->CreateRenderTargetView(backBuf, nullptr, &g_RTV);
            backBuf->Release();
        }
    }

    void DrawESP();   // fwd — defined below RenderFrame

    void DestroyRTV()
    {
        if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; }
    }

    // Poll all 256 VKs — 5ms input pump is the ONLY caller.
    // Sets a 300ms edge latch so ImGui-frame-rate consumers (mkbind) don't
    // miss short taps.
    void PollKeys()
    {
        const DWORD now = GetTickCount();
        for (int i = 0; i < 256; ++i)
        {
            const bool down = (GetAsyncKeyState(i) & 0x8000) != 0;
            if (down && !keys[i]) { keyslatch[i] = true; keysedge_time[i] = now; }
            oldkeys[i] = keys[i];
            keys[i]    = down;
            if (keyslatch[i] && (now - keysedge_time[i] > 300)) keyslatch[i] = false;
        }
    }

    // Toggle ctx.open on INSERT + push hotkey-driven feature toggles.
    void HandleMenuInput()
    {
        if (presschk(VK_INSERT)) ctx.open = !ctx.open;

        ctx.aimkey.updbind();
        if (ctx.aimkey.mode == 2 && presschk(ctx.aimkey.key) && ctx.aimkey.key != 0)
        { ctx.aim = !ctx.aim; ctx.aimkey.active = ctx.aim; }

        ctx.trigkey.updbind();
        if (ctx.trigkey.mode == 2 && presschk(ctx.trigkey.key) && ctx.trigkey.key != 0)
        { ctx.trig = !ctx.trig; ctx.trigkey.active = ctx.trig; }
    }

    void SyncCtxToFeatures()
    {
        auto& c = features::GetConfig();

        // Aim / trigger honour keybind mode (always/hold/toggle)
        c.Aimbot     = ctx.aim  && ctx.aimkey.isact();
        c.Triggerbot = ctx.trig && ctx.trigkey.isact();
        c.AimFov     = ctx.fov * 4.0f;              // deg -> approx px
        c.AimAtHead  = (ctx.hitbox == 0);

        // Player
        c.GodMode         = ctx.god;
        c.InfVitality     = ctx.infVitality;
        c.InfAmmo         = ctx.infAmmo;
        c.NoRecoil        = ctx.noRecoil;
        c.BigDamage       = ctx.bigDamage;
        c.DamageMultiplier = ctx.damageMult;
        c.InfBackpack     = ctx.bigBackpack;

        // Movement
        c.SpeedHack   = ctx.speedHack;
        c.SpeedValue  = ctx.speedVal;
        c.JumpHack    = ctx.jumpHack;
        c.JumpValue   = ctx.jumpVal;
        c.FlyHack     = ctx.flyHack;
        c.NoGravity   = ctx.noGravity;

        // World / visuals
        c.XRayZombies = ctx.xrayZombies || (ctx.espen && ctx.box);

        // Telekill (edge-triggered)
        if (ctx.telekillHotkey && ctx.telekillKey.isact())
        {
            c.Telekill = true;
            ctx.telekillHotkey = false;   // one-shot
        }

        // Difficulty apply (edge-triggered by button)
        if (ctx.applyDifficulty)
        {
            c.Difficulty = ctx.difficulty;
            ctx.applyDifficulty = false;
        }

        // ---- toggle-change logging (console) ----
        static bool s_prevInit = false;
        static struct {
            bool god, vit, ammo, rec, dmg, bag, spd, jmp, fly, nograv, xray, aim, trig;
        } s_prev{};
        if (!s_prevInit) { s_prevInit = true; s_prev = { false }; }
        auto logFlip = [](const char* name, bool now, bool& prev) {
            if (now != prev) { tka::LogFeat("%s -> %s", name, now ? "ON" : "off"); prev = now; }
        };
        logFlip("GodMode",     c.GodMode,      s_prev.god);
        logFlip("InfVitality", c.InfVitality,  s_prev.vit);
        logFlip("InfAmmo",     c.InfAmmo,      s_prev.ammo);
        logFlip("NoRecoil",    c.NoRecoil,     s_prev.rec);
        logFlip("BigDamage",   c.BigDamage,    s_prev.dmg);
        logFlip("BigBackpack", c.InfBackpack,  s_prev.bag);
        logFlip("SpeedHack",   c.SpeedHack,    s_prev.spd);
        logFlip("JumpHack",    c.JumpHack,     s_prev.jmp);
        logFlip("FlyHack",     c.FlyHack,      s_prev.fly);
        logFlip("NoGravity",   c.NoGravity,    s_prev.nograv);
        logFlip("XRay",        c.XRayZombies,  s_prev.xray);
        logFlip("Aimbot",      c.Aimbot,       s_prev.aim);
        logFlip("Triggerbot",  c.Triggerbot,   s_prev.trig);
    }

    LRESULT CALLBACK WndProcImpl(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (g_ImGuiReady && ctx.open)
        {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp);
            const ImGuiIO& io = ImGui::GetIO();
            // Swallow mouse/keyboard so game doesn't receive it while typing.
            if (io.WantCaptureMouse   && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) return 1;
            if (io.WantCaptureKeyboard && (msg == WM_KEYDOWN || msg == WM_KEYUP ||
                                            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
                                            msg == WM_CHAR)) return 1;
        }
        return CallWindowProc(oWndProc, hWnd, msg, wp, lp);
    }

    // SEH wrapper — UE's global exception filter turns ANY access violation in
    // ANY thread into the "Fatal Error" dialog. Our render/wndproc paths must
    // never let one escape.
    LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        __try { return WndProcImpl(hWnd, msg, wp, lp); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return oWndProc ? CallWindowProc(oWndProc, hWnd, msg, wp, lp)
                            : DefWindowProc(hWnd, msg, wp, lp);
        }
    }

    void InitImGui(IDXGISwapChain* sc)
    {
        if (g_ImGuiReady) return;
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_Dev)))
            g_Dev->GetImmediateContext(&g_Ctx);
        if (!g_Dev || !g_Ctx) return;

        DXGI_SWAP_CHAIN_DESC desc{};
        sc->GetDesc(&desc);
        g_Hwnd   = desc.OutputWindow;
        g_ScreenW.store((float)desc.BufferDesc.Width,  std::memory_order_release);
        g_ScreenH.store((float)desc.BufferDesc.Height, std::memory_order_release);
        oWndProc = (WNDPROC)SetWindowLongPtr(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;  // don't write imgui.ini
        io.LogFilename = nullptr;

        // Fonts (fall back to default if file missing)
        fnrm = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf",  14.0f);
        fttl = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdanab.ttf", 16.0f);
        if (!fnrm) fnrm = io.Fonts->AddFontDefault();
        if (!fttl) fttl = fnrm;

        ImGui_ImplWin32_Init(g_Hwnd);
        ImGui_ImplDX11_Init(g_Dev, g_Ctx);

        g_ImGuiReady = true;
    }

    void RenderFrame()
    {
        if (!g_ImGuiReady || !g_RTV) return;

        // Eased menu open/close alpha (input state comes from the 5ms input
        // thread — see hooks::PollInput).
        const float dt = ImGui::GetIO().DeltaTime;
        g_MenuAnim = (g_MenuAnim + dt * 12.0f * (ctx.open ? 1.0f : -1.0f));
        if (g_MenuAnim < 0.0f) g_MenuAnim = 0.0f;
        if (g_MenuAnim > 1.0f) g_MenuAnim = 1.0f;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawESP();

        if (ctx.showbinds) drawovr();
        if (g_MenuAnim > 0.01f) drawmenu(g_MenuAnim);

        ImGui::EndFrame();
        ImGui::Render();

        g_Ctx->OMSetRenderTargets(1, &g_RTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    // Real ESP: box + name(class) + health bar on live zombies, using the
    // camera POV from APlayerCameraManager::CameraCachePrivate.
    void DrawESP()
    {
        if (!ctx.espen) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float sw = ImGui::GetIO().DisplaySize.x;
        const float sh = ImGui::GetIO().DisplaySize.y;

        const ImU32 boxCol = ImGui::GetColorU32(ImVec4(ctx.boxclr[0], ctx.boxclr[1], ctx.boxclr[2], 1.f));
        const ImU32 nmCol  = ImGui::GetColorU32(ImVec4(ctx.nmclr[0],  ctx.nmclr[1],  ctx.nmclr[2],  1.f));

        for (const auto& e : features::GetEntitiesCopy())
        {
            if (!e.Actor || !e.IsZombie || e.HP <= 0) continue;

            float hx, hy, px, py;
            if (!game::WorldToScreen(e.Head,   hx, hy, sw, sh)) continue;
            if (!game::WorldToScreen(e.Pelvis, px, py, sw, sh)) continue;

            // Box from head to feet (pelvis + leg margin).
            const float footY = py + (py - hy) * 0.55f;
            const float hgt   = footY - hy;
            if (hgt < 2.0f) continue;
            const float wid = hgt * 0.4f;
            const float cx  = (hx + px) * 0.5f;

            if (ctx.box)
                dl->AddRect(ImVec2(cx - wid * 0.5f, hy), ImVec2(cx + wid * 0.5f, footY), boxCol, 0.f, 0, 1.5f);

            if (ctx.hp)
            {
                const float ratio = (e.HPMax > 0) ? (e.HP / e.HPMax) : 0.f;
                const float bx = cx - wid * 0.5f - 5.f;
                dl->AddRectFilled(ImVec2(bx, hy), ImVec2(bx + 3.f, footY),
                                  IM_COL32(20, 20, 20, 200));
                const float fillY = footY - hgt * ratio;
                const ImU32 hpCol = IM_COL32((int)(255 * (1.f - ratio)), (int)(255 * ratio), 40, 255);
                dl->AddRectFilled(ImVec2(bx, fillY), ImVec2(bx + 3.f, footY), hpCol);
            }

            if (ctx.name)
            {
                char label[32];
                _snprintf_s(label, sizeof label, _TRUNCATE, "%.0fm", e.Dist / 100.f);
                const ImVec2 ts = ImGui::CalcTextSize(label);
                dl->AddText(ImVec2(cx - ts.x * 0.5f, hy - ts.y - 2.f), nmCol, label);
            }
        }

        // Aim FOV circle
        if (ctx.aim && ctx.drawfov)
            dl->AddCircle(ImVec2(sw * 0.5f, sh * 0.5f), ctx.fov * 4.0f,
                          IM_COL32(255, 255, 255, 60), 64, 1.f);
    }

    HRESULT STDMETHODCALLTYPE hkResizeBuffersImpl(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
    {
        DestroyRTV();
        HRESULT hr = oResizeBuffers(sc, n, w, h, f, flags);
        if (!g_ShuttingDown.load(std::memory_order_acquire))
            CreateRTV(sc);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE hkPresentImpl(IDXGISwapChain* sc, UINT sync, UINT flags)
    {
        InitImGui(sc);       // once
        CreateRTV(sc);       // idempotent
        RenderFrame();
        return oPresent(sc, sync, flags);
    }

    // SEH shells — no C++ locals here (C2712), guard object lives in caller.
    HRESULT STDMETHODCALLTYPE hkResizeBuffersGuarded(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
    {
        __try { return hkResizeBuffersImpl(sc, n, w, h, f, flags); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            tka::LogErr("hkResizeBuffers: exception swallowed");
            return oResizeBuffers(sc, n, w, h, f, flags);
        }
    }

    HRESULT STDMETHODCALLTYPE hkPresentGuarded(IDXGISwapChain* sc, UINT sync, UINT flags)
    {
        __try { return hkPresentImpl(sc, sync, flags); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            tka::LogErr("hkPresent: exception swallowed (render path)");
            return oPresent(sc, sync, flags);
        }
    }

    HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
    {
        InFlightGuard guard;
        if (g_ShuttingDown.load(std::memory_order_acquire))
            return oResizeBuffers(sc, n, w, h, f, flags);
        return hkResizeBuffersGuarded(sc, n, w, h, f, flags);
    }

    HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags)
    {
        InFlightGuard guard;
        if (g_ShuttingDown.load(std::memory_order_acquire))
            return oPresent(sc, sync, flags);
        return hkPresentGuarded(sc, sync, flags);
    }

    // Grab SwapChain vtable slots via a throwaway device.
    bool GetSwapchainSlots(void*& outPresent, void*& outResizeBuffers)
    {
        outPresent = outResizeBuffers = nullptr;

        WNDCLASSEX wc{ sizeof(wc), CS_HREDRAW | CS_VREDRAW, DefWindowProc,
                       0, 0, GetModuleHandle(nullptr), nullptr, nullptr,
                       nullptr, nullptr, L"tka_hk_wc", nullptr };
        if (!RegisterClassEx(&wc)) return false;
        HWND hwnd = CreateWindow(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd) { UnregisterClass(wc.lpszClassName, wc.hInstance); return false; }

        DXGI_SWAP_CHAIN_DESC scd{};
        scd.BufferCount = 1;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hwnd;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL fl;
        ID3D11Device* dev = nullptr;
        ID3D11DeviceContext* ctxD = nullptr;
        IDXGISwapChain* sc = nullptr;
        const D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            want, 1, D3D11_SDK_VERSION, &scd, &sc, &dev, &fl, &ctxD);
        if (SUCCEEDED(hr))
        {
            void** vt          = *(void***)sc;
            outPresent         = vt[8];   // IDXGISwapChain::Present
            outResizeBuffers   = vt[13];  // IDXGISwapChain::ResizeBuffers
        }
        if (sc)    sc->Release();
        if (ctxD)  ctxD->Release();
        if (dev)   dev->Release();
        DestroyWindow(hwnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return outPresent != nullptr;
    }
}

namespace hooks
{
    // Called from the dedicated input thread (~5ms). Single writer of
    // keys[]/oldkeys[] and owner of menu hotkey handling + UI->features sync.
    // Rendering no longer polls input, so edges can never be eaten.
    void PollInput()
    {
        PollKeys();
        HandleMenuInput();
        SyncCtxToFeatures();
    }

    void GetScreenSize(float& w, float& h)
    {
        w = g_ScreenW.load(std::memory_order_acquire);
        h = g_ScreenH.load(std::memory_order_acquire);
    }

    bool Install()
    {
        if (MH_Initialize() != MH_OK) return false;

        void* pPresent = nullptr; void* pResize = nullptr;
        if (!GetSwapchainSlots(pPresent, pResize)) return false;

        g_PresentTarget = pPresent;
        g_ResizeTarget  = pResize;
        g_ShuttingDown.store(false, std::memory_order_release);

        if (MH_CreateHook(pPresent, &hkPresent, (void**)&oPresent) != MH_OK) return false;
        if (MH_EnableHook(pPresent) != MH_OK) return false;

        if (MH_CreateHook(pResize, &hkResizeBuffers, (void**)&oResizeBuffers) != MH_OK) return false;
        if (MH_EnableHook(pResize) != MH_OK) return false;

        return true;
    }

    // REAL unhook — safe order of operations:
    //
    //   1. Flip kill-switch: any thread that enters our hooks from now on
    //      forwards straight to the original and renders nothing.
    //   2. Disable both hooks (MinHook restores the original prologue bytes;
    //      NEW callers go to the original function directly).
    //   3. Drain: wait until every thread that was already inside our hook
    //      has left. Without this, freeing the DLL while a game thread sits
    //      inside hkPresent = instant crash.
    //   4. Sleep a few frames as belt-and-suspenders (Present is per-frame).
    //   5. Restore the window WndProc — only if it still points at ours.
    //   6. Shut down ImGui backends (invalidates their D3D state objects),
    //      then release RTV, context, device in that order.
    //   7. Remove hook entries + MH_Uninitialize.
    //
    // Caller (dllmain) must already have stopped the feature thread so no
    // other thread touches game/UI state while this runs.
    void Uninstall()
    {
        // 1. kill-switch
        g_ShuttingDown.store(true, std::memory_order_release);
        tka::Log("[unhook] kill-switch set");

        // 2. disable hooks (restore original bytes)
        if (g_PresentTarget) MH_DisableHook(g_PresentTarget);
        if (g_ResizeTarget)  MH_DisableHook(g_ResizeTarget);
        tka::Log("[unhook] hooks disabled (prologues restored)");

        // 3. drain in-flight callers
        int waited = 0;
        for (int i = 0; i < 500 && g_InFlight.load(std::memory_order_acquire) > 0; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++waited;
        }
        tka::Log("[unhook] in-flight drained (%d left, waited %dms)",
                 (int)g_InFlight.load(), waited);

        // 4. extra frames of margin
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // 5. restore WndProc only if it's still ours — if someone chained on
        //    top of us, restoring would corrupt THEIR hook; leave it and leak
        //    the chain (still safe: our handler forwards via CallWindowProc).
        if (g_Hwnd && oWndProc)
        {
            if ((WNDPROC)GetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC) == WndProcHook)
            {
                SetWindowLongPtrW(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
                tka::Log("[unhook] WndProc restored");
            }
            else
                tka::Log("[unhook] WndProc was re-chained by someone else — left as-is");
            oWndProc = nullptr;
        }

        // 6. ImGui shutdown (game thread is no longer rendering our overlay)
        if (g_ImGuiReady)
        {
            ImGui_ImplDX11_Shutdown();   // releases imgui's shaders/buffers
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_ImGuiReady = false;
            tka::Log("[unhook] ImGui shut down");
        }

        // D3D resources — RTV first, then context, then device
        DestroyRTV();
        if (g_Ctx) { g_Ctx->Release(); g_Ctx = nullptr; }
        if (g_Dev) { g_Dev->Release(); g_Dev = nullptr; }
        g_Hwnd = nullptr;

        // 7. remove hook entries + uninit MinHook (frees trampoline memory)
        if (g_PresentTarget) { MH_RemoveHook(g_PresentTarget); g_PresentTarget = nullptr; }
        if (g_ResizeTarget)  { MH_RemoveHook(g_ResizeTarget);  g_ResizeTarget  = nullptr; }
        MH_Uninitialize();
        tka::Log("[unhook] MinHook entries removed, uninitialized");
    }
}

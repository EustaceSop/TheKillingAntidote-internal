#include <Windows.h>
#include <thread>
#include <chrono>

#include "sdk.h"
#include "game.h"
#include "features.h"
#include "hooks.h"
#include "spoof.h"
#include "log.h"
#include "ctx.hpp"

// ---------- Input + hotkey monitor (5ms) -----------------------------------
// INSERT  -> toggle menu      (handled in hooks::PollInput)
// END     -> unhook + unload  (here)
// NUMPAD0 -> unhook + unload  (legacy, kept)
// F1..F12 -> feature toggles  (here)
// ---------------------------------------------------------------------------

namespace {
    HMODULE g_Self = nullptr;
    volatile bool g_UnloadReq = false;

    void InputLoop()
    {
        auto& c = features::GetConfig();
        while (!g_UnloadReq)
        {
            // Single-writer key pump: keys[]/oldkeys[], INSERT toggle,
            // bind handling, UI->features sync. Never races with WndProc.
            hooks::PollInput();

            if (presschk(VK_F1)) { c.GodMode      = !c.GodMode;      }
            if (presschk(VK_F2)) { c.InfAmmo      = !c.InfAmmo;      }
            if (presschk(VK_F3)) { c.NoRecoil     = !c.NoRecoil;     }
            if (presschk(VK_F4)) { c.SpeedHack    = !c.SpeedHack;    }
            if (presschk(VK_F5)) { c.JumpHack     = !c.JumpHack;     }
            if (presschk(VK_F6)) { c.FlyHack      = !c.FlyHack;      }
            if (presschk(VK_F7)) { c.XRayZombies  = !c.XRayZombies;  }
            if (presschk(VK_F8)) { c.Aimbot       = !c.Aimbot;       }
            if (presschk(VK_F9)) { c.BigDamage    = !c.BigDamage;    }
            if (presschk(VK_F10)){ c.InfVitality  = !c.InfVitality;  }
            if (presschk(VK_F11)){ c.Telekill     = true;           }
            if (presschk(VK_F12)){ c.InfBackpack  = !c.InfBackpack;  }

            if (presschk(VK_END) || presschk(VK_NUMPAD0))
            {
                tka::Log("unload requested via %s", "END/NUM0");
                g_UnloadReq = true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    DWORD WINAPI EntryImpl()
    {
        tka::LogInit();
        tka::Log("injected, waiting for GObjects...");

        // 1. Wait for GObjects (game past early startup).
        bool sdkOk = false;
        for (int i = 0; i < 300; ++i)
        {
            if (sdk::Init()) { sdkOk = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (!sdkOk || !sdk::GObjects())
        {
            tka::LogErr("sdk::Init failed after 60s — wrong game version or too early. Unloading.");
            tka::LogShutdown();
            FreeLibraryAndExitThread(g_Self, 0);
            return 0;
        }
        tka::LogOk("sdk ready: GObjects=%p NumElements=%d NumChunks=%d",
                   (void*)sdk::GObjects(),
                   sdk::GObjects()->NumElements,
                   sdk::GObjects()->NumChunks);

        // 2. Wait for world + game state.
        for (int i = 0; i < 300 && !game::GameState(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        tka::LogOk("World=%p GameState=%p Jodi=%p",
                   (void*)game::World(), (void*)game::GameState(), (void*)game::Jodi());

        // 3. Feature loop.
        features::Start();
        tka::LogOk("feature thread started (33ms tick)");

        // 4. D3D11 + input hooks (features still work if this fails).
        if (hooks::Install()) tka::LogOk("hooks installed (Present + ResizeBuffers + WndProc)");
        else                  tka::LogErr("hooks::Install failed — menu overlay unavailable");

        tka::Log("ready. INSERT=menu  END=unhook");

        // 5. Input pump (blocks until unload requested).
        InputLoop();

        // 6. Clean shutdown — strict order:
        tka::Log("unhooking...");
        features::Stop();
        tka::LogOk("features stopped + game values restored");
        hooks::Uninstall();
        tka::LogOk("hooks removed (drained in-flight, WndProc restored, ImGui down)");
        tka::Log("bye.");
        tka::LogShutdown();

        FreeLibraryAndExitThread(g_Self, 0);
        return 0;
    }

    // SEH shell — early-startup GWorld can be half-initialised; an AV here
    // would otherwise surface as the game's Fatal Error dialog.
    DWORD WINAPI Entry(LPVOID)
    {
        __try { return EntryImpl(); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            tka::LogErr("Entry: unhandled exception — emergency unload");
            FreeLibraryAndExitThread(g_Self, 0);
            return 0;
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hMod);
        g_Self = hMod;
        CreateThread(nullptr, 0, Entry, nullptr, 0, nullptr);
        // SpoofThreadLocation((void*)Entry, hMod); // optional
    }
    return TRUE;
}

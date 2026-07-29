#include "features.h"
#include "game.h"
#include "offsets.h"
#include "sdk.h"
#include "log.h"
#include "hooks.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <Windows.h>

namespace {
    features::Config g_Cfg;
    std::vector<features::EntityData> g_Ents;
    std::mutex g_EntsMx;
    std::atomic<bool> g_Running{ false };
    std::thread g_Thread;

    // ================================================================
    // Original-value snapshots. Every feature that WRITES to game
    // memory captures the original value before its first write and
    // restores it when the feature is turned off (or on unload).
    // ================================================================
    struct MovOrig {
        bool    captured = false;
        float   maxWalk = 0, maxWalkCrouch = 0, maxSwim = 0, maxFly = 0;
        float   jumpZ = 0, gravity = 0, accel = 0, airControl = 0;
        uint8_t moveMode = 0;
    };
    MovOrig g_MovOrig;

    struct WpnOrig {
        bool    captured = false;
        void*   weapon  = nullptr;   // originals belong to THIS weapon instance
        float   recoil = 0, shake = 0, extraDamage = 0, curDamage = 0;
        float   recoilCur = 0;       // Jodi::recoil_current
    };
    WpnOrig g_WpnOrig;

    struct BagOrig {
        bool    captured = false;
        int32_t weightCap = 0;
    };
    BagOrig g_BagOrig;

    // Previous-frame toggle states (for falling-edge detection).
    struct Prev {
        bool speed = false, jump = false, fly = false, nograv = false;
        bool norecoil = false, bigdmg = false, bigbag = false;
    };
    Prev g_Prev;

    // (PickAimTarget removed — ApplyAimbot now does a real screen-space FOV
    //  search using the camera POV projection.)

    // Rotator from A->B
    FRotator LookAt(const FVector& from, const FVector& to)
    {
        FVector d = to - from;
        FRotator r;
        r.Yaw   = atan2f(d.Y, d.X) * (180.0f / 3.14159265f);
        r.Pitch = atan2f(d.Z, sqrtf(d.X * d.X + d.Y * d.Y)) * (180.0f / 3.14159265f);
        r.Roll  = 0;
        return r;
    }

    // ---- Per-tick appliers -----------------------------------------
    void ApplyGod()
    {
        UObject* jodi = game::Jodi();
        if (!jodi) return;
        // Direct field set (survives ProcessEvent overwrite next tick, so also refresh via UFn)
        float maxHP = game::GetMaxHealth(jodi);
        if (maxHP > 0) game::SetHealth(jodi, maxHP);
        // Refresh invincible timer every few ticks
        static int c = 0;
        if ((c++ % 30) == 0) game::SetInvincibleTime(9999.f);
    }

    void ApplyVitality()
    {
        UObject* jodi = game::Jodi();
        if (!jodi) return;
        float max = Get<float>(jodi, off::jodi::Vitality_Max);
        if (max > 0) Set<float>(jodi, off::jodi::vitality, max);
    }

    void ApplyMovement(const features::Config& c)
    {
        UObject* jodi = game::Jodi();
        if (!jodi) return;
        UObject* cm = *reinterpret_cast<UObject**>((uintptr_t)jodi + off::character::CharacterMovement);
        if (!cm) return;

        const bool anyMov = c.SpeedHack || c.JumpHack || c.NoGravity || c.FlyHack;

        // Snapshot originals once per enable-session.
        if (anyMov && !g_MovOrig.captured)
        {
            g_MovOrig.maxWalk       = Get<float>(cm, off::movement::MaxWalkSpeed);
            g_MovOrig.maxWalkCrouch = Get<float>(cm, off::movement::MaxWalkSpeedCrouched);
            g_MovOrig.maxSwim       = Get<float>(cm, off::movement::MaxSwimSpeed);
            g_MovOrig.maxFly        = Get<float>(cm, off::movement::MaxFlySpeed);
            g_MovOrig.jumpZ         = Get<float>(cm, off::movement::JumpZVelocity);
            g_MovOrig.gravity       = Get<float>(cm, off::movement::GravityScale);
            g_MovOrig.accel         = Get<float>(cm, off::movement::MaxAcceleration);
            g_MovOrig.airControl    = Get<float>(cm, off::movement::AirControl);
            g_MovOrig.moveMode      = Get<uint8_t>(cm, off::movement::MovementMode);
            g_MovOrig.captured      = true;
        }

        if (c.SpeedHack)
        {
            float s = c.SpeedValue.load();
            Set<float>(cm, off::movement::MaxWalkSpeed, s);
            Set<float>(cm, off::movement::MaxWalkSpeedCrouched, s * 0.6f);
        }
        else if (g_Prev.speed && g_MovOrig.captured)   // turned OFF this frame
        {
            Set<float>(cm, off::movement::MaxWalkSpeed, g_MovOrig.maxWalk);
            Set<float>(cm, off::movement::MaxWalkSpeedCrouched, g_MovOrig.maxWalkCrouch);
        }

        if (c.JumpHack)
            Set<float>(cm, off::movement::JumpZVelocity, c.JumpValue.load());
        else if (g_Prev.jump && g_MovOrig.captured)
            Set<float>(cm, off::movement::JumpZVelocity, g_MovOrig.jumpZ);

        if (c.NoGravity)
            Set<float>(cm, off::movement::GravityScale, 0.0f);
        else if (g_Prev.nograv && g_MovOrig.captured)
            Set<float>(cm, off::movement::GravityScale, g_MovOrig.gravity);

        if (c.FlyHack)
        {
            Set<uint8_t>(cm, off::movement::MovementMode, off::MOVE_Flying);
            Set<float>(cm, off::movement::MaxFlySpeed, c.SpeedValue.load());
        }
        else if (g_Prev.fly && g_MovOrig.captured)
        {
            // Restore walking mode (game will re-derive on landing; Walking is safe)
            Set<uint8_t>(cm, off::movement::MovementMode, off::MOVE_Walking);
            Set<float>(cm, off::movement::MaxFlySpeed, g_MovOrig.maxFly);
        }

        // Everything off -> drop the snapshot so the next enable re-captures.
        if (!anyMov && g_MovOrig.captured) g_MovOrig.captured = false;

        g_Prev.speed  = c.SpeedHack;
        g_Prev.jump   = c.JumpHack;
        g_Prev.fly    = c.FlyHack;
        g_Prev.nograv = c.NoGravity;
    }

    void ApplyWeapon(const features::Config& c)
    {
        UObject* jodi = game::Jodi();
        if (!jodi) return;
        UObject* w = game::CurrentWeapon(jodi);
        if (!w) return;

        // Weapon switched -> old originals are meaningless, drop them.
        if (g_WpnOrig.captured && g_WpnOrig.weapon != w)
            g_WpnOrig.captured = false;

        const bool anyWpn = c.NoRecoil || c.BigDamage;

        if (anyWpn && !g_WpnOrig.captured)
        {
            g_WpnOrig.weapon      = w;
            g_WpnOrig.recoil      = Get<float>(w, off::gun::recoil);
            g_WpnOrig.shake       = Get<float>(w, off::gun::Shake_Scale);
            g_WpnOrig.extraDamage = Get<float>(w, off::gun::extra_damage);
            g_WpnOrig.curDamage   = Get<float>(w, off::weapon::current_weapon_damage);
            g_WpnOrig.recoilCur   = Get<float>(jodi, off::jodi::recoil_current);
            g_WpnOrig.captured    = true;
        }

        if (c.NoRecoil)
        {
            Set<float>(w, off::gun::recoil, 0.0f);
            Set<float>(w, off::gun::Shake_Scale, 0.0f);
            Set<float>(jodi, off::jodi::recoil_current, 0.0f);
        }
        else if (g_Prev.norecoil && g_WpnOrig.captured && g_WpnOrig.weapon == w)
        {
            Set<float>(w, off::gun::recoil, g_WpnOrig.recoil);
            Set<float>(w, off::gun::Shake_Scale, g_WpnOrig.shake);
        }

        if (c.InfAmmo)
        {
            int32_t mag = Get<int32_t>(w, off::gun::Mag_Size_from_Table);
            if (mag > 0)
            {
                Set<int32_t>(w, off::gun::Current_MagSize, mag);
                Set<int32_t>(w, off::gun::GunData_AmmoInMag, mag);
                Set<bool>   (w, off::gun::has_a_bullet_in_gun, true);
            }
        }
        // Note: no restore needed for ammo — leaving the mag full is benign and
        // the game re-clamps it naturally on the next reload.

        if (c.BigDamage)
        {
            float mult = c.DamageMultiplier.load();
            float base = Get<float>(w, off::weapon::Weapon_Damage);
            Set<float>(w, off::weapon::current_weapon_damage, base * mult);
            Set<float>(w, off::gun::extra_damage, base * (mult - 1.0f));
        }
        else if (g_Prev.bigdmg && g_WpnOrig.captured && g_WpnOrig.weapon == w)
        {
            Set<float>(w, off::weapon::current_weapon_damage, g_WpnOrig.curDamage);
            Set<float>(w, off::gun::extra_damage, g_WpnOrig.extraDamage);
        }

        if (!anyWpn && g_WpnOrig.captured) g_WpnOrig.captured = false;

        g_Prev.norecoil = c.NoRecoil;
        g_Prev.bigdmg   = c.BigDamage;
    }

    void ApplyBackpack(const features::Config& c)
    {
        UObject* jodi = game::Jodi();
        if (!jodi) return;
        UObject* bag = *reinterpret_cast<UObject**>((uintptr_t)jodi + off::cplayer::Bag);
        if (!bag) return;

        const bool wantBig = c.InfBackpack || c.WideBackpack;

        if (wantBig && !g_BagOrig.captured)
        {
            g_BagOrig.weightCap = Get<int32_t>(bag, off::bag::weight_capacity);
            g_BagOrig.captured  = true;
        }

        if (wantBig)
            Set<int32_t>(bag, off::bag::weight_capacity, 999999);
        else if (g_Prev.bigbag && g_BagOrig.captured)
        {
            Set<int32_t>(bag, off::bag::weight_capacity, g_BagOrig.weightCap);
            g_BagOrig.captured = false;
        }

        g_Prev.bigbag = wantBig;
    }

    // ---- SEH-safe helpers (NO C++ locals — C2712) -------------------
    // __try/__except is only legal in functions without object unwinding.
    // Every risky touch of a game object goes through one of these.
    bool ReadEntitySafe(UObject* c, UObject* jodi, const FVector& jodiLoc, features::EntityData& e)
    {
        __try
        {
            e.Actor    = c;
            e.HP       = game::GetHealth(c);
            e.HPMax    = game::GetMaxHealth(c);
            e.Origin   = game::GetActorLocation(c);
            e.IsZombie = game::IsZombie(c);
            e.Dist     = jodiLoc.Dist(e.Origin);
            if (e.IsZombie && e.HP > 0)
            {
                e.Head    = game::GetCharacterHead(c);
                e.Pelvis  = game::GetCharacterPelvis(c);
                if (e.Dist < 5000.f) e.Visible = game::CanISeeThePoint(jodi, e.Head);
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    void XRayOne(UObject* actor, UFunction* fn)
    {
        __try
        {
            struct { bool E; float D; } p{ true, 5.0f };
            actor->ProcessEvent(fn, &p);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    bool KillOne(UObject* actor)
    {
        __try { game::SetHealth(actor, 0.0f); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    void WriteRepRotation(UObject* jodi, const FRotator& r)
    {
        __try { Set<FRotator>(jodi, off::cplayer::Rep_Control_Rotation, r); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void FireOne(UObject* jodi, UFunction* fn)
    {
        __try { struct { bool D; } p{ true }; jodi->ProcessEvent(fn, &p); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // Refresh entity snapshot for ESP + aimbot.
    // Every entity pointer is validated before touching — during level
    // teardown character_List contains dangling pointers and touching them
    // (or PE-calling on them) is what crashed the game at the main menu.
    void RefreshEntities()
    {
        auto list = game::CharacterList();
        UObject* jodi = game::Jodi();
        if (!game::IsValidObj(jodi)) return;
        FVector jodiLoc = game::GetActorLocation(jodi);

        std::vector<features::EntityData> tmp;
        tmp.reserve(list.Count);

        for (int i = 0; i < list.Count; ++i)
        {
            UObject* c = list.Data[i];
            if (!c || c == jodi) continue;
            if (!game::IsValidObj(c)) continue;      // freed actor — skip

            features::EntityData e{};
            if (ReadEntitySafe(c, jodi, jodiLoc, e))
                tmp.push_back(e);
        }
        std::lock_guard<std::mutex> lk(g_EntsMx);
        g_Ents.swap(tmp);
    }

    void ApplyXRay()
    {
        UFunction* fn = sdk::CachedFn("Function Character_Base.Character_Base_C.Set XRay Edge Enabled");
        if (!fn) return;
        std::lock_guard<std::mutex> lk(g_EntsMx);
        for (auto& e : g_Ents)
            if (e.Actor && e.IsZombie && e.HP > 0 && game::IsValidObj(e.Actor))
                XRayOne(e.Actor, fn);
    }

    void ApplyTelekill()
    {
        int killed = 0;
        std::lock_guard<std::mutex> lk(g_EntsMx);
        for (auto& e : g_Ents)
            if (e.Actor && e.IsZombie && e.HP > 0 && game::IsValidObj(e.Actor))
                if (KillOne(e.Actor)) ++killed;
        tka::LogFeat("Telekill: %d zombies HP->0", killed);
    }

    // Aimbot: pick the zombie whose HEAD is closest to screen centre within
    // AimFov px, then write AController::ControlRotation (the field the game
    // actually reads — TargetViewRotation is ignored for local view).
    void ApplyAimbot(const features::Config& c)
    {
        UObject* jodi = game::Jodi();
        if (!game::IsValidObj(jodi)) return;

        FVector camLoc; FRotator camRot; float camFov;
        if (!game::GetCameraPOV(camLoc, camRot, camFov)) return;

        // Use the swapchain backbuffer size (same reference as the ESP
        // renderer) — GetSystemMetrics is wrong for windowed/multi-monitor.
        float sw = 0.f, sh = 0.f;
        hooks::GetScreenSize(sw, sh);
        if (sw < 1.f || sh < 1.f) { sw = (float)GetSystemMetrics(SM_CXSCREEN); sh = (float)GetSystemMetrics(SM_CYSCREEN); }
        const float cx = sw * 0.5f, cy = sh * 0.5f;

        UObject* best = nullptr;
        FVector  bestPos{};
        float    bestScreenDist = c.AimFov;

        {
            std::lock_guard<std::mutex> lk(g_EntsMx);
            for (auto& e : g_Ents)
            {
                if (!e.Actor || !e.IsZombie || e.HP <= 0) continue;
                if (!game::IsValidObj(e.Actor)) continue;
                if (c.AimVisCheck && !e.Visible) continue;

                const FVector& aimPt = c.AimAtHead ? e.Head : e.Pelvis;
                float sx, sy;
                if (!game::WorldToScreen(aimPt, sx, sy, sw, sh)) continue;
                const float sd = sqrtf((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy));
                if (sd < bestScreenDist) { bestScreenDist = sd; best = e.Actor; bestPos = aimPt; }
            }
        }

        if (!best) return;

        FRotator want = LookAt(camLoc, bestPos);
        game::SetControlRotation(want);

        // Mirror to Jodi for animation compensation.
        WriteRepRotation(jodi, want);

        if (c.Triggerbot)
        {
            UFunction* fn = sdk::CachedFn("Function Jodi.Jodi_C.on_input_action_fire");
            if (fn) FireOne(jodi, fn);
        }
    }

    // Log once at startup which UFunctions actually resolved — instant
    // diagnosis if a name is wrong/missing in this build.
    void VerifyFunctions()
    {
        static const char* names[] = {
            "Function Character_Base.Character_Base_C.get head location",
            "Function Character_Base.Character_Base_C.Get Pelvis Location",
            "Function Character_Base.Character_Base_C.Get Predicted Position",
            "Function Character_Base.Character_Base_C.Can I See The Point",
            "Function Character_Base.Character_Base_C.Restore Health",
            "Function Character_Base.Character_Base_C.Change Character Location",
            "Function Character_Base.Character_Base_C.Set XRay Edge Enabled",
            "Function Character_Player_Base.Character_Player_Base_C.Set Invincible Time",
            "Function Jodi.Jodi_C.Restore Vitality",
            "Function Jodi.Jodi_C.on_input_action_fire",
            "Function Jodi.Jodi_C.Got Exp",
            "Function TKA_GameState.TKA_GameState_C.Wake All Zombies Up",
            "Function TKA_GameState.TKA_GameState_C.Come to me Zombies",
            "Function TKA_GameState.TKA_GameState_C.Revive Local Player",
            "Function TKA_GameState.TKA_GameState_C.Change Game Difficulty",
            "Function TKA_GameState.TKA_GameState_C.Show UI",
            "Function TKA_GameState.TKA_GameState_C.Change View Distance Scale",
        };
        int missing = 0;
        for (auto* n : names)
            if (!sdk::CachedFn(n)) { tka::LogErr("UFunction NOT FOUND: %s", n); ++missing; }
        if (!missing) tka::LogOk("all %d UFunctions resolved", (int)(sizeof(names)/sizeof(names[0])));
        else          tka::LogErr("%d UFunctions missing — those features will be dead", missing);
    }

    void ApplyDifficulty(features::Config& c)
    {
        int d = c.Difficulty.exchange(-1);
        if (d >= 0) game::ChangeGameDifficulty(d);
    }

    void ApplyLocks(features::Config& c)
    {
        if (c.UnlockAllLocks.exchange(false))
        {
            // A well-known master key name would go here.  Without knowing the FName
            // ID, this call is a stub — user must supply the actual key FName.
        }
    }

    // ---- Main loop --------------------------------------------------
    // Tick body lives in LoopTick (may use C++ objects); the SEH wrapper
    // GuardedTick has NO C++ locals so __try is legal there (C2712).
    UObject* g_LastGS = nullptr;
    bool     g_Verified = false;

    void LoopTick()
    {
        auto& c = g_Cfg;
        static int tick = 0;

        UObject* world = game::World();
        UObject* gs    = game::GameState();

        if (!world || !gs || !game::IsValidObj(game::Jodi()))
        {
            // Main menu / loading — clear snapshot so nothing stale is drawn.
            {
                std::lock_guard<std::mutex> lk(g_EntsMx);
                if (!g_Ents.empty()) g_Ents.clear();
            }
            g_LastGS = nullptr;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return;
        }

        // Map change: GameState instance changed -> drop stale data,
        // snapshots and give the engine a moment before touching actors.
        if (gs != g_LastGS)
        {
            g_LastGS = gs;
            {
                std::lock_guard<std::mutex> lk(g_EntsMx);
                g_Ents.clear();
            }
            g_MovOrig.captured = false;
            g_WpnOrig.captured = false;
            g_WpnOrig.weapon   = nullptr;
            g_BagOrig.captured = false;
            tka::LogFeat("new GameState %p — snapshots reset", (void*)gs);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            return;
        }

        if (!g_Verified) { g_Verified = true; VerifyFunctions(); }

        RefreshEntities();

        if (c.GodMode)      ApplyGod();
        if (c.InfVitality)  ApplyVitality();
                            ApplyMovement(c);
                            ApplyWeapon(c);
                            ApplyBackpack(c);
        if (c.XRayZombies && (tick % 15) == 0) ApplyXRay();
        if (c.Telekill.exchange(false)) ApplyTelekill();
        if (c.Aimbot)       ApplyAimbot(c);
                            ApplyDifficulty(c);
                            ApplyLocks(c);
        ++tick;
    }

    void GuardedTick()
    {
        __try { LoopTick(); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            tka::LogErr("feature loop: exception caught — tick skipped (likely level teardown)");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    void Loop()
    {
        while (g_Running.load())
        {
            GuardedTick();
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30Hz
        }
    }
}

namespace features
{
    Config& GetConfig() { return g_Cfg; }

    const std::vector<EntityData>& GetEntities() { return g_Ents; }

    std::vector<EntityData> GetEntitiesCopy()
    {
        std::lock_guard<std::mutex> lk(g_EntsMx);
        return g_Ents;   // copy under lock — safe for render-thread iteration
    }

    void Start()
    {
        if (g_Running.exchange(true)) return;
        g_Thread = std::thread(Loop);
    }

    // ---- SEH-safe restore helpers (no C++ locals) --------------------
    void RestoreMovementSafe(UObject* jodi)
    {
        __try
        {
            if (!game::IsValidObj(jodi)) return;
            UObject* cm = *reinterpret_cast<UObject**>((uintptr_t)jodi + off::character::CharacterMovement);
            if (!cm) return;
            Set<float>(cm, off::movement::MaxWalkSpeed,        g_MovOrig.maxWalk);
            Set<float>(cm, off::movement::MaxWalkSpeedCrouched, g_MovOrig.maxWalkCrouch);
            Set<float>(cm, off::movement::MaxSwimSpeed,        g_MovOrig.maxSwim);
            Set<float>(cm, off::movement::MaxFlySpeed,         g_MovOrig.maxFly);
            Set<float>(cm, off::movement::JumpZVelocity,       g_MovOrig.jumpZ);
            Set<float>(cm, off::movement::GravityScale,        g_MovOrig.gravity);
            Set<float>(cm, off::movement::MaxAcceleration,     g_MovOrig.accel);
            Set<float>(cm, off::movement::AirControl,          g_MovOrig.airControl);
            Set<uint8_t>(cm, off::movement::MovementMode,      off::MOVE_Walking);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void RestoreWeaponSafe(UObject* w)
    {
        __try
        {
            if (!game::IsValidObj(w)) return;
            Set<float>(w, off::gun::recoil,                   g_WpnOrig.recoil);
            Set<float>(w, off::gun::Shake_Scale,              g_WpnOrig.shake);
            Set<float>(w, off::gun::extra_damage,             g_WpnOrig.extraDamage);
            Set<float>(w, off::weapon::current_weapon_damage, g_WpnOrig.curDamage);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void RestoreBackpackSafe(UObject* jodi)
    {
        __try
        {
            if (!game::IsValidObj(jodi)) return;
            UObject* bag = *reinterpret_cast<UObject**>((uintptr_t)jodi + off::cplayer::Bag);
            if (!bag) return;
            Set<int32_t>(bag, off::bag::weight_capacity, g_BagOrig.weightCap);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void RestoreAimRotationSafe(UObject* jodi, UObject* pc)
    {
        __try
        {
            if (!game::IsValidObj(jodi) || !game::IsValidObj(pc)) return;
            FRotator cur = Get<FRotator>(pc, off::pc::TargetViewRotation);
            Set<FRotator>(jodi, off::cplayer::Rep_Control_Rotation, cur);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void RestoreAll()
    {
        // Idempotent. Safe to call when snapshots were never taken.
        // Called from the ENTRY thread during unload — NOT inside GuardedTick —
        // so every touch goes through an SEH-safe helper.
        UObject* jodi = game::Jodi();
        bool restoredAny = false;

        if (g_MovOrig.captured && jodi)
        {
            RestoreMovementSafe(jodi);
            restoredAny = true;
            g_MovOrig.captured = false;
        }

        if (g_WpnOrig.captured && g_WpnOrig.weapon)
        {
            RestoreWeaponSafe(reinterpret_cast<UObject*>(g_WpnOrig.weapon));
            restoredAny = true;
            g_WpnOrig.captured = false;
            g_WpnOrig.weapon   = nullptr;
        }

        if (g_BagOrig.captured && jodi)
        {
            RestoreBackpackSafe(jodi);
            restoredAny = true;
            g_BagOrig.captured = false;
        }

        // Aimbot may have left a forced view rotation; snap it back to the
        // controller's current rotation so the camera doesn't stay tilted.
        if (jodi)
            RestoreAimRotationSafe(jodi, game::PlayerController());

        g_Prev = Prev{};
        if (restoredAny) tka::LogFeat("RestoreAll: movement/weapon/backpack values restored");
        else             tka::LogFeat("RestoreAll: nothing was modified");
    }

    void Stop()
    {
        g_Running.store(false);
        if (g_Thread.joinable()) g_Thread.join();
        RestoreAll();
    }
}

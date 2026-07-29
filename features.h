#pragma once
#include "sdk_types.h"
#include <atomic>
#include <vector>

namespace features
{
    struct Config
    {
        std::atomic<bool> GodMode{ false };          // freezes health to max, keeps Set_Invincible_Time fresh
        std::atomic<bool> InfVitality{ false };
        std::atomic<bool> InfAmmo{ false };
        std::atomic<bool> NoRecoil{ false };
        std::atomic<bool> SpeedHack{ false };
        std::atomic<float> SpeedValue{ 1200.0f };    // MaxWalkSpeed target
        std::atomic<bool> JumpHack{ false };
        std::atomic<float> JumpValue{ 900.0f };      // JumpZVelocity
        std::atomic<bool> FlyHack{ false };          // sets MovementMode=Flying + MaxFlySpeed
        std::atomic<bool> NoGravity{ false };        // GravityScale=0
        std::atomic<bool> InfBackpack{ false };
        std::atomic<bool> WideBackpack{ false };
        std::atomic<bool> BigDamage{ false };        // writes current_weapon_damage
        std::atomic<float> DamageMultiplier{ 100.0f };
        std::atomic<bool> XRayZombies{ false };      // set_XRay_Edge on each zombie
        std::atomic<bool> Aimbot{ false };
        std::atomic<float> AimFov{ 200.0f };         // screen-space radius (px)
        std::atomic<bool> AimVisCheck{ true };       // use Can_I_See_The_Point
        std::atomic<bool> AimAtHead{ true };
        std::atomic<bool> Triggerbot{ false };       // fire when target in crosshair
        std::atomic<bool> Telekill{ false };         // manual: set zombies HP=0

        std::atomic<int>  Difficulty{ -1 };          // set once via UI/hotkey
        std::atomic<bool> UnlockAllLocks{ false };   // trigger-once (auto-reset)
    };

    // ESP snapshot — refilled every tick by feature loop; menu / renderer reads this.
    struct EntityData
    {
        UObject* Actor = nullptr;
        FVector  Head{};
        FVector  Pelvis{};
        FVector  Origin{};
        float    HP = 0, HPMax = 0;
        float    Dist = 0;
        bool     IsZombie = false;
        bool     Visible  = false;    // whether local player can see them
    };

    Config& GetConfig();

    // Snapshot access. Do NOT hold the reference across frames — the feature
    // thread swaps the internal vector every tick. Use the copy version.
    const std::vector<EntityData>& GetEntities();
    std::vector<EntityData>        GetEntitiesCopy();

    // Start background feature loop (own thread).
    void Start();
    // Stop loop thread (joins) and restore every modified value back to the
    // originals captured when each feature was first enabled.
    void Stop();
    // Restore-only (idempotent). Called by Stop() and by hooks on unload.
    void RestoreAll();
}

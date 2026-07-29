#pragma once
#include "sdk.h"
#include "sdk_types.h"
#include "offsets.h"

// Field accessor — reads T at (base+offset). Returns default T() if base is null.
template<typename T>
inline T Get(void* base, uintptr_t offset)
{
    if (!base) return T{};
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
}

template<typename T>
inline T* Ptr(void* base, uintptr_t offset)
{
    if (!base) return nullptr;
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
}

template<typename T>
inline void Set(void* base, uintptr_t offset, const T& value)
{
    if (!base) return;
    *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset) = value;
}

namespace game
{
    // === World / State chain ===
    UObject* World();
    UObject* GameInstance();
    UObject* GameState();       // ATKA_GameState_C
    UObject* LocalPlayer();     // ULocalPlayer (engine)
    UObject* PlayerController();// ATKA_Controller_C
    UObject* Jodi();            // AJodi_C — local player pawn
    UObject* Camera();          // UCameraComponent on Jodi
    UObject* CameraManager();   // APlayerCameraManager (PC+0x2B8)

    // Heuristic liveness check: non-null, 8-aligned, vtable inside game image.
    // NOT a guarantee — but filters dangling pointers left by level teardown.
    bool IsValidObj(const UObject* obj);

    // Camera POV from APlayerCameraManager::CameraCachePrivate (evidence-based).
    bool GetCameraPOV(FVector& loc, FRotator& rot, float& fov);
    // Project world point to screen. Returns false when behind camera / invalid.
    bool WorldToScreen(const FVector& world, float& sx, float& sy, float sw, float sh);

    // Aim: writes AController::ControlRotation (0x288) AND calls
    // SetControlRotation via ProcessEvent. This is the rotation the game uses.
    void SetControlRotation(const FRotator& rot);

    // === Character list (all characters incl. zombies) ===
    TArray<UObject*> CharacterList();

    // Convenience — check "is player" by simple heuristic (Jodi ptr equality).
    bool IsLocalPlayer(UObject* ch);
    bool IsZombie(UObject* ch);       // uses is_Npc__ ufunction (cached)
    bool IsAlive(UObject* ch);

    // === World -> Screen (via camera manager pathway) ===
    FVector    GetActorLocation(UObject* actor);
    FVector    GetCharacterHead(UObject* ch);       // via get_head_location UFn
    FVector    GetCharacterPelvis(UObject* ch);
    FVector    GetPredictedPosition(UObject* ch, float time);
    FRotator   GetControlRotation();
    FVector    GetCameraLocation();

    // ---- UFunction wrappers ----
    void  SetInvincibleTime(float sec);
    void  RestoreHealth(float ratio);
    void  RestoreVitality(float ratio);
    void  WakeAllZombies();
    void  ComeToMeZombies();
    void  MarkZombies(float radius, const FVector& location, float duration);
    void  ReviveLocalPlayer();
    bool  CanISeeThePoint(UObject* ch, const FVector& p);
    void  ChangeCharacterLocation(UObject* ch, const FVector& newLoc, const FRotator& newRot);
    void  ChangeGameDifficulty(int diff);
    void  ShowUI(bool show);
    void  ChangeViewDistanceScale(float scale);
    void  MarkAllLocksWithKey(const FName& key);
    void  DoubleMoney(UObject* bagComp);
    void  UpgradeWeaponDamage(const FName& weapon, float additive);
    void  UpgradeWeaponAttribute(const FName& weapon, float additive);
    void  GotExp(float exp);
    void  TryLeftHandMeleeAttack();
    void  NotifyMeleeAttack(float scale);

    // Helpers that write memory directly (no ProcessEvent needed).
    float GetHealth(UObject* ch);
    float GetMaxHealth(UObject* ch);
    void  SetHealth(UObject* ch, float v);
    void  SetMaxHealth(UObject* ch, float v);

    UObject* CurrentWeapon(UObject* jodi);   // Character_Player_Base_C::current_weapon
    UObject* InHandWeapon(UObject* jodi);
}

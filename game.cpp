#include "game.h"
#include "offsets.h"
#include "sdk.h"
#include "log.h"
#include <cstring>
#include <cmath>

namespace {
    uintptr_t g_ImageBase = 0;
    uintptr_t g_ImageSize = 0;

    UObject* ReadPtr(void* base, uintptr_t off)
    {
        if (!base) return nullptr;
        return *reinterpret_cast<UObject**>(reinterpret_cast<uintptr_t>(base) + off);
    }

    void InitImageRange()
    {
        if (g_ImageBase) return;
        g_ImageBase = sdk::ImageBase();
        auto dos = (PIMAGE_DOS_HEADER)g_ImageBase;
        if (dos && dos->e_magic == IMAGE_DOS_SIGNATURE)
        {
            auto nt = (PIMAGE_NT_HEADERS)(g_ImageBase + dos->e_lfanew);
            g_ImageSize = nt->OptionalHeader.SizeOfImage;
        }
    }

    inline bool InImage(uintptr_t p)
    {
        return g_ImageSize && p >= g_ImageBase && p < g_ImageBase + g_ImageSize;
    }

    template<typename Params>
    void Call(const char* fnName, UObject* self, Params& p)
    {
        if (!self || !game::IsValidObj(self)) return;
        UFunction* fn = sdk::CachedFn(fnName);
        if (!fn) return;
        __try { self->ProcessEvent(fn, &p); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

namespace game
{
    bool IsValidObj(const UObject* obj)
    {
        InitImageRange();
        if (!obj) return false;
        const uintptr_t p = (uintptr_t)obj;
        if (p & 0x7) return false;                       // alignment
        __try
        {
            const uintptr_t vt = *(uintptr_t*)p;
            if (!InImage(vt)) return false;              // vtable in image
            const UObject* cls = obj->ClassPrivate;
            if (!cls) return false;
            const uintptr_t cp = (uintptr_t)cls;
            if (cp & 0x7) return false;
            if (!InImage(*(uintptr_t*)cp)) return false; // class vtable in image
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    UObject* CameraManager()
    {
        return ReadPtr(PlayerController(), off::pc::PlayerCameraManager);
    }

    bool GetCameraPOV(FVector& loc, FRotator& rot, float& fov)
    {
        UObject* pcm = CameraManager();
        if (!IsValidObj(pcm)) return false;
        __try
        {
            loc = Get<FVector>(pcm, off::cam::POV_Location);
            rot = Get<FRotator>(pcm, off::cam::POV_Rotation);
            fov = Get<float>(pcm, off::cam::POV_FOV);
            return fov > 1.0f && fov < 179.0f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    bool WorldToScreen(const FVector& world, float& sx, float& sy, float sw, float sh)
    {
        FVector camLoc; FRotator camRot; float fov;
        if (!GetCameraPOV(camLoc, camRot, fov)) return false;

        const float P = camRot.Pitch * (3.14159265f / 180.f);
        const float Y = camRot.Yaw   * (3.14159265f / 180.f);
        const float R = camRot.Roll  * (3.14159265f / 180.f);
        const float SP = sinf(P), CP = cosf(P);
        const float SY = sinf(Y), CY = cosf(Y);
        const float SR = sinf(R), CR = cosf(R);

        // UE axes from FRotator
        const FVector fwd ( CP * CY,              CP * SY,             SP      );
        const FVector right( SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP );
        const FVector up  ( -(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP );

        const FVector d = world - camLoc;
        const float z = d.Dot(fwd);
        if (z < 1.0f) return false;

        const float x = d.Dot(right);
        const float y = d.Dot(up);
        const float f = (sw * 0.5f) / tanf(fov * 3.14159265f / 360.f);
        sx = (sw  * 0.5f) + x * f / z;
        sy = (sh * 0.5f) - y * f / z;
        return true;
    }

    void SetControlRotation(const FRotator& rot)
    {
        UObject* pc = PlayerController();
        if (!IsValidObj(pc)) return;
        // Field write — instant, no PE scheduling needed.
        __try
        {
            Set<FRotator>(pc, off::controller::ControlRotation, rot);
            Set<FRotator>(pc, off::pc::TargetViewRotation, rot);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    UObject* World()
    {
        // Use GWorld ptr since SDK-provided UWorld::GetWorld isn't linked in.
        auto pp = (UObject**)(sdk::ImageBase() + off::GWorld);
        return pp ? *pp : nullptr;
    }

    UObject* GameInstance() { return ReadPtr(World(),         off::world::OwningGameInstance); }
    UObject* GameState()    { return ReadPtr(World(),         off::world::GameState);          }
    UObject* Jodi()         { return ReadPtr(GameState(),     off::gs::LocalPlayer);           }
    UObject* Camera()       { return ReadPtr(Jodi(),          off::jodi::Camera);              }

    UObject* PlayerController()
    {
        // Preferred path: Jodi->Player_Controller (already TKA_Controller_C*).
        UObject* pc = ReadPtr(Jodi(), off::jodi::Player_Controller);
        if (pc) return pc;
        // Fallback: APawn::Controller.
        return ReadPtr(Jodi(), off::pawn::Controller);
    }

    UObject* LocalPlayer()
    {
        UObject* gi = GameInstance();
        if (!gi) return nullptr;
        auto arr = *reinterpret_cast<TArray<UObject*>*>(reinterpret_cast<uintptr_t>(gi) + off::gameinstance::LocalPlayers);
        return (arr.Count > 0) ? arr.Data[0] : nullptr;
    }

    TArray<UObject*> CharacterList()
    {
        UObject* gs = GameState();
        if (!gs) return {};
        return *reinterpret_cast<TArray<UObject*>*>(reinterpret_cast<uintptr_t>(gs) + off::gs::character_List);
    }

    bool IsLocalPlayer(UObject* ch) { return ch && ch == Jodi(); }

    bool IsAlive(UObject* ch)
    {
        if (!ch) return false;
        float hp = GetHealth(ch);
        return hp > 0.0f && hp < 100000.0f; // sanity clamp
    }

    bool IsZombie(UObject* ch)
    {
        if (!ch || ch == Jodi()) return false;
        // Cheap: NPC objects have `is_Npc__` UFn. Cheaper: check class name.
        if (!ch->ClassPrivate) return false;
        std::string cn = ch->ClassPrivate->GetName();
        return cn.find("Zombie") != std::string::npos;
    }

    FVector GetActorLocation(UObject* actor)
    {
        if (!actor) return {};
        UObject* root = ReadPtr(actor, off::actor::RootComponent);
        if (!root) return {};
        return Get<FVector>(root, off::scene::RelativeLocation);
    }

    struct Head_Params { FVector Loc; };
    FVector GetCharacterHead(UObject* ch)
    {
        Head_Params p{};
        Call("Function Character_Base.Character_Base_C.get head location", ch, p);
        return p.Loc;
    }

    FVector GetCharacterPelvis(UObject* ch)
    {
        Head_Params p{};
        Call("Function Character_Base.Character_Base_C.Get Pelvis Location", ch, p);
        return p.Loc;
    }

    struct Predict_Params { float Time; FVector Pos; };
    FVector GetPredictedPosition(UObject* ch, float time)
    {
        Predict_Params p{ time, {} };
        Call("Function Character_Base.Character_Base_C.Get Predicted Position", ch, p);
        return p.Pos;
    }

    FRotator GetControlRotation()
    {
        UObject* pc = PlayerController();
        if (!pc) return {};
        __try { return Get<FRotator>(pc, off::controller::ControlRotation); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return {}; }
    }

    FVector GetCameraLocation()
    {
        FVector loc; FRotator rot; float fov;
        if (GetCameraPOV(loc, rot, fov)) return loc;
        return GetActorLocation(Jodi());
    }

    // ------ UFunction wrappers ------
    struct FloatP { float v; };
    struct BoolP  { bool  v; };
    struct IntP   { int32_t v; };
    struct NameP  { FName n; };
    struct VecP   { FVector v; };

    void SetInvincibleTime(float sec)
    {
        FloatP p{ sec };
        Call("Function Character_Player_Base.Character_Player_Base_C.Set Invincible Time", Jodi(), p);
    }

    void RestoreHealth(float ratio)
    {
        FloatP p{ ratio };
        Call("Function Character_Base.Character_Base_C.Restore Health", Jodi(), p);
    }

    void RestoreVitality(float ratio)
    {
        FloatP p{ ratio };
        Call("Function Jodi.Jodi_C.Restore Vitality", Jodi(), p);
    }

    void WakeAllZombies()
    {
        struct{} p{};
        Call("Function TKA_GameState.TKA_GameState_C.Wake All Zombies Up", GameState(), p);
    }

    void ComeToMeZombies()
    {
        struct { UObject* Char; } p{ Jodi() };
        Call("Function TKA_GameState.TKA_GameState_C.Come to me Zombies", GameState(), p);
    }

    void MarkZombies(float radius, const FVector& loc, float duration)
    {
        struct { float R; FVector L; float D; } p{ radius, loc, duration };
        Call("Function TKA_GameState.TKA_GameState_C.Mark Zombies", GameState(), p);
    }

    void ReviveLocalPlayer()
    {
        struct{} p{};
        Call("Function TKA_GameState.TKA_GameState_C.Revive Local Player", GameState(), p);
    }

    bool CanISeeThePoint(UObject* ch, const FVector& p)
    {
        struct { FVector Pos; bool Yes; } q{ p, false };
        Call("Function Character_Base.Character_Base_C.Can I See The Point", ch, q);
        return q.Yes;
    }

    void ChangeCharacterLocation(UObject* ch, const FVector& l, const FRotator& r)
    {
        struct { FVector L; FRotator R; } p{ l, r };
        Call("Function Character_Base.Character_Base_C.Change Character Location", ch, p);
    }

    void ChangeGameDifficulty(int diff)
    {
        IntP p{ diff };
        Call("Function TKA_GameState.TKA_GameState_C.Change Game Difficulty", GameState(), p);
    }

    void ShowUI(bool show)
    {
        BoolP p{ show };
        Call("Function TKA_GameState.TKA_GameState_C.Show UI", GameState(), p);
    }

    void ChangeViewDistanceScale(float s)
    {
        FloatP p{ s };
        Call("Function TKA_GameState.TKA_GameState_C.Change View Distance Scale", GameState(), p);
    }

    void MarkAllLocksWithKey(const FName& key)
    {
        NameP p{ key };
        Call("Function TKA_GameState.TKA_GameState_C.Mark_All_Locks_with_Key", GameState(), p);
    }

    void DoubleMoney(UObject* bagComp)
    {
        struct{} p{};
        Call("Function Bag_Comp.Bag_Comp_C.Double Money", bagComp, p);
    }

    void UpgradeWeaponDamage(const FName& w, float add)
    {
        struct { FName N; float A; } p{ w, add };
        Call("Function Jodi.Jodi_C.Upgrade Weapon Damage", Jodi(), p);
    }

    void UpgradeWeaponAttribute(const FName& w, float add)
    {
        struct { FName N; float A; } p{ w, add };
        Call("Function Jodi.Jodi_C.Upgrade Weapon Attribute", Jodi(), p);
    }

    void GotExp(float exp)
    {
        FloatP p{ exp };
        Call("Function Jodi.Jodi_C.Got Exp", Jodi(), p);
    }

    void TryLeftHandMeleeAttack()
    {
        struct{} p{};
        Call("Function Jodi.Jodi_C.Try Left Hand Melee Attack", Jodi(), p);
    }

    void NotifyMeleeAttack(float scale)
    {
        struct { float S; FName Type; bool Pull; bool Rel; bool FX; }
            p{ scale, FName{}, true, true, true };
        Call("Function Jodi.Jodi_C.Notify Melee Attack", Jodi(), p);
    }

    float GetHealth(UObject* ch)    { return Get<float>(ch, off::cbase::health); }
    float GetMaxHealth(UObject* ch) { return Get<float>(ch, off::cbase::Health_Max); }
    void  SetHealth(UObject* ch, float v)    { Set<float>(ch, off::cbase::health, v); }
    void  SetMaxHealth(UObject* ch, float v) { Set<float>(ch, off::cbase::Health_Max, v); }

    UObject* CurrentWeapon(UObject* jodi) { return ReadPtr(jodi, off::cplayer::current_weapon); }
    UObject* InHandWeapon(UObject* jodi)  { return ReadPtr(jodi, off::cplayer::in_hand_weapon); }
}

#pragma once
#include <cstdint>

// TheKillingAntidote — UE4 4.27.2-18319896 (+++UE4+Release-4.27)
// All values relative to game module base (GetModuleHandleW(nullptr)).
// Source: Dumper-7 CppSDK/SDK/Basic.hpp + *_classes.hpp
namespace off
{
    // ---- Global (rel. to image base) ----
    constexpr uintptr_t GObjects         = 0x04D0C690; // TUObjectArray
    constexpr uintptr_t GNames           = 0x04CD0340; // fallback; prefer AppendString
    constexpr uintptr_t GWorld           = 0x04E53DB0;
    constexpr uintptr_t AppendString     = 0x00EAC7A0; // FName::AppendString(FString&)
    constexpr uintptr_t ProcessEvent     = 0x0109DBC0; // UObject::ProcessEvent(UFunction*, void*)
    constexpr int       ProcessEventIdx  = 0x44;       // vtable index (fallback path)

    // ---- UWorld ----
    namespace world
    {
        constexpr uintptr_t PersistentLevel    = 0x0030;
        constexpr uintptr_t AuthorityGameMode  = 0x0118;
        constexpr uintptr_t GameState          = 0x0120; // -> ATKA_GameState_C*
        constexpr uintptr_t Levels             = 0x0138;
        constexpr uintptr_t OwningGameInstance = 0x0180;
    }

    // ---- UGameInstance ----
    namespace gameinstance
    {
        constexpr uintptr_t LocalPlayers = 0x0038; // TArray<ULocalPlayer*>
    }

    // ---- ULocalPlayer / UPlayer ----
    namespace localplayer
    {
        constexpr uintptr_t PlayerController = 0x0030; // UPlayer::PlayerController (base)
        constexpr uintptr_t ViewportClient   = 0x0070;
    }

    // ---- AController (parent of APlayerController) ----
    namespace controller
    {
        constexpr uintptr_t ControlRotation = 0x0288; // FRotator (Engine_classes.hpp:285)
    }

    // ---- APlayerController ----
    namespace pc
    {
        constexpr uintptr_t Player              = 0x0298;
        constexpr uintptr_t AcknowledgedPawn    = 0x02A0;
        constexpr uintptr_t MyHUD               = 0x02B0;
        constexpr uintptr_t PlayerCameraManager = 0x02B8;
        constexpr uintptr_t HiddenActors        = 0x02F0;
        constexpr uintptr_t TargetViewRotation  = 0x02CC; // FRotator (Net)
        constexpr uintptr_t CheatManager        = 0x0338;
        constexpr uintptr_t PlayerInput         = 0x0348;
        constexpr uintptr_t InputYawScale       = 0x042C;
        constexpr uintptr_t InputPitchScale     = 0x0430;
        constexpr uintptr_t bShowMouseCursor    = 0x0438; // bit 0
    }

    // ---- APlayerCameraManager (PC+0x2B8) ----
    namespace cam
    {
        constexpr uintptr_t CameraCachePrivate = 0x1AE0; // FCameraCacheEntry
        // FCameraCacheEntry: Timestamp@0x00, POV@0x10
        // FMinimalViewInfo:  Location@0x00, Rotation@0x0C, FOV@0x18
        constexpr uintptr_t POV_Location = CameraCachePrivate + 0x10 + 0x00;
        constexpr uintptr_t POV_Rotation = CameraCachePrivate + 0x10 + 0x0C;
        constexpr uintptr_t POV_FOV      = CameraCachePrivate + 0x10 + 0x18;
    }

    // ---- AActor ----
    namespace actor
    {
        constexpr uintptr_t Owner         = 0x00E0;
        constexpr uintptr_t Instigator    = 0x0118;
        constexpr uintptr_t Tags          = 0x0170;
        constexpr uintptr_t RootComponent = 0x0130;
    }

    // ---- USceneComponent (RootComponent) ----
    namespace scene
    {
        constexpr uintptr_t RelativeLocation = 0x011C; // FVector
        constexpr uintptr_t RelativeRotation = 0x0128; // FRotator
        constexpr uintptr_t ComponentVelocity = 0x0140; // FVector
    }

    // ---- APawn ----
    namespace pawn
    {
        constexpr uintptr_t Controller         = 0x0258;
        constexpr uintptr_t PlayerState        = 0x0240;
        constexpr uintptr_t ControlInputVector = 0x0264; // FVector
    }

    // ---- ACharacter ----
    namespace character
    {
        constexpr uintptr_t Mesh              = 0x0280; // USkeletalMeshComponent*
        constexpr uintptr_t CharacterMovement = 0x0288;
        constexpr uintptr_t CapsuleComponent  = 0x0290;
        constexpr uintptr_t bIsCrouched_byte  = 0x0330; // bit 0
        constexpr uintptr_t JumpMaxHoldTime   = 0x0340;
        constexpr uintptr_t JumpMaxCount      = 0x0344;
    }

    // ---- UCharacterMovementComponent ----
    namespace movement
    {
        constexpr uintptr_t GravityScale         = 0x0150;
        constexpr uintptr_t JumpZVelocity        = 0x0158;
        constexpr uintptr_t MovementMode         = 0x0168; // uint8 EMovementMode
        constexpr uintptr_t GroundFriction       = 0x016C;
        constexpr uintptr_t MaxWalkSpeed         = 0x018C;
        constexpr uintptr_t MaxWalkSpeedCrouched = 0x0190;
        constexpr uintptr_t MaxSwimSpeed         = 0x0194;
        constexpr uintptr_t MaxFlySpeed          = 0x0198;
        constexpr uintptr_t MaxAcceleration      = 0x01A0;
        constexpr uintptr_t AirControl           = 0x01C4;
    }

    // EMovementMode values
    enum EMovementMode : uint8_t
    {
        MOVE_None = 0, MOVE_Walking = 1, MOVE_NavWalking = 2, MOVE_Falling = 3,
        MOVE_Swimming = 4, MOVE_Flying = 5, MOVE_Custom = 6,
    };

    // ---- USkeletalMeshComponent (for bone matrices) ----
    namespace mesh
    {
        constexpr uintptr_t ComponentToWorld = 0x01C0; // FTransform (typical UE4.27)
        constexpr uintptr_t BoneArray        = 0x05C0; // CachedBoneSpaceTransforms; may need +0x10
    }

    // ============ TKA game classes ============

    // ACharacter_Base_C  (size 0x6D0)
    namespace cbase
    {
        constexpr uintptr_t health          = 0x0504; // float (SaveGame)
        constexpr uintptr_t Health_Max      = 0x0560;
        constexpr uintptr_t Real_Velocity   = 0x04F4; // FVector
        constexpr uintptr_t Speed_2d        = 0x0584;
        constexpr uintptr_t hit_position    = 0x0564; // FVector
        constexpr uintptr_t Alignment       = 0x0594; // EAlignmentMode (uint8)
        constexpr uintptr_t TKA_Game_State  = 0x05D0;
        constexpr uintptr_t Leg_Broken      = 0x05E0; // bool
        constexpr uintptr_t Arms_Broken     = 0x0638; // int32
        constexpr uintptr_t Head_Cut        = 0x06BC; // bool
        constexpr uintptr_t Loading_State   = 0x06C8; // int32
    }

    // ACharacter_Player_Base_C  (size 0x9F0)
    namespace cplayer
    {
        constexpr uintptr_t Bag             = 0x06D8;
        constexpr uintptr_t current_weapon  = 0x0780;
        constexpr uintptr_t Weapons         = 0x0788; // TMap<FName, AWeapon*>
        constexpr uintptr_t in_hand_weapon  = 0x08B8;
        constexpr uintptr_t Rep_Control_Rotation = 0x094C; // FRotator
        constexpr uintptr_t is_Aim_View     = 0x08CC;
        constexpr uintptr_t Wanna_Aim       = 0x08CD;
        constexpr uintptr_t Fire_Button_Holding = 0x08D9;
        constexpr uintptr_t Invincible_Time = 0x0994;
        constexpr uintptr_t GasMask_Persent = 0x09E0;
    }

    // AJodi_C  (size 0xE40)
    namespace jodi
    {
        constexpr uintptr_t HandPosition        = 0x0A20;
        constexpr uintptr_t GunPosition         = 0x0A28;
        constexpr uintptr_t FirstPersonCamera   = 0x0A30;
        constexpr uintptr_t Camera              = 0x0A40; // UCameraComponent*
        constexpr uintptr_t SpringArm           = 0x0A48;
        constexpr uintptr_t Player_Controller   = 0x0A58; // ATKA_Controller_C*
        constexpr uintptr_t View_Sensitivity    = 0x0A70;
        constexpr uintptr_t Weapon_Damage_Additive    = 0x0AB0; // TMap
        constexpr uintptr_t Weapon_Attribute_Additive = 0x0B00;
        constexpr uintptr_t crosshair           = 0x0B60;
        constexpr uintptr_t Flashlight          = 0x0B90;
        constexpr uintptr_t vitality            = 0x0BA4;
        constexpr uintptr_t vitality_timer      = 0x0BA8;
        constexpr uintptr_t Vitality_Max        = 0x0BAC;
        constexpr uintptr_t Vitality_Restore    = 0x0BB0;
        constexpr uintptr_t Health_Restore      = 0x0BB8;
        constexpr uintptr_t Restore_Health_Value = 0x0BC0;
        constexpr uintptr_t Exp                 = 0x0BD0;
        constexpr uintptr_t Exp_Max             = 0x0BD4;
        constexpr uintptr_t Exp_Multiple        = 0x0BD8;
        constexpr uintptr_t count_killed_zombies = 0x0C78; // int32 Net
        constexpr uintptr_t backpack_capacity   = 0x0CF8;
        constexpr uintptr_t Rep_Move_Input      = 0x0D70; // FVector2D
        constexpr uintptr_t Rep_Max_Move_Speed  = 0x0D78;
        constexpr uintptr_t Input_Enabled       = 0x0D7C;
        constexpr uintptr_t Look_View_Input     = 0x0D68; // FVector2D
        constexpr uintptr_t recoil_last_time    = 0x0DBC;
        constexpr uintptr_t recoil_current      = 0x0DC0;
        constexpr uintptr_t fov_for_focus       = 0x0DF0;
        constexpr uintptr_t is_First_Person_View = 0x0DF6;
        constexpr uintptr_t Aiming_Button_Down  = 0x0DF7;
    }

    // AZombie_Base_C  (size 0x780)
    namespace zombie
    {
        constexpr uintptr_t ZombieState     = 0x06D8; // uint8
        constexpr uintptr_t AI_Mode         = 0x06D9; // uint8
        constexpr uintptr_t Ai_Controller   = 0x06E8;
        constexpr uintptr_t can_see_target  = 0x06F4;
        constexpr uintptr_t spawn_in_runtime = 0x06F5;
        constexpr uintptr_t Crystal         = 0x06F8;
        constexpr uintptr_t Revivable       = 0x0700;
        constexpr uintptr_t Patrol_Location = 0x0710; // FVector
        constexpr uintptr_t Spawn_Location  = 0x0744; // FVector
        constexpr uintptr_t is_target_visible = 0x0760;
        constexpr uintptr_t Exp             = 0x0764;
        constexpr uintptr_t Survivor        = 0x0768;
        constexpr uintptr_t Loot_Name       = 0x0770; // FName
    }

    // AZombie_Controller_C  (size 0x600)
    namespace zctrl
    {
        constexpr uintptr_t ZombieAttr           = 0x04F0; // FZombieAttributesStruct (0x40)
        constexpr uintptr_t attr_Health          = 0x04F0 + 0x18;
        constexpr uintptr_t attr_AttackDamage    = 0x04F0 + 0x1C;
        constexpr uintptr_t attr_HyperAttackDamage = 0x04F0 + 0x20;
        constexpr uintptr_t attr_AntiKnock       = 0x04F0 + 0x30;
        constexpr uintptr_t attr_RotationSpeed   = 0x04F0 + 0x34;

        constexpr uintptr_t AttackSphereRadius   = 0x048C;
        constexpr uintptr_t Pounce_Radius_Current = 0x0488;
        constexpr uintptr_t Normal_Attack_Interval = 0x04E4;
        constexpr uintptr_t Think_Interval       = 0x04E8;
        constexpr uintptr_t Immune               = 0x05A8; // bool
        constexpr uintptr_t is_bulletproof       = 0x05B4; // bool
        constexpr uintptr_t Hyper_Attack_Enabled = 0x05D4;
    }

    // AWeapon_Base_C  (size 0x368)
    namespace weapon
    {
        constexpr uintptr_t Weapon_Damage        = 0x0258;
        constexpr uintptr_t Affected_Radius      = 0x025C;
        constexpr uintptr_t Muzzle_Distance      = 0x0260;
        constexpr uintptr_t Damage_Type_Class    = 0x0270;
        constexpr uintptr_t Owner_Character      = 0x0278;
        constexpr uintptr_t current_weapon_damage = 0x02B4;
        constexpr uintptr_t in_hand              = 0x02CC;
        constexpr uintptr_t is_heavy_attack      = 0x0348;
    }

    // AWeapon_Gun_Base_C  (size 0x490)
    namespace gun
    {
        constexpr uintptr_t Mag_Size_from_Table  = 0x0380;
        constexpr uintptr_t recoil               = 0x0384;
        constexpr uintptr_t sound_radius         = 0x0388;
        constexpr uintptr_t Current_MagSize      = 0x03B0;
        constexpr uintptr_t Has_Magazine         = 0x03B4;
        constexpr uintptr_t Muzzle_Real_Location = 0x03C0; // FVector
        constexpr uintptr_t Muzzle_Real_Rotation = 0x03CC; // FRotator
        constexpr uintptr_t Automatic            = 0x03E0;
        constexpr uintptr_t has_a_bullet_in_gun  = 0x03F0;
        constexpr uintptr_t Gun_Data             = 0x0440; // FGun_Struct (0x38)
        constexpr uintptr_t GunData_AmmoInMag    = 0x0440 + 0x08; // int32
        constexpr uintptr_t GunData_MagIncrement = 0x0440 + 0x14; // int32
        constexpr uintptr_t GunData_DamageIncrement = 0x0440 + 0x18; // float
        constexpr uintptr_t extra_mag_size       = 0x047C;
        constexpr uintptr_t extra_damage         = 0x0480;
        constexpr uintptr_t Shake_Scale          = 0x0484;
    }

    // ATKA_GameState_C  (size 0x5D0)
    namespace gs
    {
        constexpr uintptr_t LocalPlayer            = 0x0300; // AJodi_C*
        constexpr uintptr_t UserInterface          = 0x0308; // UPlayingHud_C*
        constexpr uintptr_t Game_Instance          = 0x0328;
        constexpr uintptr_t is_challenge_mode      = 0x0348;
        constexpr uintptr_t Max_Bodies             = 0x034C;
        constexpr uintptr_t character_List         = 0x0358; // TArray<ACharacter_Base_C*>
        constexpr uintptr_t Current_Game_Difficulty = 0x0500; // int32 Net
        constexpr uintptr_t Time_Game_Over         = 0x0508;
        constexpr uintptr_t Flashlight_Intensity   = 0x0548;
    }

    // ATKA_Controller_C  (size 0x5C8)
    namespace tkctrl
    {
        constexpr uintptr_t Jodi              = 0x0578;
        constexpr uintptr_t Input_Look_Values = 0x0580; // FVector
        constexpr uintptr_t Input_Move_Values = 0x058C;
        constexpr uintptr_t TKA_Game_Instance = 0x05B0;
        constexpr uintptr_t TKA_Game_State    = 0x05B8;
    }

    // UBag_Comp_C  (size 0x150)
    namespace bag
    {
        constexpr uintptr_t bag_Tools       = 0x00B0; // TMap<FName,int32>
        constexpr uintptr_t total_weight    = 0x0108;
        constexpr uintptr_t weight_capacity = 0x010C;
        constexpr uintptr_t Player          = 0x0110;
        constexpr uintptr_t Clothes_in_bag  = 0x0128; // TArray<FName>
    }
}

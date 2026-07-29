#include "debug.hpp"
#include "../ctx.hpp"
#include "../imgui/imgui.h"
#include "../sdk.h"
#include "../game.h"
#include "../features.h"
#include "../offsets.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>

namespace {
    void TextPtr(const char* label, const void* p)
    {
        if (p) ImGui::Text("%-14s 0x%p", label, p);
        else   ImGui::TextDisabled("%-14s (null)", label);
    }

    // ---- SEH-safe readers (no C++ locals — C2712) ---------------------
    template<typename T>
    T SafeGet(const UObject* base, uintptr_t off, T def = T{})
    {
        if (!base) return def;
        __try { return *reinterpret_cast<const T*>(reinterpret_cast<uintptr_t>(base) + off); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return def; }
    }

    FVector SafeActorLocation(UObject* actor)
    {
        __try
        {
            UObject* root = *reinterpret_cast<UObject**>(reinterpret_cast<uintptr_t>(actor) + off::actor::RootComponent);
            if (!root) return {};
            return *reinterpret_cast<FVector*>(reinterpret_cast<uintptr_t>(root) + off::scene::RelativeLocation);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return {}; }
    }

    void ClassNameTo(UObject* obj, char* out, size_t n)
    {
        std::string s = (obj && obj->ClassPrivate) ? obj->ClassPrivate->GetName() : "?";
        strncpy_s(out, n, s.c_str(), _TRUNCATE);
    }
    void SafeClassName(UObject* obj, char* out, size_t n)
    {
        __try { ClassNameTo(obj, out, n); }
        __except (EXCEPTION_EXECUTE_HANDLER) { strncpy_s(out, n, "?", _TRUNCATE); }
    }

    void FeatureRow(const char* label, bool on)
    {
        ImGui::Text("%-16s", label);
        ImGui::SameLine();
        ImGui::TextColored(on ? ImVec4(0.3f, 0.9f, 0.3f, 1.f)
                              : ImVec4(0.5f, 0.5f, 0.5f, 1.f),
                           on ? "ON" : "off");
    }

    // World/GS/Jodi can be DANGLING during map transitions — never dereference
    // without IsValidObj (which itself is SEH-guarded).
    UObject* SafeWorld()
    {
        UObject* w = game::World();
        return game::IsValidObj(w) ? w : nullptr;
    }
    UObject* SafeGS()
    {
        if (!SafeWorld()) return nullptr;
        UObject* gs = game::GameState();
        return game::IsValidObj(gs) ? gs : nullptr;
    }
    UObject* SafeJodi()
    {
        UObject* j = game::Jodi();
        return game::IsValidObj(j) ? j : nullptr;
    }

    void DumpOffsetsToDebugOut()
    {
        char buf[512];
        UObject* jodi = SafeJodi();
        UObject* gs   = SafeGS();
        UObject* w    = jodi ? game::CurrentWeapon(jodi) : nullptr;
        if (w && !game::IsValidObj(w)) w = nullptr;
        _snprintf_s(buf, sizeof buf, _TRUNCATE,
            "[TKA DBG] base=%p GObjects=%p World=%p GS=%p Jodi=%p PC=%p Cam=%p Weapon=%p HP=%.1f/%.1f Vit=%.1f\n",
            (void*)sdk::ImageBase(), (void*)sdk::GObjects(),
            (void*)game::World(), (void*)gs, (void*)jodi,
            (void*)game::PlayerController(), (void*)game::Camera(), (void*)w,
            jodi ? game::GetHealth(jodi) : 0.f,
            jodi ? game::GetMaxHealth(jodi) : 0.f,
            jodi ? Get<float>(jodi, off::jodi::vitality) : 0.f);
        OutputDebugStringA(buf);
    }
}

void drawdebug()
{
    UObject* jodi = SafeJodi();
    auto&    cfg  = features::GetConfig();

    ImGui::Columns(2, nullptr, false);

    // ---- Engine ----
    ImGui::BeginChild("dbg_eng", ImVec2(0, 175), true);
    ImGui::TextDisabled("ENGINE"); ImGui::Dummy(ImVec2(0, 4));
    TextPtr("ImageBase",  (void*)sdk::ImageBase());
    TextPtr("GObjects",   (void*)sdk::GObjects());
    if (sdk::GObjects())
    {
        ImGui::Text("NumElements    %d", sdk::GObjects()->NumElements);
        ImGui::Text("NumChunks      %d", sdk::GObjects()->NumChunks);
    }
    TextPtr("World",      (void*)game::World());
    TextPtr("GameState",  (void*)SafeGS());
    TextPtr("GameInst",   (void*)(SafeWorld() ? game::GameInstance() : nullptr));
    ImGui::Text("Render FPS      %.1f", ImGui::GetIO().Framerate);
    ImGui::EndChild();

    // ---- Features state ----
    ImGui::BeginChild("dbg_feat", ImVec2(0, 0), true);
    ImGui::TextDisabled("FEATURES"); ImGui::Dummy(ImVec2(0, 4));
    ImGui::Columns(2, nullptr, false);
    FeatureRow("GodMode",     cfg.GodMode);
    FeatureRow("InfVitality", cfg.InfVitality);
    FeatureRow("InfAmmo",     cfg.InfAmmo);
    FeatureRow("NoRecoil",    cfg.NoRecoil);
    FeatureRow("BigDamage",   cfg.BigDamage);
    FeatureRow("SpeedHack",   cfg.SpeedHack);
    FeatureRow("JumpHack",    cfg.JumpHack);
    ImGui::NextColumn();
    FeatureRow("FlyHack",     cfg.FlyHack);
    FeatureRow("NoGravity",   cfg.NoGravity);
    FeatureRow("BigBackpack", cfg.InfBackpack);
    FeatureRow("XRay",        cfg.XRayZombies);
    FeatureRow("Aimbot",      cfg.Aimbot);
    FeatureRow("Triggerbot",  cfg.Triggerbot);
    FeatureRow("Telekill",    cfg.Telekill);
    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::NextColumn();

    // ---- Local player ----
    ImGui::BeginChild("dbg_local", ImVec2(0, 175), true);
    ImGui::TextDisabled("LOCAL PLAYER"); ImGui::Dummy(ImVec2(0, 4));
    TextPtr("Jodi",       (void*)jodi);
    TextPtr("Controller", (void*)game::PlayerController());
    TextPtr("Camera",     (void*)game::Camera());
    if (jodi)
    {
        ImGui::Text("HP             %.1f / %.1f",
                    SafeGet<float>(jodi, off::cbase::health),
                    SafeGet<float>(jodi, off::cbase::Health_Max));
        ImGui::Text("Vitality       %.1f / %.1f",
                    SafeGet<float>(jodi, off::jodi::vitality),
                    SafeGet<float>(jodi, off::jodi::Vitality_Max));
        FVector pos = SafeActorLocation(jodi);
        ImGui::Text("Pos            %.0f  %.0f  %.0f", pos.X, pos.Y, pos.Z);
        UObject* w = game::CurrentWeapon(jodi);
        if (w && !game::IsValidObj(w)) w = nullptr;
        if (w)
        {
            char wcls[64]; SafeClassName(w, wcls, sizeof wcls);
            ImGui::Text("Weapon         %s", wcls);
            ImGui::Text("  Ammo         %d / mag %d",
                        SafeGet<int32_t>(w, off::gun::Current_MagSize),
                        SafeGet<int32_t>(w, off::gun::Mag_Size_from_Table));
            ImGui::Text("  Dmg          %.1f (base %.1f)",
                        SafeGet<float>(w, off::weapon::current_weapon_damage),
                        SafeGet<float>(w, off::weapon::Weapon_Damage));
        }
        else ImGui::TextDisabled("Weapon         (none)");
    }
    else ImGui::TextDisabled("no local player (menu / transition)");
    ImGui::EndChild();

    // ---- Actions ----
    ImGui::BeginChild("dbg_act", ImVec2(0, 0), true);
    ImGui::TextDisabled("TESTS"); ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::Button("PE: RestoreHealth", ImVec2(150, 0)))   game::RestoreHealth(1.0f);
    if (ImGui::Button("PE: WakeAllZombies", ImVec2(150, 0)))  game::WakeAllZombies();
    if (ImGui::Button("Dump ptrs -> DbgView", ImVec2(170, 0))) DumpOffsetsToDebugOut();
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::TextDisabled("(DbgView = Sysinternals DebugView)");
    ImGui::EndChild();

    ImGui::Columns(1);

    // ---- Entity list (COPY under lock — iterating the live vector while the
    // feature thread swaps it was an instant crash) ----
    ImGui::BeginChild("dbg_ents", ImVec2(0, 150), true);
    const auto ents = features::GetEntitiesCopy();
    int zombies = 0;
    for (auto& e : ents) if (e.IsZombie && e.HP > 0) ++zombies;
    ImGui::TextDisabled("ENTITIES  total=%d  alive-zombies=%d", (int)ents.size(), zombies);
    ImGui::Separator();

    // Nearest 20
    std::vector<const features::EntityData*> sorted;
    sorted.reserve(ents.size());
    for (auto& e : ents) sorted.push_back(&e);
    std::sort(sorted.begin(), sorted.end(),
              [](const features::EntityData* a, const features::EntityData* b){ return a->Dist < b->Dist; });

    ImGui::Columns(5, "ent_cols", false);
    ImGui::TextDisabled("address");  ImGui::NextColumn();
    ImGui::TextDisabled("hp");       ImGui::NextColumn();
    ImGui::TextDisabled("dist (m)"); ImGui::NextColumn();
    ImGui::TextDisabled("type");     ImGui::NextColumn();
    ImGui::TextDisabled("visible");  ImGui::NextColumn();
    ImGui::Separator();
    int shown = 0;
    for (auto* e : sorted)
    {
        if (shown++ >= 20) break;
        ImGui::Text("0x%p", (void*)e->Actor);                 ImGui::NextColumn();
        ImGui::Text("%.0f/%.0f", e->HP, e->HPMax);            ImGui::NextColumn();
        ImGui::Text("%.1f", e->Dist / 100.0f);                ImGui::NextColumn();
        ImGui::Text("%s", e->IsZombie ? "zombie" : "other");  ImGui::NextColumn();
        ImGui::Text("%s", e->Visible ? "yes" : "-");          ImGui::NextColumn();
    }
    ImGui::Columns(1);
    ImGui::EndChild();
}

#include "player.hpp"
#include "../ctx.hpp"
#include "../imgui/imgui.h"
#include "../uicomponents/checkbox.hpp"
#include "../game.h"

void drawplayer() {
    ImGui::Columns(2, nullptr, false);

    // ---- Combat
    ImGui::BeginChild("p_combat", ImVec2(0, 0), true);
    ImGui::TextDisabled("COMBAT"); ImGui::Dummy(ImVec2(0, 5));
    cbx("God mode",                 &ctx.god);
    cbx("Infinite ammo",            &ctx.infAmmo);
    cbx("No recoil",                &ctx.noRecoil);
    cbx("Big damage",               &ctx.bigDamage);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("##dmg_m", &ctx.damageMult, 1.f, 10000.f, "x%.1f");
    ImGui::SameLine(); ImGui::Text("Damage x");
    ImGui::EndChild();

    ImGui::NextColumn();

    // ---- Character
    ImGui::BeginChild("p_char", ImVec2(0, 0), true);
    ImGui::TextDisabled("CHARACTER"); ImGui::Dummy(ImVec2(0, 5));
    cbx("Infinite vitality",  &ctx.infVitality);
    cbx("Big backpack (99999)", &ctx.bigBackpack);
    ImGui::Dummy(ImVec2(0, 5));

    if (ImGui::Button("Restore health (full)", ImVec2(200, 0))) game::RestoreHealth(1.0f);
    if (ImGui::Button("Restore vitality",       ImVec2(200, 0))) game::RestoreVitality(1.0f);
    if (ImGui::Button("+1000 EXP",              ImVec2(200, 0))) game::GotExp(1000.f);
    if (ImGui::Button("Revive local player",    ImVec2(200, 0))) game::ReviveLocalPlayer();
    ImGui::EndChild();

    ImGui::Columns(1);
}

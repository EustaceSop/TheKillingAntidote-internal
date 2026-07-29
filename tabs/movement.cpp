#include "movement.hpp"
#include "../ctx.hpp"
#include "../imgui/imgui.h"
#include "../uicomponents/checkbox.hpp"

void drawmovement() {
    ImGui::BeginChild("m_body", ImVec2(0, 0), true);
    ImGui::TextDisabled("MOVEMENT"); ImGui::Dummy(ImVec2(0, 5));

    cbx("Speed hack",   &ctx.speedHack);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("##spd", &ctx.speedVal, 100.f, 5000.f, "%.0f");
    ImGui::SameLine(); ImGui::Text("MaxWalkSpeed");

    ImGui::Dummy(ImVec2(0, 5));
    cbx("Jump hack",    &ctx.jumpHack);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("##jmp", &ctx.jumpVal,  200.f, 3000.f, "%.0f");
    ImGui::SameLine(); ImGui::Text("JumpZVelocity");

    ImGui::Dummy(ImVec2(0, 5));
    cbx("Fly hack (MovementMode=Flying)", &ctx.flyHack);
    cbx("No gravity (GravityScale=0)",    &ctx.noGravity);

    ImGui::Dummy(ImVec2(0, 15));
    ImGui::TextDisabled("HINTS");
    ImGui::TextWrapped("Speed applies to walking + swimming + (when Fly enabled) MaxFlySpeed. Combine Fly + High jump for vertical mobility.");

    ImGui::EndChild();
}

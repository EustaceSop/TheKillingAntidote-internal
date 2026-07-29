#include "visuals.hpp"
#include "../ctx.hpp"
#include "../imgui/imgui.h"
#include "../uicomponents/checkbox.hpp"
#include "../uicomponents/colorpicker.hpp"

void drawvisuals() {
    ImGui::Columns(2, nullptr, false);

    ImGui::BeginChild("esp_m", ImVec2(0, 300), true);
    ImGui::TextDisabled("ESP"); ImGui::Dummy(ImVec2(0, 5));
    cbx("Enable ESP", &ctx.espen);
    cbx("Box",        &ctx.box);   clrpk("box_c",  ctx.boxclr);
    cbx("Name",       &ctx.name);  clrpk("name_c", ctx.nmclr);
    cbx("Health bar", &ctx.hp);
    ImGui::EndChild();

    ImGui::NextColumn();
    ImGui::BeginChild("esp_mdl", ImVec2(0, 150), true);
    ImGui::TextDisabled("HIGHLIGHTS"); ImGui::Dummy(ImVec2(0, 5));
    cbx("XRay edge (via Set_XRay_Edge_Enabled)", &ctx.xrayZombies);
    cbx("Zombie glow tint", &ctx.glwen); clrpk("glow_c", ctx.glwclr);
    ImGui::EndChild();

    ImGui::BeginChild("esp_xtr", ImVec2(0, 0), true);
    ImGui::TextDisabled("Notes");
    ImGui::TextWrapped("Box/Name/HP need world->screen projection wired into the DX11 renderer. XRay uses the game's own UFunction and works right away.");
    ImGui::EndChild();

    ImGui::Columns(1);
}

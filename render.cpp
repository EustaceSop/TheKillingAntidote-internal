#include "render.hpp"
#include "ctx.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "uicomponents/tab.hpp"
#include "tabs/visuals.hpp"
#include "tabs/aimbot.hpp"
#include "tabs/misc.hpp"
#include "tabs/player.hpp"
#include "tabs/movement.hpp"
#include "tabs/world.hpp"
#include "tabs/debug.hpp"

void applystyle(float mod) {
    auto& s = ImGui::GetStyle();
    s.WindowRounding = 4.0f; s.ChildRounding = 2.0f; s.FrameRounding = 2.0f;
    s.PopupRounding  = 2.0f; s.GrabRounding  = 2.0f; s.ScrollbarSize = 4.0f;
    s.ScrollbarRounding = 2.0f; s.Alpha = mod;
    s.WindowBorderSize = 1.0f; s.ChildBorderSize = 1.0f; s.FrameBorderSize = 0.0f;
    s.PopupBorderSize  = 1.0f; s.ItemSpacing   = ImVec2(8, 8);
    auto c = s.Colors;
    ImVec4 acc(0.42f, 0.65f, 0.95f, 1.0f);
    c[ImGuiCol_WindowBg]      = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
    c[ImGuiCol_ChildBg]       = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    c[ImGuiCol_Border]        = ImVec4(0.12f, 0.12f, 0.12f, 1.0f * mod);
    c[ImGuiCol_FrameBg]       = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    c[ImGuiCol_FrameBgHovered]= ImVec4(0.11f, 0.11f, 0.11f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
    c[ImGuiCol_PopupBg]       = ImVec4(0.06f, 0.06f, 0.06f, 0.98f);
    c[ImGuiCol_CheckMark]     = acc;
    c[ImGuiCol_SliderGrab]    = acc;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.52f, 0.75f, 1.0f, 1.0f);
    c[ImGuiCol_Header]        = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    c[ImGuiCol_Button]        = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
    c[ImGuiCol_Text]          = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    c[ImGuiCol_TextDisabled]  = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
}

void drawovr() {
    if (!ctx.showbinds) return;
    applystyle(1.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(180, -1), ImVec2(300, -1));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.04f, 0.9f));
    ImGui::Begin("b_hud", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextDisabled("keybinds"); ImGui::Dummy(ImVec2(0, 5));
    auto item = [](const char* n, const bind_t& b, bool e) {
        if (e && b.isact()) {
            ImGui::Text("%s", n);
            const char* st = (b.mode == 2) ? "[toggle]" : (b.mode == 0 ? "[always]" : "[hold]");
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(st).x - 10);
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], st);
        }
    };
    if (!(ctx.aim && ctx.aimkey.isact()) && !(ctx.trig && ctx.trigkey.isact()))
        ImGui::TextDisabled("empty");
    else {
        item("aimbot",     ctx.aimkey,      ctx.aim);
        item("triggerbot", ctx.trigkey,     ctx.trig);
        item("telekill",   ctx.telekillKey, ctx.telekillHotkey);
    }
    ImGui::End(); ImGui::PopStyleColor();
}

void drawmenu(float mod) {
    applystyle(mod);
    ImGui::SetNextWindowSize(ImVec2(720, 540), ImGuiCond_FirstUseEver);
    ImGui::Begin("tka_menu", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
    dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y),
                      ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.06f, 1.0f)));

    ImGui::PushFont(fttl); ImGui::SetCursorPos(ImVec2(20, 15));
    ImGui::Text("TKA internal"); ImGui::PopFont();

    ImGui::SameLine(150); ImGui::SetCursorPosY(15);
    if (mktab("Player",   ctx.tab == 0, ImVec2(75, 25))) ctx.tab = 0; ImGui::SameLine();
    if (mktab("Movement", ctx.tab == 1, ImVec2(85, 25))) ctx.tab = 1; ImGui::SameLine();
    if (mktab("World",    ctx.tab == 2, ImVec2(75, 25))) ctx.tab = 2; ImGui::SameLine();
    if (mktab("Aim",      ctx.tab == 3, ImVec2(60, 25))) ctx.tab = 3; ImGui::SameLine();
    if (mktab("Visuals",  ctx.tab == 4, ImVec2(75, 25))) ctx.tab = 4; ImGui::SameLine();
    if (mktab("Misc",     ctx.tab == 5, ImVec2(60, 25))) ctx.tab = 5; ImGui::SameLine();
    if (mktab("Debug",    ctx.tab == 6, ImVec2(65, 25))) ctx.tab = 6;

    ImGui::Dummy(ImVec2(0, 15));
    dl->AddLine(ImVec2(p.x, ImGui::GetCursorPosY() + p.y),
                ImVec2(p.x + sz.x, ImGui::GetCursorPosY() + p.y),
                ImGui::GetColorU32(ImVec4(1,1,1,0.05f)));
    ImGui::Dummy(ImVec2(0, 5));

    ImGui::BeginChild("c_wnd", ImVec2(0, 0), false);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.04f, 1.0f));

    switch (ctx.tab) {
        case 0: drawplayer();   break;
        case 1: drawmovement(); break;
        case 2: drawworld();    break;
        case 3: drawaimbot();   break;
        case 4: drawvisuals();  break;
        case 5: drawmisc();     break;
        case 6: drawdebug();    break;
    }

    ImGui::PopStyleColor(); ImGui::EndChild(); ImGui::End();
}

#include "misc.hpp"
#include "../ctx.hpp"
#include "../imgui/imgui.h"
#include "../uicomponents/checkbox.hpp"

void drawmisc() {
    ImGui::BeginChild("ui_m", ImVec2(0, 0), true);
    ImGui::TextDisabled("UI"); ImGui::Dummy(ImVec2(0, 5));
    cbx("Show watermark",         &ctx.wm);
    cbx("Show keybinds overlay",  &ctx.showbinds);
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::TextDisabled("BUILD");
    ImGui::Text("TheKillingAntidote internal");
    ImGui::Text("ImGui + DX11 + MinHook");
    ImGui::EndChild();
}

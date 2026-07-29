#include "world.hpp"
#include "../ctx.hpp"
#include "../imgui/imgui.h"
#include "../uicomponents/checkbox.hpp"
#include "../uicomponents/tab.hpp"
#include "../uicomponents/combo.hpp"
#include "../game.h"
#include "../features.h"

void drawworld() {
    ImGui::Columns(2, nullptr, false);

    // ---- Zombies
    ImGui::BeginChild("w_zom", ImVec2(0, 0), true);
    ImGui::TextDisabled("ZOMBIES"); ImGui::Dummy(ImVec2(0, 5));

    cbx("XRay edge highlight (auto)", &ctx.xrayZombies);
    cbx("Telekill hotkey enabled",    &ctx.telekillHotkey);
    mkbind("tk_b", ctx.telekillKey);

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Wake up ALL zombies",     ImVec2(220, 0))) game::WakeAllZombies();
    if (ImGui::Button("Bring all zombies to me", ImVec2(220, 0))) game::ComeToMeZombies();
    if (ImGui::Button("Kill all visible zombies", ImVec2(220, 0)))
        features::GetConfig().Telekill = true;

    ImGui::Dummy(ImVec2(0, 5));
    ImGui::TextDisabled("Alive zombies: %d",
        (int)features::GetEntitiesCopy().size());   // copy under lock — never the live vector
    ImGui::EndChild();

    ImGui::NextColumn();

    // ---- Game state
    ImGui::BeginChild("w_game", ImVec2(0, 0), true);
    ImGui::TextDisabled("GAME"); ImGui::Dummy(ImVec2(0, 5));

    static const char* difOpts[] = { "Easy (0)", "Normal (1)", "Hard (2)", "Challenge (3)" };
    cmb("Difficulty", &ctx.difficulty, difOpts, 4);
    if (ImGui::Button("Apply difficulty", ImVec2(180, 0))) ctx.applyDifficulty = true;

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Show UI ON",  ImVec2(90, 0))) game::ShowUI(true);
    ImGui::SameLine();
    if (ImGui::Button("Show UI OFF", ImVec2(90, 0))) game::ShowUI(false);

    if (ImGui::Button("View distance x10", ImVec2(180, 0)))
        game::ChangeViewDistanceScale(10.f);
    if (ImGui::Button("View distance x1",  ImVec2(180, 0)))
        game::ChangeViewDistanceScale(1.f);

    ImGui::EndChild();

    ImGui::Columns(1);
}

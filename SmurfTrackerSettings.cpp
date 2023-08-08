#include "pch.h"
#include "SmurfTracker.h"

void SmurfTracker::RenderSettings() {
    ImGui::TextUnformatted("A really cool plugin");

    if (ImGui::Button("Check Function")) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            cvarManager->executeCommand("DisplayPlayerIDs");
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Test");
    }

    CVarWrapper enableCvar = cvarManager->getCvar("SmurfTracker_enabled");
    if (!enableCvar) { return; }
    bool enabled = enableCvar.getBoolValue();
    if (ImGui::Checkbox("Enable plugin", &enabled)) {
        enableCvar.setValue(enabled);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle SmurfTracker Plugin");
    }
}

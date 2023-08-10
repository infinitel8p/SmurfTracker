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

    if (ImGui::Button("Check HTTP Request")) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            cvarManager->executeCommand("HTTPRequest https:||jsonplaceholder.typicode.com|posts");
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Send a HTTP Request to jsonplaceholder.typicode.com/posts and log results");
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

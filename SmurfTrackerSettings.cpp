#include "pch.h"
#include "SmurfTracker.h"

void SmurfTracker::RenderSettings() {
    ImGui::TextUnformatted("A really cool plugin");


    // Check Function Button
    if (ImGui::Button("Check Function")) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            cvarManager->executeCommand("DisplayPlayerIDs");
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Test");
    }

    // Check HTTP Request Button
    if (ImGui::Button("Check HTTP Request")) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            cvarManager->executeCommand("HTTPRequest https:||jsonplaceholder.typicode.com|posts");
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Send a HTTP Request to jsonplaceholder.typicode.com/posts and log results");
    }

    // Enable Render Checkbox
    CVarWrapper enableCvar = cvarManager->getCvar("SmurfTracker_enabled");
    if (!enableCvar) { return; }
    bool enabled = enableCvar.getBoolValue();
    if (ImGui::Checkbox("Enable plugin", &enabled)) {
        enableCvar.setValue(enabled);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle SmurfTracker Plugin");
    }

    // Mode Combo Box
    CVarWrapper modeCvar = cvarManager->getCvar("SmurfTracker_mode");
    if (!modeCvar) { return; } 
    static int currentItem = 0;  // Index of the currently selected item, static to keep its state
    const char* items[] = { "Test", "MMR", "Wins" }; // Modes
    if (ImGui::Combo("Mode", &currentItem, items, IM_ARRAYSIZE(items))) { 
        modeCvar.setValue(currentItem); 
    }
    if (ImGui::IsItemHovered()) { 
        ImGui::SetTooltip("Select the mode to display"); 
    }
}

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
            cvarManager->executeCommand("TestHTTPRequest");
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
    static int selectedMode = 0;  // Index of the currently selected item, static to keep its state
    const char* items[] = { "Test", "MMR", "Wins" }; // Modes
    if (ImGui::Combo("Mode", &selectedMode, items, IM_ARRAYSIZE(items))) {
        modeCvar.setValue(selectedMode);
    }
    if (ImGui::IsItemHovered()) { 
        ImGui::SetTooltip("Select the mode to display"); 
    }

    // IP address input of endpoint
    CVarWrapper ipCvar = cvarManager->getCvar("SmurfTracker_ip");
    if (!ipCvar) { return; }
    char ip[16] = ""; // Buffer to store IP address
    std::string currentIP = ipCvar.getStringValue();
    strncpy(ip, currentIP.c_str(), sizeof(ip));
    ip[sizeof(ip) - 1] = '\0'; // Ensure null-termination

    if (ImGui::InputText("IP Address", ip, IM_ARRAYSIZE(ip))) {
        ipCvar.setValue(std::string(ip));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Set the IP address of the SmurfTracker endpoint");
    }
}

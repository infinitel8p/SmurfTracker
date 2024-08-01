#include "pch.h"
#include "SmurfTracker.h"

void SmurfTracker::RenderSettings() {
    ImGui::TextUnformatted("General Settings");

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
    int selectedMode = modeCvar.getIntValue();
    const char* items[] = { "Score", "MMR", "Wins" }; // Modes
    if (ImGui::Combo("Mode", &selectedMode, items, IM_ARRAYSIZE(items))) {
        modeCvar.setValue(selectedMode);
    }
    if (ImGui::IsItemHovered()) { 
        ImGui::SetTooltip("Select the mode to display"); 
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Wins mode settings:");

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

    ImGui::TextUnformatted("Does not work yet:");
    CVarWrapper checkTeammatesCvar = cvarManager->getCvar("SmurfTracker_check_teammates");
    if (!checkTeammatesCvar) { return; }
    bool checkTeammates = checkTeammatesCvar.getBoolValue();
    if (ImGui::Checkbox("Check teammates", &checkTeammates)) {
        checkTeammatesCvar.setValue(checkTeammates);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Disable checking teammates stats to reduce API calls");
    }

    CVarWrapper checkSelfCvar = cvarManager->getCvar("SmurfTracker_check_self");
    if (!checkSelfCvar) { return; }
    bool checkSelf = checkSelfCvar.getBoolValue();
    if (ImGui::Checkbox("Check yourself", &checkSelf)) {
        checkSelfCvar.setValue(checkSelf);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Disable checking your own stats to reduce API calls");
    }
    ImGui::Separator();
}

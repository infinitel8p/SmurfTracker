#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include <string>
#include <vector>
#include <fstream>

#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

struct PlayerDetails {
	std::string playerName;
	std::string platform;
	std::string uniqueID;
	int playerIndex = 0;
	int wins = 0;
	int currentScore = 0;
	int team;
};

std::string getCurrentTime();

class SmurfTracker : public BakkesMod::Plugin::BakkesModPlugin
	, public SettingsWindowBase
{
	// Boilerplate
	void onLoad() override;
	void onUnload() override;

	void DisplayPlayerIDs();
	void HTTPRequest(const std::string& url);
	void Render(CanvasWrapper canvas);
	void InitializeCurrentPlayers();
	void ClearCurrentPlayers();
	void LogF(const std::string& message);
	void UpdatePlayerList();
	void UpdateTeamStrings();	

	bool isSBOpen;
	bool smurfTrackerEnabled;
	std::vector<PlayerDetails> currentPlayers;
	std::ofstream logFile;
	std::vector<std::string> blueTeam;
	std::vector<std::string> orangeTeam;

public:
	void RenderSettings() override;
};

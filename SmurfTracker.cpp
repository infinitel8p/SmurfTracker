#include "pch.h"
#include "SmurfTracker.h"
#include "json.hpp"
#include <set>
#include "url_encode.h"

BAKKESMOD_PLUGIN(SmurfTracker, "Identify Smurfs.", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

std::string getCurrentTime() {
	std::time_t t = std::time(nullptr);
	std::tm tm = *std::localtime(&t);
	std::stringstream ss;
	ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return ss.str();
}

void SmurfTracker::LogF(const std::string& message) {
	if (logFile.is_open()) {
		logFile << "[" << getCurrentTime() << "] " << message << std::endl;
	}
	else {
		LOG("Log file is not open!");
	}
}

void SmurfTracker::onLoad()
{
	_globalCvarManager = cvarManager;
	LOG("SmurfTracker loaded!");
	DEBUGLOG("SmurfTracker debug mode enabled"); // logging.h DEBUG_LOG = true;

	// Open the log file in append mode (Location is Epic Games\rocketleague\Binaries\Win64\SmurfTracker.log)
	logFile.open("SmurfTracker.log", std::ios::app);
	if (!logFile.is_open()) {
		LOG("Failed to open log file!");
	}

	// Register the render function to be called each frame
	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});
	
	cvarManager->registerCvar("SmurfTracker_enabled", "0", "Enable SmurfTracker Plugin", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		smurfTrackerEnabled = cvar.getBoolValue();
	});

	cvarManager->registerCvar("SmurfTracker_mode", "0", "SmurfTracker selected mode", true, true, 0, true, 2)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		selectedMode = cvar.getIntValue();
	});

	cvarManager->registerCvar("SmurfTracker_ip", "127.0.0.1", "IP Address for SmurfTracker endpoint", true, true, 0, true, 15)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		ipAddress = cvar.getStringValue();
	});

	cvarManager->registerNotifier("DisplayPlayerIDs", [this](std::vector<std::string> args) {
		DisplayPlayerIDs();
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("InitializeCurrentPlayers", [this](std::vector<std::string> args) {
		InitializeCurrentPlayers();
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("TestHTTPRequest", [this](std::vector<std::string> args) {
		HTTPRequest();
		}, "", PERMISSION_ALL);

	// Hook into the OnAllTeamsCreated event to log when all teams are created	
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnAllTeamsCreated", [this](std::string eventName) {
		LOG("Initialize Game Session");
		cvarManager->executeCommand("InitializeCurrentPlayers");
		});

	// Hook into the OnOpenScoreboard event to display player IDs when the scoreboard is opened
	gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnOpenScoreboard", [this](std::string eventName) {
		//cvarManager->executeCommand("DisplayPlayerIDs");
		isSBOpen = true;
		UpdatePlayerList();
		});

	// Hook into the OnCloseScoreboard event to log a message when the scoreboard is closeds
	gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnCloseScoreboard", [this](std::string eventName) {
		isSBOpen = false;
		});

	// Hook into the OnMatchEnded event to remove cached player details
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnMatchEnded", [this](std::string eventName) {
		LOG("Finalize Game Session");
		ClearCurrentPlayers();
		});
}

void SmurfTracker::InitializeCurrentPlayers()
{
	if (!smurfTrackerEnabled) {
		return;
	}

	// Clear any existing players
	currentPlayers.clear();

	// Check if the game is valid
	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay() || gameWrapper->IsInReplay()) {
		LOG("Not in an online game or freeplay!");
		return;
	}

	ServerWrapper sw = NULL;
	if (gameWrapper->IsInFreeplay()) {
		sw = gameWrapper->GetGameEventAsServer();
	}
	else {
		sw = gameWrapper->GetOnlineGame();
	}

	if (sw.IsNull() || sw.GetbMatchEnded()) {
		LOG("Invalid game state or match ended!");
		return;
	}

	// Open the log file in append mode (Location is Epic Games\rocketleague\Binaries\Win64\SmurfTracker.log)
	std::ofstream logFile("SmurfTracker.log", std::ios::app);

	// Initialize a map to store unique IDs and corresponding player details
	std::map<std::string, PlayerDetails> uniqueIDMap;

	// Get the array of players
	ArrayWrapper<PriWrapper> players = sw.GetPRIs();

	// Iterate through the players
	for (size_t i = 0; i < players.Count(); i++)
	{
		PriWrapper playerWrapper = players.Get(i);
		if (playerWrapper.IsNull()) continue;

		UniqueIDWrapper uniqueID = playerWrapper.GetUniqueIdWrapper();
		std::string uniqueIDString = uniqueID.GetIdString();

		// Extract player information
		PlayerDetails details;
		details.playerName = playerWrapper.GetPlayerName().ToString();
		details.uniqueID = uniqueIDString;
		details.playerIndex = static_cast<int>(i);  // Assuming i is the player index
		details.team = playerWrapper.GetTeamNum();  // 0 is blue, 1 is orange
		details.wins = "Searching...";

		// Find separators
		size_t firstSeparator = uniqueIDString.find('|');
		size_t secondSeparator = uniqueIDString.find('|', firstSeparator + 1);

		// Check for valid separators
		if (firstSeparator == std::string::npos || secondSeparator == std::string::npos || firstSeparator >= secondSeparator) {
			LOG("Invalid unique ID format: " + uniqueIDString);
			continue; // Skip this player if the unique ID format is not as expected
		}

		std::string uniqueIDPart = uniqueIDString.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
		int playerIndex = static_cast<int>(std::stoi(uniqueIDString.substr(secondSeparator + 1)));
		std::string platform = uniqueIDString.substr(0, firstSeparator);
		details.platform = platform;

		// If this is a main player, store the details
		if (playerIndex == 0) {
			uniqueIDMap[uniqueIDPart] = details;
		}
		// If this is a splitscreen player, retrieve the main player's details
		else {
			if (uniqueIDMap.find(uniqueIDPart) != uniqueIDMap.end()) {
				details = uniqueIDMap[uniqueIDPart];
				// Log the splitscreen relationship
				LOG("Splitscreen player of main player at index " + std::to_string(details.playerIndex));
				continue;
			}
			else {
				LOG("Main player's details not found for splitscreen player: " + uniqueIDString);
				continue;
			}
		}

		// Add the player details to the currentPlayers vector
		currentPlayers.push_back(details);
	}

	HTTPRequest();
	UpdateTeamStrings();
}

void SmurfTracker::ClearCurrentPlayers()
{
	currentPlayers.clear();
}

void SmurfTracker::UpdatePlayerList() {
	ServerWrapper sw = NULL;
	if (gameWrapper->IsInFreeplay()) {
		sw = gameWrapper->GetGameEventAsServer();
	}
	else {
		sw = gameWrapper->GetOnlineGame();
	}

	if (sw.IsNull() || sw.GetbMatchEnded()) {
		LOG("Invalid game state or match ended!");
		return;
	}

	if (currentPlayers.size() < 1) {
		InitializeCurrentPlayers();
		return;
	}

	if (currentPlayers.size() < sw.GetPRIs().Count()) {
		InitializeCurrentPlayers();
		return;
	}

	// Get the array of players
	ArrayWrapper<PriWrapper> players = sw.GetPRIs();

	// perform sanity check for player count and expected player count
	std::set<std::string> playerNamesSet(blueTeam.begin(), blueTeam.end());
	playerNamesSet.insert(orangeTeam.begin(), orangeTeam.end());

	bool allPlayersFound = true;
	for (size_t i = 0; i < players.Count(); ++i) {
		PriWrapper priw = players.Get(i);
		if (priw.IsNull()) continue;

		std::string playerName = priw.GetPlayerName().ToString();
		if (playerNamesSet.find(playerName) == playerNamesSet.end()) {
			allPlayersFound = false;
			break;
		}
	}
	if (!allPlayersFound) {
		LogF("Connected Players:" + std::to_string(currentPlayers.size()) + "/" + std::to_string(players.Count()) + " (Not all players found!)");
		InitializeCurrentPlayers();
		return;
	}

	// Update the current scores of the players
	for (PlayerDetails& player : currentPlayers) {
		for (size_t i = 0; i < players.Count(); ++i) {
			PriWrapper priw = players.Get(i);
			if (priw.IsNull()) continue;

			UniqueIDWrapper uidw = priw.GetUniqueIdWrapper();
			std::string uidString = uidw.GetIdString();
			if (uidString == player.uniqueID) {
				player.currentScore = priw.GetMatchScore();
				break;  // Exit the inner loop as we've found the matching player
			}
		}
	}

	// Sort the players by team and score	
	std::sort(currentPlayers.begin(), currentPlayers.end(), [](const PlayerDetails& a, const PlayerDetails& b) {
		if (a.team == b.team) {
			return a.currentScore > b.currentScore;  // Sort by currentScore within the same team
		}
		return a.team < b.team;  // Sort by team (Blue before Orange)
		});

	UpdateTeamStrings();
}

void SmurfTracker::UpdateTeamStrings() {
	blueTeam.clear();
	orangeTeam.clear();

	for (const auto& player : currentPlayers) {
		if (player.team == 0) {
			blueTeam.push_back(player.playerName);
		}
		else if (player.team == 1) {
			orangeTeam.push_back(player.playerName);
		}
	}
}

void SmurfTracker::HTTPRequest()
{
	ServerWrapper sw = NULL;
	if (gameWrapper->IsInFreeplay()) {
		sw = gameWrapper->GetGameEventAsServer();
	}
	else {
		sw = gameWrapper->GetOnlineGame();
	}

	if (sw.IsNull() || sw.GetbMatchEnded()) {
		LOG("Invalid game state or match ended!");
		return;
	}

	if (currentPlayers.size() < sw.GetPRIs().Count()) {
		InitializeCurrentPlayers();
		return;
	}

	for (PlayerDetails& player : currentPlayers) {
		std::string ipAddress = cvarManager->getCvar("SmurfTracker_ip").getStringValue();
		std::string url = "http://" + ipAddress + ":8191/v1";
		std::string platform = player.platform;
		std::string playerName = player.playerName;
		std::string targetUrl = "https://rlstats.net/profile/" + platform + "/" + urlEncode(playerName);

		// Create the JSON data for the POST request
		nlohmann::json data;
		data["cmd"] = "request.get";
		data["url"] = targetUrl;
		data["maxTimeout"] = 60000;

		LOG("Sending stats request to " + url);

		// non async curl request:
		CurlRequest req;
		req.url = url;
		req.body = data.dump(); // Convert JSON data to string
		req.headers["Content-Type"] = "application/json";

		// Variable to store the result
		std::string result;

		// Define the callback function, capturing `this` to access class members
		auto callback = [this, &player](int code, std::string response) {
			if (code == 200) { // Check if the request was successful
				try {
					auto response_data = nlohmann::json::parse(response);
					std::string wins = response_data["wins"];

					//std::ofstream logFile("SmurfTracker.log", std::ios::app);
					//logFile << "\n[" << getCurrentTime() << "] " << html_content << std::endl;

					LOG("Currently assigned value: " + player.wins);
					LOG("New assigned value: " + wins);
					player.wins = wins;
				}
				catch (const nlohmann::json::exception& e) {
					LOG(std::string("JSON parsing error: ") + e.what());
					player.wins = "Error";
				}
			}
			else {
				LOG("Request failed with code: " + std::to_string(code));
				player.wins = "Error: " + std::to_string(code);
			}
			};

		// Send the request using BakkesMod's HttpWrapper
		HttpWrapper::SendCurlRequest(req, callback);
	}
}

void SmurfTracker::Render(CanvasWrapper canvas)
{
	if (!smurfTrackerEnabled || !isSBOpen) {
		return;
	}

	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay() || gameWrapper->IsInReplay()) {
		return;
	}

	// selected mode to display
	int currentMode = cvarManager->getCvar("SmurfTracker_mode").getIntValue();
	const char* items[] = { "Test", "MMR", "Wins" }; // Modes

	if (currentMode == 0) {
		// if mode is test, display the scoreboard
		// Get the screen size
		int screenWidth = canvas.GetSize().X;
		int screenHeight = canvas.GetSize().Y;

		// Calculate the position next to the scoreboard
		Vector2 scoreboardPosition(screenWidth - screenWidth / 4, screenHeight / 2 - screenHeight / 10);

		std::string TextToDisplay = "Connected Players:" + std::to_string(currentPlayers.size()) + " Mode: " + items[currentMode];

		LinearColor white;
		white.R = 255;
		white.G = 255;
		white.B = 255;
		white.A = 255;
		canvas.SetColor(white);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString(TextToDisplay);

		// Initialize vertical offset for player names
		int verticalOffset = 20;

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;

		// Draw Blue Team
		LinearColor blue;
		blue.R = 0;
		blue.G = 0;
		blue.B = 255;
		blue.A = 255;
		canvas.SetColor(blue);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString("Blue:");

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;
		canvas.SetColor(white);

		for (const auto& player : blueTeam) {
			for (const auto& players : currentPlayers) {
				if (player == players.playerName) {
					LOG("");
				}
			}
			canvas.SetPosition(scoreboardPosition);
			canvas.DrawString(player);
			scoreboardPosition.Y += verticalOffset;
		}


		// Draw Orange Team
		LinearColor orange;
		orange.R = 255;
		orange.G = 165;
		orange.B = 0;
		orange.A = 255;
		canvas.SetColor(orange);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString("Orange:");

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;
		canvas.SetColor(white);

		for (const auto& player : orangeTeam) {
			canvas.SetPosition(scoreboardPosition);
			canvas.DrawString(player);
			scoreboardPosition.Y += verticalOffset;
		}
	}

	if (currentMode == 1) {
		// if mode is MMR, display the MMR of each player
		// Get the screen size
		int screenWidth = canvas.GetSize().X;
		int screenHeight = canvas.GetSize().Y;

		// Calculate the position next to the scoreboard
		Vector2 scoreboardPosition(screenWidth - screenWidth / 4, screenHeight / 2 - screenHeight / 10);

		std::string TextToDisplay = "Connected Players:" + std::to_string(currentPlayers.size()) + " Mode: " + items[currentMode];

		LinearColor white;
		white.R = 255;
		white.G = 255;
		white.B = 255;
		white.A = 255;
		canvas.SetColor(white);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString(TextToDisplay);

		// Initialize vertical offset for player names
		int verticalOffset = 20;

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;

		// Draw Blue Team
		LinearColor blue;
		blue.R = 0;
		blue.G = 0;
		blue.B = 255;
		blue.A = 255;
		canvas.SetColor(blue);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString("Blue:");

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;
		canvas.SetColor(white);

		// playlist id 11
		for (const auto& player : blueTeam) {
			std::string playerNameWithMMR = player;
			canvas.SetPosition(scoreboardPosition);
			canvas.DrawString(playerNameWithMMR);
			scoreboardPosition.Y += verticalOffset;
		}


		// Draw Orange Team
		LinearColor orange;
		orange.R = 255;
		orange.G = 165;
		orange.B = 0;
		orange.A = 255;
		canvas.SetColor(orange);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString("Orange:");

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;
		canvas.SetColor(white);

		for (const auto& player : orangeTeam) {
			canvas.SetPosition(scoreboardPosition);
			canvas.DrawString(player);
			scoreboardPosition.Y += verticalOffset;
		}
	}

	if (currentMode == 2) {
		// if mode is wins, display the wins of each player
		// Get the screen size
		int screenWidth = canvas.GetSize().X;
		int screenHeight = canvas.GetSize().Y;

		// Calculate the position next to the scoreboard
		Vector2 scoreboardPosition(screenWidth - screenWidth / 4, screenHeight / 2 - screenHeight / 10);

		std::string TextToDisplay = "Connected Players:" + std::to_string(currentPlayers.size()) + " Mode: " + items[currentMode];

		LinearColor white;
		white.R = 255;
		white.G = 255;
		white.B = 255;
		white.A = 255;
		canvas.SetColor(white);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString(TextToDisplay);

		// Initialize vertical offset for player names
		int verticalOffset = 20;

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;

		// Draw Blue Team
		LinearColor blue;
		blue.R = 0;
		blue.G = 0;
		blue.B = 255;
		blue.A = 255;
		canvas.SetColor(blue);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString("Blue:");

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;
		canvas.SetColor(white);

		for (const auto& playerName : blueTeam) {
			std::string displayString;
			for (const auto& playerDetails : currentPlayers) {
				if (playerName == playerDetails.playerName) {
					displayString = playerDetails.playerName + " - Wins: " + playerDetails.wins;
					break; // Found the player, no need to continue the inner loop
				}
			}
			canvas.SetPosition(scoreboardPosition);
			canvas.DrawString(displayString.empty() ? playerName : displayString);
			scoreboardPosition.Y += verticalOffset;
		}


		// Draw Orange Team
		LinearColor orange;
		orange.R = 255;
		orange.G = 165;
		orange.B = 0;
		orange.A = 255;
		canvas.SetColor(orange);
		canvas.SetPosition(scoreboardPosition);
		canvas.DrawString("Orange:");

		// Update the vertical position for the next line
		scoreboardPosition.Y += verticalOffset;
		canvas.SetColor(white);

		for (const auto& playerName : orangeTeam) {
			std::string displayString;
			for (const auto& playerDetails : currentPlayers) {
				if (playerName == playerDetails.playerName) {
					displayString = playerDetails.playerName + " - Wins: " + playerDetails.wins;
					break; // Found the player, no need to continue the inner loop
				}
			}
			canvas.SetPosition(scoreboardPosition);
			canvas.DrawString(displayString.empty() ? playerName : displayString);
			scoreboardPosition.Y += verticalOffset;
		}
	}
}

void SmurfTracker::DisplayPlayerIDs()
{
	if (!smurfTrackerEnabled) {
		LOG("Plugin not enabled!");
		return;
	}

	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay() || gameWrapper->IsInReplay()) {
		LOG("Not in an online game or freeplay!");
		return;
	}

	ServerWrapper sw = NULL;
	if (gameWrapper->IsInFreeplay()) {
		sw = gameWrapper->GetGameEventAsServer();
	}
	else {
		sw = gameWrapper->GetOnlineGame();
	}

	if (sw.IsNull() || sw.GetbMatchEnded()) {
		LOG("Invalid game state or match ended!");
		return;
	}

	// Open the log file in append mode (Location is Epic Games\rocketleague\Binaries\Win64\SmurfTracker.log)
	std::ofstream logFile("SmurfTracker.log", std::ios::app);

	// Iterate through the currentPlayers vector to log or display the details
	for (const auto& details : currentPlayers) {

		// Log the details
		std::string logMessage = "Player name: " + details.playerName +
			" | Platform: " + details.platform +
			" | ID: " + details.uniqueID +
			" | PlayerIndex: " + std::to_string(details.playerIndex);
		LOG(logMessage);
		logFile << "\n[" << getCurrentTime() << "] " << logMessage << std::endl;

		// Check the platform in the unique ID and log the appropriate request URL
		if (details.uniqueID.find("Epic") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/epic/" + details.playerName);
		}
		else if (details.uniqueID.find("PS4") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/psn/" + details.playerName);
		}
		else if (details.uniqueID.find("Switch") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/switch/" + details.playerName);
		}

		// https://rocketleague.tracker.network/rocket-league/profile/steam/76561198138690072/overview ????-steam|uniqueID
		// https://rocketleague.tracker.network/rocket-league/profile/xbl/RocketLeague893/overview ????-xbl|xbl-username
		// https://rocketleague.tracker.network/rocket-league/profile/epic/test/overview Epic-epic|epic-username
		// https://rocketleague.tracker.network/rocket-league/profile/psn/RocketLeagueNA/overview PS4-psn|psn-username
		// https://rocketleague.tracker.network/rocket-league/profile/switch/test/overview Switch-switch|switch-usernamefpla
		// uniqueID = Platform|Userid|Splitscreen/PlayerIndex

		// TODO: Determine the position to draw the ID
		// TODO: Use the correct method to draw the string on the canvas

		std::vector<CareerStatsWrapper::StatValue> statValues = CareerStatsWrapper::GetStatValues();
		for (const auto& statValue : statValues) {
			if (statValue.stat_name == "Win") {
				std::string logMessage = getCurrentTime() + " | Stat Name: " + statValue.stat_name +
					" | Private: " + std::to_string(statValue.private_) +
					" | Unranked: " + std::to_string(statValue.unranked) +
					" | Ranked: " + std::to_string(statValue.ranked);
				logFile << "\n[" << getCurrentTime() << "] " << logMessage << std::endl;
				break; // Exit the loop once the "Win" stat is found
			}
		}

		// HTTPRequest("https://jsonplaceholder.typicode.com/users");
	}

	// Close the log file
	logFile.close();
}

void SmurfTracker::onUnload()
{
	if (logFile.is_open()) {
		logFile.close();
	}
	LOG("SmurfTracker unloaded!");
}
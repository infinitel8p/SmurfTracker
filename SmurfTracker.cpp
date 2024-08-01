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

	cvarManager->registerCvar("SmurfTracker_check_teammates", "1", "Check teammates stats", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		checkTeammates = cvar.getBoolValue();
	});

	cvarManager->registerCvar("SmurfTracker_check_self", "1", "Check your own stats", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		checkSelf = cvar.getBoolValue();
	});

	cvarManager->registerNotifier("InitializeCurrentPlayers", [this](std::vector<std::string> args) {
		InitializeCurrentPlayers();
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("TestHTTPRequest", [this](std::vector<std::string> args) {
		HTTPRequest();
		}, "", PERMISSION_ALL);

	// Hook into the OnAllTeamsCreated event to log when all teams are created	
	//gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnAllTeamsCreated", [this](std::string eventName) {
	//	LOG("Initialize Game Session");
	//	cvarManager->executeCommand("InitializeCurrentPlayers");
	//	});

	gameWrapper->HookEvent("Function TAGame.Team_TA.PostBeginPlay", [this](std::string eventName) {
		LOG("Initialize Game Session");
		cvarManager->executeCommand("InitializeCurrentPlayers");
		});

	// Hook into the OnOpenScoreboard event to display player IDs when the scoreboard is opened
	gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnOpenScoreboard", [this](std::string eventName) {
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
		details.uniqueID = uniqueIDString; // Unique ID format: Platform|UniqueID|PlayerIndex
		details.playerIndex = static_cast<int>(i);  // Assuming i is the player index
		details.team = playerWrapper.GetTeamNum();  // 0 is blue, 1 is orange 11
		details.mmr = std::to_string(static_cast<int>(std::round(gameWrapper->GetMMRWrapper().GetPlayerMMR(playerWrapper.GetUniqueIdWrapper(), 11))));// 11 is playlist ID for ranked 2v2
		details.wins = "Waiting..."; // Default value	

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

	UpdateTeamStrings();

	int currentMode = cvarManager->getCvar("SmurfTracker_mode").getIntValue();
	if (currentMode == 2) {
		HTTPRequest();
	}
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

	auto processPlayer = std::make_shared<std::function<void(std::shared_ptr<std::set<std::string>>)>>();

	*processPlayer = [this, processPlayer](std::shared_ptr<std::set<std::string>> processedPlayers) {
		for (auto& player : currentPlayers) {
			if (processedPlayers->count(player.playerName)) {
				continue; // Skip already processed players
			}
			if (player.requested) {
				continue; // Skip players that have already been requested
			}

			processedPlayers->insert(player.playerName);
			player.requested = true;

			std::string ipAddress = cvarManager->getCvar("SmurfTracker_ip").getStringValue();
			std::string url = "http://" + ipAddress + ":8191/v1";
			std::string platform = player.platform == "XboxOne" ? "Xbox" : player.platform;
			std::string playerName = player.playerName;
			std::string targetUrl = "https://rlstats.net/profile/" + platform + "/" + urlEncode(playerName);
			player.wins = "Searching...";
			LOG("Requesting stats for: " + playerName);

			nlohmann::json data;
			data["cmd"] = "request.get";
			data["url"] = targetUrl;
			data["maxTimeout"] = 60000;

			LOG(getCurrentTime() + " Sending stats request: " + targetUrl);

			CurlRequest req;
			req.url = url;
			req.body = data.dump();
			req.headers["Content-Type"] = "application/json";

			auto weakProcessPlayer = std::weak_ptr<std::function<void(std::shared_ptr<std::set<std::string>>)>>
				(processPlayer);

			HttpWrapper::SendCurlRequest(req, [this, weakProcessPlayer, processedPlayers, playerName](int code, std::string response) {
				if (auto processPlayer = weakProcessPlayer.lock()) {
					if (code == 200) {
						try {
							auto response_data = nlohmann::json::parse(response);
							std::string wins = response_data["wins"];
							for (auto& p : currentPlayers) {
								if (p.playerName == playerName) {
									p.wins = wins;
									LOG(playerName + " - Wins: " + wins);
									break;
								}
							}
						}
						catch (const nlohmann::json::exception& e) {
							LOG(std::string("JSON parsing error: ") + e.what());
							for (auto& p : currentPlayers) {
								if (p.playerName == playerName) {
									p.wins = "Error";
									break;
								}
							}
						}
					}
					else {
						LOG("Request failed with code: " + std::to_string(code));
						for (auto& p : currentPlayers) {
							if (p.playerName == playerName) {
								p.wins = "Error: " + std::to_string(code);
								break;
							}
						}
					}

					gameWrapper->SetTimeout([processPlayer, processedPlayers](...) {
						(*processPlayer)(processedPlayers);
						}, 1.0f);
				}
				});

			break; // Exit after sending the request for the first player found
		}
		};

	auto processedPlayers = std::make_shared<std::set<std::string>>();
	(*processPlayer)(processedPlayers);
}

void SmurfTracker::Render(CanvasWrapper canvas)
{
	if (!smurfTrackerEnabled || !isSBOpen || (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay()) || gameWrapper->IsInReplay()) {
		return;
	}

	// selected mode to display
	int currentMode = cvarManager->getCvar("SmurfTracker_mode").getIntValue();
	const char* items[] = { "Score", "MMR", "Wins" }; // Modes
	LinearColor white{ 255, 255, 255, 255 };
	LinearColor blue{ 0, 0, 255, 255 };
	LinearColor orange{ 255, 165, 0, 255 };

	int bluePos[3] = { 380, 429, 478 };
	int orangePos[3] = { 615, 663, 710 };

	if (currentPlayers.size() <= 2) {
		bluePos[0] = 488;
		orangePos[0] = 615;
	}
	if (currentPlayers.size() <= 4 && currentPlayers.size() > 2) {
		bluePos[0] = 435;
		bluePos[1] = 483;
		orangePos[0] = 615;
		orangePos[1] = 663;
	}
	if (currentPlayers.size() > 4) {
		bluePos[0] = 380;
		bluePos[1] = 429;
		bluePos[2] = 478;
		orangePos[0] = 615;
		orangePos[1] = 663;
		orangePos[2] = 710;
	}

	// Map for quick lookup of player details by name
	std::unordered_map<std::string, PlayerDetails> playerDetailsMap;
	for (const auto& player : currentPlayers) {
		playerDetailsMap[player.playerName] = player;
	}

	// Display header information
	canvas.SetColor(white);
	canvas.SetPosition(Vector2(1440, 0));
	canvas.DrawString("Connected Players: " + std::to_string(currentPlayers.size()) + " Mode: " + items[currentMode], 2.0, 2.0, true, true);

	auto drawTeam = [&](const std::vector<std::string>& team, const LinearColor& color, int* positions) {
		canvas.SetColor(color);
		std::string teamName = (color == blue) ? "Blue:" : "Orange:";
		canvas.SetPosition(Vector2(1440, positions[0] - 50));
		canvas.DrawString(teamName, 1.5, 1.5, true);

		canvas.SetColor(white);
		int index = 0;
		for (const auto& player : team) {
			auto it = playerDetailsMap.find(player);
			if (it != playerDetailsMap.end()) {
				const auto& playerDetails = it->second;
				std::string displayString = player;
				if (currentMode == 0) {
					displayString += " - Score: " + std::to_string(playerDetails.currentScore);
				}
				else if (currentMode == 1) {
					displayString += " - MMR: " + playerDetails.mmr;
				}
				else if (currentMode == 2) {
					displayString += " - Wins: " + playerDetails.wins;
				}
				canvas.SetPosition(Vector2(1440, positions[index]));
				canvas.DrawString(displayString, 1.5, 1.5, true);
				index++;
			}
		}
		};

	drawTeam(blueTeam, blue, bluePos);
	drawTeam(orangeTeam, orange, orangePos);
}

void SmurfTracker::onUnload()
{
	if (logFile.is_open()) {
		logFile.close();
	}
	LOG("SmurfTracker unloaded!");
}
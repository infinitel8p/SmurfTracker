#include "pch.h"
#include "SmurfTracker.h"

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

	cvarManager->registerNotifier("DisplayPlayerIDs", [this](std::vector<std::string> args) {
		DisplayPlayerIDs();
		}, "", PERMISSION_ALL);

	cvarManager->registerNotifier("InitializeCurrentPlayers", [this](std::vector<std::string> args) {
		InitializeCurrentPlayers();
		}, "", PERMISSION_ALL);

	// Updated the HTTPRequest notifier to always expect a URL argument
	cvarManager->registerNotifier("HTTPRequest", [this](std::vector<std::string> args) {
		if (!args.empty()) {
			std::string url = args[1];
			// Replace the custom delimiter with the correct one
			std::replace(url.begin(), url.end(), '|', '/');
			HTTPRequest(url);
		}
		else {
			LOG("No URL provided for HTTPRequest!");
		}
		}, "", PERMISSION_ALL);

	// Hook into the OnAllTeamsCreated event to log when all teams are created	
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnAllTeamsCreated", [this](std::string eventName) {
		LOG("Initialize Game Session");
		cvarManager->executeCommand("InitializeCurrentPlayers");
		});

	// Hook into the StartRound event to display player IDs at the start of every kickoff
	//gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound", [this](std::string eventName) {
	//	cvarManager->executeCommand("DisplayPlayerIDs");
	//	});

	// Hook into the BeginState event to display player IDs at the start of a kickoff countdown or beginning/reset of freeplay
	//gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Countdown.BeginState", [this](std::string eventName) {
	//	cvarManager->executeCommand("DisplayPlayerIDs");
	//	});

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

		// Extract player information (you can add more fields as needed)
		PlayerDetails details;
		details.playerName = playerWrapper.GetPlayerName().ToString();
		details.uniqueID = uniqueIDString;
		details.playerIndex = static_cast<int>(i);  // Assuming i is the player index
		details.wins = 0;  // TODO: Get wins from tracker network or other source
		details.team = playerWrapper.GetTeamNum();  // 0 is blue, 1 is orange

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

		LogF("Player name: " + details.playerName +
			" | Platform: " + details.platform +
			" | ID: " + details.uniqueID +
			" | PlayerIndex: " + std::to_string(details.playerIndex));

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
}

void SmurfTracker::ClearCurrentPlayers()
{
	currentPlayers.clear();
}

void SmurfTracker::UpdatePlayerList() {
	//TODO: Check if all players are still connected and/or if new players have joined

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

	// Get the array of players
	ArrayWrapper<PriWrapper> players = sw.GetPRIs();

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

	// Sort the players
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

void SmurfTracker::HTTPRequest(const std::string& url)
{
	// Create a CurlRequest
	CurlRequest req;
	req.url = url;
	req.body = "testing with body";

	// Create a shared pointer to manage the log file object
	auto logFile = std::make_shared<std::ofstream>("SmurfTracker.log", std::ios::app);

	*logFile << "[" << getCurrentTime() << "] Start of request" << std::endl;

	// Define the callback function, capturing logFile by value
	auto callback = [logFile](int code, std::string result) {
		LOG("Body result{}", result);
		*logFile << "[" << getCurrentTime() << "] Response: " << result << std::endl;
	};

	// Send the request using BakkesMod's HttpWrapper
	LOG("sending body request");
	HttpWrapper::SendCurlRequest(req, callback);

	*logFile << "[" << getCurrentTime() << "] End of request" << std::endl;
}

void SmurfTracker::Render(CanvasWrapper canvas)
{
	if (!smurfTrackerEnabled || !isSBOpen) {
		return;
	}

	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay() || gameWrapper->IsInReplay()) {
		return;
	}

	if (currentPlayers.size() < 1) {
		InitializeCurrentPlayers();
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
		return;
	}

	if (currentPlayers.size() < sw.GetPRIs().Count()) {
		LogF("Not all players found! " + std::to_string(currentPlayers.size()) + "in Array vs.: " + std::to_string(sw.GetPRIs().Count()));
		InitializeCurrentPlayers();
		return;
	}

	// Get the screen size
	int screenWidth = canvas.GetSize().X;
	int screenHeight = canvas.GetSize().Y;

	// Calculate the position next to the scoreboard
	Vector2 scoreboardPosition(screenWidth - 500, screenHeight / 2);

	std::string TextToDisplay = "Connected Players:" + std::to_string(currentPlayers.size()) + "/" + std::to_string(sw.GetPRIs().Count());
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

		HTTPRequest("https://jsonplaceholder.typicode.com/users");
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
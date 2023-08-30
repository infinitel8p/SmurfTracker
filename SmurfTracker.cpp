#include "pch.h"
#include "SmurfTracker.h"
#include <fstream>
#include <ctime>
#include <iomanip>

BAKKESMOD_PLUGIN(SmurfTracker, "Identify Smurfs.", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
bool smurfTrackerEnabled = false;

struct PlayerDetails {
	std::string playerName;
	std::string platform;
	std::string uniqueID;
	int playerIndex = 0;
	int wins = 0;	
};

std::string getCurrentTime() {
	std::time_t t = std::time(nullptr);
	std::tm tm = *std::localtime(&t);
	std::stringstream ss;
	ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return ss.str();
}

void SmurfTracker::onLoad()
{
	// Register the render function to be called each frame
	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		Render(canvas);
		});

	_globalCvarManager = cvarManager;
	LOG("SmurfTracker loaded!");
	DEBUGLOG("SmurfTracker debug mode enabled"); // logging.h DEBUG_LOG = true;
	
	cvarManager->registerCvar("SmurfTracker_enabled", "0", "Enable SmurfTracker Plugin", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		smurfTrackerEnabled = cvar.getBoolValue();
	});

	cvarManager->registerNotifier("DisplayPlayerIDs", [this](std::vector<std::string> args) {
		DisplayPlayerIDs();
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

	// Hook into the StartRound event to display player IDs at the start of every kickoff
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound", [this](std::string eventName) {
		cvarManager->executeCommand("DisplayPlayerIDs");
		});

	// Hook into the BeginState event to display player IDs at the start of a kickoff countdown or beginning/reset of freeplay
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Countdown.BeginState", [this](std::string eventName) {
		cvarManager->executeCommand("DisplayPlayerIDs");
		});

	// Hook into the OnOpenScoreboard event to display player IDs when the scoreboard is opened
	gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnOpenScoreboard", [this](std::string eventName) {
		cvarManager->executeCommand("DisplayPlayerIDs");
		});

	// Hook into the OnCloseScoreboard event to log a message when the scoreboard is closed
	gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnCloseScoreboard", [this](std::string eventName) {
		LOG("Scoreboard closed!");
		});
}

void SmurfTracker::onUnload()
{
	LOG("SmurfTracker unloaded!");
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

// Function to render text next to the scoreboard
void SmurfTracker::Render(CanvasWrapper canvas)
{
	// defines colors in RGBA 0-255
	LinearColor colors;
	colors.R = 255;
	colors.G = 255;
	colors.B = 255;
	colors.A = 255;
	canvas.SetColor(colors);

	// Get the screen size
	int screenWidth = canvas.GetSize().X;
	int screenHeight = canvas.GetSize().Y;

	// Calculate the position next to the scoreboard
	Vector2 scoreboardPosition(screenWidth - 500, screenHeight / 2);

	std::string TextToDisplay = "SmurfTracker";
	
	// Draw the text
	canvas.SetPosition(scoreboardPosition);
	canvas.DrawString(TextToDisplay);
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

	// Get the array of players
	ArrayWrapper<PriWrapper> players = sw.GetPRIs();

	// Open the log file in append mode (Location is Epic Games\rocketleague\Binaries\Win64\SmurfTracker.log)
	std::ofstream logFile("SmurfTracker.log", std::ios::app);

	std::map<std::string, PlayerDetails> uniqueIDMap;

	// Iterate through the players
	for (size_t i = 0; i < players.Count(); i++)
	{
		PriWrapper playerWrapper = players.Get(i);
		if (playerWrapper.IsNull()) continue;

		UniqueIDWrapper uniqueID = playerWrapper.GetUniqueIdWrapper();
		std::string uniqueIDString = uniqueID.GetIdString();

		// Find separators
		size_t firstSeparator = uniqueIDString.find('|');
		size_t secondSeparator = uniqueIDString.find('|', firstSeparator + 1);

		// Check for valid separators
		if (firstSeparator == std::string::npos || secondSeparator == std::string::npos || firstSeparator >= secondSeparator) {
			LOG("Invalid unique ID format: " + uniqueIDString);
			continue; // Skip this player if the unique ID format is not as expected
		}

		std::string platform = uniqueIDString.substr(0, firstSeparator);
		std::string uniqueIDPart = uniqueIDString.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
		int playerIndex = static_cast<int>(std::stoi(uniqueIDString.substr(secondSeparator + 1)));

		PlayerDetails details;

		// If this is a main player, store the details
		if (playerIndex == 0) {
			details = { playerWrapper.GetPlayerName().ToString(), platform, uniqueIDPart, playerIndex };
			uniqueIDMap[uniqueIDPart] = details;
		}
		// If this is a splitscreen player, retrieve the main player's details
		else {
			// Check if the main player's details are in the map
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

		// Log the details
		std::string logMessage = "Player name: " + details.playerName +
			" | Platform: " + details.platform +
			" | ID: " + details.uniqueID +
			" | PlayerIndex: " + std::to_string(details.playerIndex);
		LOG(logMessage);
		logFile << "\n[" << getCurrentTime() << "] " << logMessage << std::endl;

		// Check the platform in the unique ID and log the appropriate request URL
		if (uniqueIDString.find("Epic") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/epic/" + details.playerName);
		}
		else if (uniqueIDString.find("PS4") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/psn/" + details.playerName);
		}
		else if (uniqueIDString.find("Switch") != std::string::npos) {
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
#include "pch.h"
#include "SmurfTracker.h"


BAKKESMOD_PLUGIN(SmurfTracker, "Identify Smurfs.", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
bool smurfTrackerEnabled = false;

void SmurfTracker::onLoad()
{
	_globalCvarManager = cvarManager;
	LOG("SmurfTracker loaded!");
	
	cvarManager->registerCvar("SmurfTracker_enabled", "0", "Enable SmurfTracker Plugin", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		smurfTrackerEnabled = cvar.getBoolValue();
	});

	cvarManager->registerNotifier("DisplayPlayerIDs", [this](std::vector<std::string> args) {
		DisplayPlayerIDs();
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

	// !! Enable debug logging by setting DEBUG_LOG = true in logging.h !!
	//DEBUGLOG("SmurfTracker debug mode enabled");

	// LOG and DEBUGLOG use fmt format strings https://fmt.dev/latest/index.html
	//DEBUGLOG("1 = {}, 2 = {}, pi = {}, false != {}", "one", 2, 3.14, true);

	//cvarManager->registerNotifier("my_aweseome_notifier", [&](std::vector<std::string> args) {
	//	LOG("Hello notifier!");
	//}, "", 0);
	// 
	//auto cvar = cvarManager->registerCvar("template_cvar", "hello-cvar", "just a example of a cvar");
	//auto cvar2 = cvarManager->registerCvar("template_cvar2", "0", "just a example of a cvar with more settings", true, true, -10, true, 10 );

	//cvar.addOnValueChanged([this](std::string cvarName, CVarWrapper newCvar) {
	//	LOG("the cvar with name: {} changed", cvarName);
	//	LOG("the new value is: {}", newCvar.getStringValue());
	//});

	//cvar2.addOnValueChanged(std::bind(&SmurfTracker::YourPluginMethod, this, _1, _2));

	// enabled decleared in the header
	//enabled = std::make_shared<bool>(false);
	//cvarManager->registerCvar("TEMPLATE_Enabled", "0", "Enable the TEMPLATE plugin", true, true, 0, true, 1).bindTo(enabled);

	//cvarManager->registerNotifier("NOTIFIER", [this](std::vector<std::string> params){FUNCTION();}, "DESCRIPTION", PERMISSION_ALL);
	//cvarManager->registerCvar("CVAR", "DEFAULTVALUE", "DESCRIPTION", true, true, MINVAL, true, MAXVAL);//.bindTo(CVARVARIABLE);
	//gameWrapper->HookEvent("FUNCTIONNAME", std::bind(&TEMPLATE::FUNCTION, this));
	//gameWrapper->HookEventWithCallerPost<ActorWrapper>("FUNCTIONNAME", std::bind(&SmurfTracker::FUNCTION, this, _1, _2, _3));
	//gameWrapper->RegisterDrawable(bind(&TEMPLATE::Render, this, std::placeholders::_1));


	//gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", [this](std::string eventName) {
	//	LOG("Your hook got called and the ball went POOF");
	//});
	// You could also use std::bind here
	//gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", std::bind(&SmurfTracker::YourPluginMethod, this);
}

void SmurfTracker::onUnload()
{
	LOG("SmurfTracker unloaded!");
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
	//TODO: Toggle Function when scoreboard is open
	ArrayWrapper<PriWrapper> players = sw.GetPRIs();

	// Iterate through the players
	for (size_t i = 0; i < players.Count(); i++)
	{
		PriWrapper playerWrapper = players.Get(i);
		if (playerWrapper.IsNull()) continue;

		UniqueIDWrapper uniqueID = playerWrapper.GetUniqueIdWrapper();
		std::string playerName = playerWrapper.GetPlayerName().ToString();
		std::string uniqueIDString = uniqueID.GetIdString();

		// Log the player's name and ID to the console
		LOG("Player name: " + playerName + " | ID: " + uniqueIDString);

		// Check the platform in the unique ID and log the appropriate request URL
		if (uniqueIDString.find("Epic") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/epic/" + playerName);
		}
		else if (uniqueIDString.find("PS4") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/psn/" + playerName);
		}
		else if (uniqueIDString.find("Switch") != std::string::npos) {
			LOG("Request: https://rocketleague.tracker.network/rocket-league/profile/switch/" + playerName);
		}

		// https://rocketleague.tracker.network/rocket-league/profile/steam/76561198138690072/overview ????-steam|uniqueID
		// https://rocketleague.tracker.network/rocket-league/profile/xbl/RocketLeague893/overview ????-xbl|xbl-username
		// https://rocketleague.tracker.network/rocket-league/profile/epic/test/overview Epic-epic|epic-username
		// https://rocketleague.tracker.network/rocket-league/profile/psn/RocketLeagueNA/overview PS4-psn|psn-username
		// https://rocketleague.tracker.network/rocket-league/profile/switch/test/overview Switch-switch|switch-username

		// TODO: Determine the position to draw the ID
		// TODO: Use the correct method to draw the string on the canvas
	}
}
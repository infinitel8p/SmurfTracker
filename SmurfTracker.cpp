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

	// Get the array of players
	ArrayWrapper<PriWrapper> players = gameWrapper->GetGameEventAsServer().GetPRIs();

	// Iterate through the players
	for (int i = 0; i < players.Count(); i++) {
		PriWrapper player = players.Get(i);
		if (player.IsNull()) continue;

		// Get the player's name
		std::string playerName = player.GetPlayerName().ToString();
		
		// Get the player's unique ID
		UniqueIDWrapper uniqueID = player.GetUniqueIdWrapper();
		std::string playerID = uniqueID.GetIdString(); // Assuming GetIdString() returns the ID as a string

		// Log the player's name and ID to the console
		LOG("Player name: " + playerName + " | ID: " + playerID);

		// TODO: Determine the position to draw the ID
		// TODO: Use the correct method to draw the string on the canvas
	}
}
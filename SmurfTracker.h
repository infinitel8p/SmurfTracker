#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class SmurfTracker: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase
	//,public PluginWindowBase // Uncomment if you want to render your own plugin window
{

	//std::shared_ptr<bool> enabled;

	//Boilerplate
	void onLoad() override;
	void onUnload() override;
	void DisplayPlayerIDs();
	void HTTPRequest(const std::string& url);
	void Render(CanvasWrapper canvas);


public:
	void RenderSettings() override;
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window
};
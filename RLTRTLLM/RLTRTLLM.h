#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class RLTRTLLM: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase // Uncomment if you wanna render your own tab in the settings menu
	//,public PluginWindowBase // Uncomment if you want to render your own plugin window
{

	//std::shared_ptr<bool> enabled;
	std::string message_from_llm = "";
	std::string sanitized_message = ""; 
	bool first_touch = false;

	// bot name -- updated when the game starts
	std::string bot_name = "AI";
	std::string player_name = "ME";

	//Boilerplate
	void onLoad() override;
	//void onUnload() override; // Uncomment and implement if you need a unload method
	void makeRequest();
	void sendMessage(std::string message);
	void onStatTickerMessage(void* params);
	void onStatEvent(void* params);
	void appendToPrompt(std::string message, std::string role);

	// these methods are used for santizing unwanted characters from LLM responses
	std::string removeEmojiCharacters(std::string input);
	std::string removeDoubleQuotes(std::string str);
	std::string removeTag(std::string input);
	std::string sanitizeMessage(std::string message);


public:
	void RenderSettings() override; // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window
};

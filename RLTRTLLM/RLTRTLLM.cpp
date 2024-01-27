#include "pch.h"
#include "RLTRTLLM.h"

#include <json.hpp>

using json = nlohmann::json;


BAKKESMOD_PLUGIN(RLTRTLLM, "Rocket League Tensor-RT LLM v2", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;


void RLTRTLLM::onLoad()
{
	_globalCvarManager = cvarManager;
	//LOG("Plugin loaded!");
	// !! Enable debug logging by setting DEBUG_LOG = true in logging.h !!
	DEBUGLOG("RLTRTLLM debug mode enabled");

	//gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", [this](std::string eventName) {
	//	LOG("Your hook got called and the ball went POOF");
	//});
	// You could also use std::bind here
	//gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", std::bind(&RLTRTLLM::YourPluginMethod, this);
	cvarManager->registerCvar("system_prompt", "You are an a player in a game called Rocket League. You are not a person, but you are a program. You should respond to prompts in short messages under 100 characters in lenght. You can be funny and dorky, but also really cool.", "System Prompt");
	cvarManager->registerCvar("prompt", "You just scored another goal on your opponent.", "Prompt to use for OpenAI API request");
	cvarManager->registerCvar("temperature", "Temperature to use for OpenAI API request");
	cvarManager->registerCvar("seed", "1", "Seed to use for OpenAI API request");
	cvarManager->registerCvar("isReplay", "0", "Replay boolean", true, true, 0, true, 1);

	/*gameWrapper->HookEventWithCaller()*/
		//Check if it is a replay
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlaybackReplayPlayback.BeginState", std::bind(&RLTRTLLM::Replay, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.ReplayPlayback.EndState", std::bind(&RLTRTLLM::NotReplay, this, std::placeholders::_1));
	
	// Triggered with Console for testing
	cvarManager->registerNotifier("MakeRequest", [this](std::vector<std::string> args) {
		makeRequest();
	}, "", PERMISSION_ALL);
	
	// hook goal event
	// hooked whenever the primary player earns a stat
	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatTickerMessage",
		[this](ServerWrapper caller, void* params, std::string eventname) {
			onStatEvent(params);
		});

	//cvarManager->registerCvar("cool_enabled", "0", "Enable Cool", true, true, 0, true, 1)
	//	.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
	//	coolEnabled = cvar.getBoolValue();
	//		});

	// gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.HUDBase_TA.OnChatMessage", bind(&RLTRTLLM::chatMessageEvent, this, std::placeholders::_1, std::placeholders::_2));
}

void RLTRTLLM::makeRequest() {
	if (!gameWrapper->IsInGame()) { return; }

	CVarWrapper promptCVar = cvarManager->getCvar("prompt");
	if (!promptCVar) { return; }
	std::string prompt = promptCVar.getStringValue();
	LOG("{}", prompt);

	CVarWrapper systemPromptCvar = cvarManager->getCvar("system_prompt");
	if (!systemPromptCvar) { return; }
	std::string system_prompt = systemPromptCvar.getStringValue();
	LOG("{}", system_prompt);

	CVarWrapper temperatureCVar = cvarManager->getCvar("temperature");
	if (!temperatureCVar) { return; }
	std::int16_t temperature = temperatureCVar.getIntValue();
	LOG("{}", temperature);

	CVarWrapper seedCVar = cvarManager->getCvar("seed");
	if (!seedCVar) { return; }
	std::int16_t seed = seedCVar.getIntValue();
	LOG("{}", seed);

	// define body to send in curl request
	json request_body;
	request_body["model"] = "code-llama";
	request_body["temperature"] = temperature;
	request_body["messages"] = { { { "role", "system" },{ "content", system_prompt } }, { { "role", "user" },{ "content", prompt } } };
	request_body["seed"] = seed;
	request_body["max_tokens"] = 100;

	// serialize json to string
	std::string json_string_of_request_body = request_body.dump();
	LOG("{}", json_string_of_request_body);

	CurlRequest req;
	req.url = "http://localhost:5000/v1/chat/completions";
	req.body = json_string_of_request_body;

	HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
		{
			try {
				LOG("Json result{}", result);
				nlohmann::json json = nlohmann::json::parse(result);
				std::string req_id = json["id"];
				LOG("Request id: {}", req_id);

				// read message content from OpenAI API style request
				std::string message = json["choices"][0]["message"]["content"];
				std::string message_to_log = message.substr(0, 100);

				this->mes = message_to_log;
				LOG("message {}", mes);

				gameWrapper->Execute([this](GameWrapper* gw) {
					gameWrapper->LogToChatbox(this->mes);
					});

			} catch (std::exception e) {
				LOG("{}", e.what());
			}
		});
}

void RLTRTLLM::Replay(std::string name)
{
	//LOG("Replay start");
	CVarWrapper replayCvar = cvarManager->getCvar("isReplay");
	bool isReplay = replayCvar.getBoolValue();
	isReplay = true;
	//LOG("{}", isReplay);
	replayCvar.setValue(isReplay);
}

void RLTRTLLM::NotReplay(std::string name)
{
	//LOG("Replay ended");
	CVarWrapper replayCvar = cvarManager->getCvar("isReplay");
	bool isReplay = replayCvar.getBoolValue();
	isReplay = false;
	//LOG("{}", isReplay);
	replayCvar.setValue(isReplay);

}

void RLTRTLLM::onStatEvent(void* params) {
	struct StatTickerParams {
		uintptr_t Receiver;
		uintptr_t Victim;
		uintptr_t StatEvent;
	};

	StatTickerParams* pStruct = (StatTickerParams*)params;
	PriWrapper primary = PriWrapper(pStruct->Receiver);
	StatEventWrapper statEvent = StatEventWrapper(pStruct->StatEvent);
	LOG("some stat event was triggered");
	if (statEvent.GetEventName() == "Goal") {
		LOG("A goal was scored");
		if (!primary) { LOG("Null reciever PRI"); return; }

		// Find the primary player's PRI
		PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
		if (!playerController) { LOG("Null controller"); return; }
		PriWrapper playerPRI = playerController.GetPRI();
		if (!playerPRI) { LOG("Null player PRI"); return; }

		// Compare the primary player to the victim
		if (playerPRI.memory_address == primary.memory_address) {
			//LOG("Hah you got demoed get good {}", victim.GetPlayerName().ToString());
			LOG("I scored!");
			makeRequest();
		}
		else {
			LOG("They scored!!");
			makeRequest();
		}


		// Primary player is the victim!
		//LOG("I was demoed!!! {} is toxic I'm uninstalling", receiver.GetPlayerName().ToString());
	}

	if (statEvent.GetEventName() == "OwnGoal") {
		LOG("I scored on myself");
	}
}

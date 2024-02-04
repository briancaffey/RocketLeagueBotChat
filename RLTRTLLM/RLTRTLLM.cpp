#pragma execution_character_set("utf-8")

#include "pch.h"
#include "RLTRTLLM.h"

#include <json.hpp>

using json = nlohmann::json;


BAKKESMOD_PLUGIN(RLTRTLLM, "Rocket League Tensor-RT LLM v2", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void RLTRTLLM::onLoad()
{
	_globalCvarManager = cvarManager;

	// !! Enable debug logging by setting DEBUG_LOG = true in logging.h !!
	DEBUGLOG("RLTRTLLM debug mode enabled");

	// system prompt setup
	std::string initial_system_prompt = "You are an AI player in the car soccer game Rocket League. You will react to the events described with short one-sentence chat messages.";
	
	// messages (including system prompt and all history) are stored in a cvar that holds a json string 
	cvarManager->registerCvar("message_json_string", "[]");

	// system_prompt is a plain string value.
	// When a user updates the system_prompt cvar value, it resets the message_json_string cvar to only include the new system prompt
	cvarManager->registerCvar("system_prompt", "").addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {

		// 
		CVarWrapper messageJsonStringCvar = cvarManager->getCvar("message_json_string");
		if (!messageJsonStringCvar) { return; }
		std::string messages_string = messageJsonStringCvar.getStringValue();
		//json messages_json = json::parse(messages_string);
		json reset_messages_json = { { { "role", "system"}, {"content", cvar.getStringValue()} } };
		std::string message_json_string = reset_messages_json.dump(2);
		messageJsonStringCvar.setValue(message_json_string);
		});

	// setting this initial value for the system_prompt cvar should update the message_json_string cvar value
	CVarWrapper systemPromptCvar = cvarManager->getCvar("system_prompt");
	systemPromptCvar.setValue(initial_system_prompt);

	cvarManager->registerCvar("temperature", "Temperature to use for OpenAI API request");
	cvarManager->registerCvar("seed", "1", "Seed to use for OpenAI API request");
	
	// A simple console command for testing
	cvarManager->registerNotifier("MakeRequest", [this](std::vector<std::string> args) {
		makeRequest();
	}, "", PERMISSION_ALL);

	// Triggered with Console for testing
	cvarManager->registerNotifier("SendMessage", [this](std::vector<std::string> args) {
		std::string message = std::accumulate(args.begin() + 1, args.end(), std::string(),
		[](std::string a, std::string b) {
				return a + " " + b;
			});
		LOG("{}", message);
		sendMessage(message);
		}, "", PERMISSION_ALL);

	// Hooks different types of events that are handled in onStatEvent
	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatTickerMessage",
		[this](ServerWrapper caller, void* params, std::string eventname) {
			onStatEvent(params);
		});
}

/**
 * sendMessage
 * 
 * This method allows a player to send a message to the bot
 * In offline matches the chat window is not enabled, so this is a simple workaround
 * To send a message press F6 to bring up the console and type SendMessage <your message>
 * Your message will be added to the prompt as a new line and and then a new LLM request will be made
 */
void RLTRTLLM::sendMessage(std::string message) {
	gameWrapper->LogToChatbox(message, "me");
	std::string message_to_add = "Your opponent said: " + message;
	appendToPrompt(message_to_add, "user");
	makeRequest();
}

/**
 * MakeRequest
 * 
 * The method is used for sending a request to the LLM
 * It reads CVar values for the prompt, system prompt and other parameters for the LLM request
 * Requests are sent to the LLM and responses are sent to the chat box
 */
void RLTRTLLM::makeRequest() {
	if (!gameWrapper->IsInGame()) { return; }

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
	request_body["seed"] = seed;
	request_body["max_tokens"] = 100;
	
	// messages
	CVarWrapper messageJsonStringCVar = cvarManager->getCvar("message_json_string");
	if (!messageJsonStringCVar) { return; }
	json messages = json::parse(messageJsonStringCVar.getStringValue());
	request_body["messages"] = messages;

	// serialize json to string
	std::string json_string_of_request_body = request_body.dump(2);
	LOG("{}", json_string_of_request_body);

	CurlRequest req;
	req.url = "http://localhost:5001/v1/chat/completions";
	req.body = json_string_of_request_body;

	// this is a wrapper from the BakkesMod plugin for making web requests
	HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result) {
		try {
			LOG("Json result{}", result);
			json response_json = json::parse(result);
			std::string req_id = response_json["id"];
			LOG("Request id: {}", req_id);

			// read message content from OpenAI API style request
			std::string message = response_json["choices"][0]["message"]["content"];

			// sanitize message (remove emoji, </s>, etc. since these characters can't be displayed in the in-game chat box)
			std::string sanitized_message = message; //sanitizeMessage(message);

			// ensure that the message to log to the chatbox is not longer than the limit of 120 characters
			std::string message_to_log = sanitized_message.substr(0, 120);

			this->mes = message_to_log;
			LOG("message {}", mes);

			gameWrapper->Execute([this](GameWrapper* gw) {

				// displays the message on the HUD
				gameWrapper->LogToChatbox(this->mes);

				// add the response to the messages array to keep it in chat history
				appendToPrompt(this->mes, "assistant");
				});

		} catch (std::exception e) {
			LOG("{}", e.what());
		}
	});
}


/**
 * appendToPrompt
 * 
 * This is a utility method for adding new messages to the prompt that is used for inference
 * 
 * The role parameter defaults to user, we only set it to assistant when a response comes back
 * 
 */
void RLTRTLLM::appendToPrompt(std::string message, std::string role = "user") {
	CVarWrapper messageJsonStringCVar = cvarManager->getCvar("message_json_string");
	if (!messageJsonStringCVar) { return; }
	json messageJson = json::parse(messageJsonStringCVar.getStringValue());
	messageJson.push_back({ {"role", role}, {"content", message } });
	messageJsonStringCVar.setValue(messageJson.dump(2));
}


/** 
 * onStatEvent
 * 
 * This method handles most game events that trigger a response from the LLM
 * 
 * Reference from Bakkesmod docs: https://wiki.bakkesplugins.com/functions/stat_events/
 */
void RLTRTLLM::onStatEvent(void* params) {

	LOG("handling StatEvent");


	struct StatTickerParams {
		uintptr_t Receiver;
		uintptr_t Victim;
		uintptr_t StatEvent;
	};

	StatTickerParams* pStruct = (StatTickerParams*)params;
	PriWrapper receiver = PriWrapper(pStruct->Receiver);
	if (!receiver) { LOG("Null reciever PRI"); return; }
	PriWrapper victim = PriWrapper(pStruct->Victim);
	StatEventWrapper statEvent = StatEventWrapper(pStruct->StatEvent);

	// Find the primary player's PRI
	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) { LOG("Null controller"); return; }
	PriWrapper playerPRI = playerController.GetPRI();
	if (!playerPRI) { LOG("Null player PRI"); return; }

	// handle different events like scoring a goal or making a save
	if (statEvent.GetEventName() == "Goal") {
		LOG("A goal was scored");

		// Compare the primary player to the victim
		if (playerPRI.memory_address == receiver.memory_address) {
			//LOG("Hah you got demoed get good {}", victim.GetPlayerName().ToString());
			LOG("I scored!");
			appendToPrompt("Your human opponent just scored a goal against you!", "user");
			makeRequest();
		}
		else {
			LOG("They scored!!");
			appendToPrompt("You just scored a goal against the human player!", "user");
			makeRequest();
		}

	} else if (statEvent.GetEventName() == "Demolish") {
		LOG("a demolition happened >:(");
		if (!receiver) { LOG("Null reciever PRI"); return; }
		if (!victim) { LOG("Null victim PRI"); return; }

		// Compare the primary player to the victim
		if (playerPRI.memory_address != victim.memory_address) {
			appendToPrompt("Your human opponent demolished your car! You will now respawn.");
			makeRequest();
		}
		else {
			appendToPrompt("You just demolished the human player's car! The human player will respawn.");
			makeRequest();
		}
	} else if (statEvent.GetEventName() == "FirstTouch") {
		LOG("Handle FirstTouch event");
		if (!receiver) { LOG("Null reciever PRI"); return; }

		// Compare the primary player to the victim
		if (playerPRI.memory_address != victim.memory_address) {
			appendToPrompt("Your human opponent got first touch on the ball after face-off.");
			makeRequest();
		}
		else {
			appendToPrompt("You beat your opponent to the ball on face-off and got first touch!");
			makeRequest();
		}
	}
	
	// TODO: handle different types of events: OwnGoal, Win, etc.
}

/**
 * sanitizeMessage
 * 
 * This method removes characters that cannot be displayed in the in game chat, such as emoji characters
 * 
 * Also removes the stop character </s> and fixes other issues with the format of the strings returned from the LLM
 * 
 * TODO: implement this method
 */
std::string RLTRTLLM::sanitizeMessage(std::string message) {

	// remove emoji characters
	std::string sanitized_message = "";
	for (char c : message) {
		if ((c >= 32 && c <= 126) && !c == 219) {
			sanitized_message += c;
		}
	}

	// remove stop character </s>
	// Find the position of "</s>" in the string
	size_t pos = sanitized_message.find("</s>");

	// Check if the substring was found
	if (pos != std::string::npos) {
		// Erase "</s>" from the string
		sanitized_message.erase(pos, 4); // 4 characters are to be removed
	}
	
	return sanitized_message;
}

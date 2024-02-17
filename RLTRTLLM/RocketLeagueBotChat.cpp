#pragma execution_character_set("utf-8")

#include "pch.h"
#include "RocketLeagueBotChat.h"

#include <json.hpp>
#include <regex>

using json = nlohmann::json;


BAKKESMOD_PLUGIN(RocketLeagueBotChat, "Rocket League BotChat -- Powered by TensorRT-LLM", plugin_version, PLUGINTYPE_FREEPLAY);

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void RocketLeagueBotChat::onLoad()
{
	_globalCvarManager = cvarManager;

	// !! Enable debug logging by setting DEBUG_LOG = true in logging.h !!
	DEBUGLOG("RocketLeagueBotChat debug mode enabled");

	std::string ai_player = "You are an elite AI player in the car soccer game Rocket League. ";
	std::string one_v_one = "You are playing a 1v1 match against a human player. ";
	std::string instructions = "You will send short chat messages to your human opponent in response to what happens in the game. ";
	std::string details = "Respond to the human player with brief messages no more than 12 words long.";
	// initial system prompt
	std::string initial_system_prompt = ai_player + one_v_one + instructions + details;
	
	// messages (including system prompt and chat history) are stored in a cvar that holds a json string
	cvarManager->registerCvar("message_json_string", "[]");

	// system_prompt is a plain string value.
	// When a user updates the system_prompt cvar value, it resets the message_json_string cvar to only include the new system prompt
	cvarManager->registerCvar("system_prompt", "").addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {

		CVarWrapper messageJsonStringCvar = cvarManager->getCvar("message_json_string");
		if (!messageJsonStringCvar) { return; }
		std::string messages_string = messageJsonStringCvar.getStringValue();
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

	// Triggered with Console for testing
	cvarManager->registerNotifier("SendAI", [this](std::vector<std::string> args) {
		std::string message = std::accumulate(args.begin() + 1, args.end(), std::string(),
		[](std::string a, std::string b) {
				return a + " " + b;
			});
		gameWrapper->LogToChatbox(message, "RLB0T24");
		}, "", PERMISSION_ALL);

	// Triggered with Console for testing
	cvarManager->registerNotifier("SendMe", [this](std::vector<std::string> args) {
		std::string message = std::accumulate(args.begin() + 1, args.end(), std::string(),
		[](std::string a, std::string b) {
				return a + " " + b;
			});
	gameWrapper->LogToChatbox(message, this->player_name);
		}, "", PERMISSION_ALL);

	// Hooks different types of events that are handled in onStatTickerMessage
	// See https://wiki.bakkesplugins.com/functions/stat_events/
	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatTickerMessage",
		[this](ServerWrapper caller, void* params, std::string eventname) {
			onStatTickerMessage(params);
		});

	// Hooks different types of events that are handled in onStatEvent
	// See https://wiki.bakkesplugins.com/functions/stat_events/
	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatEvent",
		[this](ServerWrapper caller, void* params, std::string eventname) {
			onStatEvent(params);
		});

	// hook the start of a round for setting player names
	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GameMetrics_TA.StartRound",
		[this](ServerWrapper caller, void* params, std::string eventname) {
			// get the names of the human player (0) and the bot player (1)
			ServerWrapper sw = gameWrapper->GetGameEventAsServer();
			ArrayWrapper cars = sw.GetCars();
			CarWrapper playerCar = cars.Get(0);
			std::string playerName = playerCar.GetOwnerName();
			CarWrapper botCar = cars.Get(1);
			std::string botName = botCar.GetOwnerName();
			this->bot_name = botName;
			this->player_name = playerName;
		});

	// A simple console command for testing - used for debugging
	cvarManager->registerNotifier("TestCommand", [this](std::vector<std::string> args) {
		// add logic here
		}, "", PERMISSION_ALL);
}

/**
 * sendMessage
 * 
 * This method allows a player to send a message to the bot
 * In offline matches the chat window is not enabled, so this is a simple workaround
 * To send a message press F6 to bring up the console and type SendMessage <your message>
 * Your message will be added to the prompt as a new line and and then a new LLM request will be made
 */
void RocketLeagueBotChat::sendMessage(std::string message) {
	gameWrapper->LogToChatbox(message, this->player_name);
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
void RocketLeagueBotChat::makeRequest() {
	if (!gameWrapper->IsInGame()) { return; }

	CVarWrapper systemPromptCvar = cvarManager->getCvar("system_prompt");
	if (!systemPromptCvar) { return; }
	std::string system_prompt = systemPromptCvar.getStringValue();
	//LOG("{}", system_prompt);

	CVarWrapper temperatureCVar = cvarManager->getCvar("temperature");
	if (!temperatureCVar) { return; }
	std::int16_t temperature = temperatureCVar.getIntValue();
	//LOG("{}", temperature);

	CVarWrapper seedCVar = cvarManager->getCvar("seed");
	if (!seedCVar) { return; }
	std::int16_t seed = seedCVar.getIntValue();
	//LOG("{}", seed);

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
	//LOG("{}", json_string_of_request_body);

	// the request object that will be used to make a request to a local flask app
	// the flask app serves the TensorRT-LLM-powered inference engine
	CurlRequest req;
	req.url = "http://localhost:5001/v1/chat/completions";
	req.body = json_string_of_request_body;

	// this is a wrapper from the BakkesMod plugin for making web requests
	HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result) {

		try {
			//LOG("Json result{}", result);
			json response_json = json::parse(result);
			std::string req_id = response_json["id"];
			//LOG("Request id: {}", req_id);

			// read message content from OpenAI API style request
			std::string message = response_json["choices"][0]["message"]["content"];

			this->message_from_llm = message;

			// sanitize message (remove emoji, </s>, etc. since these characters can't be displayed in the in-game chat box)
			std::string sanitized_message = sanitizeMessage(message);

			// ensure that the message to log to the chatbox is not longer than the limit of 120 characters
			std::string message_to_log = sanitized_message.substr(0, 120);

			this->sanitized_message = message_to_log;

			LOG("message {}", this->sanitized_message);

			gameWrapper->Execute([this](GameWrapper* gw) {

				// split messages into smaller messages that are each under the length limit for in-game chat (120 characters)
				std::vector<std::string> messages = splitIntoSmallStrings(this->sanitized_message, 120);


				// Print the result
				for (size_t i = 0; i < messages.size(); ++i) {
					LOG("string: {}", messages[i]);
					// log each message to the chat box
					gameWrapper->LogToChatbox(messages[i], this->bot_name);
					LOG("string logged to chat box");
				}

				// displays the message on the HUD

				// add the response to the messages array to keep it in chat history
				appendToPrompt(this->message_from_llm, "assistant");

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
 * In order to not exceed the context window size,
 * adding new messages to the prompt from the user or assistant
 * should remove older messages. We still want to keep the system prompt
 */
void RocketLeagueBotChat::appendToPrompt(std::string message, std::string role = "user") {
	CVarWrapper messageJsonStringCVar = cvarManager->getCvar("message_json_string");
	if (!messageJsonStringCVar) { return; }

	json messageJson = json::parse(messageJsonStringCVar.getStringValue());
	messageJson.push_back({ {"role", role}, {"content", message } });

	// if the length of the messages array is greater than 6, we should remove an earlier messages
	int messageJsonLength = messageJson.size();

	if (messageJsonLength > 6) {
		// be sure to keep the system prompt
		messageJson.erase(messageJson.begin() + 1);
	}

	try {
		// TODO: figure out why the game crashes here sometimes
		messageJsonStringCVar.setValue(messageJson.dump(2));
	}
	catch (std::exception e) {
		gameWrapper->LogToChatbox("There was an error.. check the logs!");
		LOG("{}", e.what());
	}
}

/**
 * onStatEvent
 * 
 */
void RocketLeagueBotChat::onStatEvent(void* params) {

	// structure of a stat event
	struct StatEventParams {
		// always primary player
		uintptr_t PRI;
		// wrapper for the stat event
		uintptr_t StatEvent;
	};

	StatEventParams* pStruct = (StatEventParams*)params;
	PriWrapper playerPRI = PriWrapper(pStruct->PRI);
	StatEventWrapper statEvent = StatEventWrapper(pStruct->StatEvent);
	LOG("Handling statEvent: {}", statEvent.GetEventName());

	if (statEvent.GetEventName() == "FirstTouch") {
		LOG("First touch event triggered");
		// Compare the primary player to the victim
		appendToPrompt("Your human opponent got the first touch on the ball on the face-off.");
		makeRequest();
	}
	else if (statEvent.GetEventName() == "Clear") {
		LOG("The human player cleared the ball.");
		appendToPrompt("Your human opponent just cleared the ball away from their goal.");
		makeRequest();
	}
}

/** 
 * onStatTickerMessage
 * 
 * This method handles most game events that trigger a response from the LLM
 * 
 * Reference from Bakkesmod docs: https://wiki.bakkesplugins.com/functions/stat_events/
 */
void RocketLeagueBotChat::onStatTickerMessage(void* params) {

	LOG("handling StatTickerMessage");


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
	LOG("statEvent name is: {}", statEvent.GetEventName());

	// Find the primary player's PRI
	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) { LOG("Null controller"); return; }
	PriWrapper playerPRI = playerController.GetPRI();
	if (!playerPRI) { LOG("Null player PRI"); return; }

	// get the names of the human player (0) and the bot player (1)
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();

	// get score of the game as a sentence
	std::string score_sentence = "";
	ArrayWrapper teams = sw.GetTeams();
	TeamWrapper playerTeam = teams.Get(0);
	int playerScore = playerTeam.GetScore();
	TeamWrapper botTeam = teams.Get(1);
	int botScore = botTeam.GetScore();

	score_sentence = std::format("The score is now: Human {} - AI {}", playerScore, botScore);
	

	// handle different events like scoring a goal or making a save
	if (statEvent.GetEventName() == "Goal") {

		// was the goal scored by the human player or the bot?
		if (playerPRI.memory_address == receiver.memory_address) {
			appendToPrompt("Your human opponent just scored a goal against you! " + score_sentence, "user");
		}
		else {
			appendToPrompt("You just scored a goal against the human player! " + score_sentence, "user");
		}
	} else if (statEvent.GetEventName() == "Demolish") {
		if (!receiver) { LOG("Null reciever PRI"); return; }
		if (!victim) { LOG("Null victim PRI"); return; }

		// Compare the primary player to the victim
		if (playerPRI.memory_address == receiver.memory_address) {
			appendToPrompt("Your human opponent demolished your car! You will now respawn.");
		}
		else {
			appendToPrompt("You just demolished the human player's car! The human player will respawn.");
		}
	} else if (statEvent.GetEventName() == "OwnGoal") {
		if (!receiver) { LOG("Null reciever PRI"); return; }

		// Compare the primary player to the victim
		if (playerPRI.memory_address == receiver.memory_address) {
			appendToPrompt("Your human opponent scored on their own goal by accident.");
		}
		else {
			appendToPrompt("You scored a goal in your own goal.");
		}
	}
	else if (statEvent.GetEventName() == "Center") {

		if (playerPRI.memory_address == receiver.memory_address) {
			appendToPrompt("The human opponent just centered the ball on your goal.");
		}
		else {
			appendToPrompt("You just centered the ball near your human opponent's goal.");
		}
	}
	else if (statEvent.GetEventName() == "Save") {

		// determine which player made the save
		if (playerPRI.memory_address == receiver.memory_address) {
			appendToPrompt("The human opponent just saved the ball from going in their goal.");
		}
		else {
			appendToPrompt("You just saved the ball from going in your goal.");
		}
	}
	// make a request to the LLM server once the prompt has been updated with a new event
	makeRequest();
}

std::vector<std::string> RocketLeagueBotChat::splitIntoSmallStrings(const std::string& txt, int messageSize) {
	std::istringstream iss(txt);
	std::vector<std::string> substrings;
	std::string word, accumulated;

	while (iss >> word) {
		if ((accumulated.length() + word.length() + 1) <= messageSize) { // +1 accounts for space
			accumulated += ' ' + word;
		}
		else {
			substrings.push_back(accumulated);
			accumulated = word;
		}
	}
	substrings.push_back(accumulated); // Add the last accumulated string

	return substrings;
}

/**
 * Remove emoji characters from a string
 */
std::string RocketLeagueBotChat::removeEmojiCharacters(std::string input) {
	std::string result;
	size_t i = 0;
	while (i < input.length()) {
		// Single byte character
		if ((input[i] & 0x80) == 0) {
			result += input[i];
			++i;
		}
		else {
			// Multibyte character
			int n = 1;
			if ((input[i] & 0xF0) == 0xF0) n = 4; // 11110xxx 4 bytes
			else if ((input[i] & 0xE0) == 0xE0) n = 3; // 1110xxxx 3 bytes
			else if ((input[i] & 0xC0) == 0xC0) n = 2; // 110xxxxx 2 bytes

			// Skip this character by advancing the pointer by n bytes
			i += n;
		}
	}
	return result;
}

/**
 * Removes escaped double quotes from strings
 * 
 * The responses from the LLM generally look like this:
 * 
 *     " \"Sample response from LLM\"</sys>"
 * 
 * This function removes escaped double quotes from the a string (\")
 * 
 */
std::string RocketLeagueBotChat::removeDoubleQuotes(std::string str) {

	// Create a regular expression to match all occurrences of ".
	std::regex regex("\\\"");

	// Replace all occurrences of " with an empty string.
	str = std::regex_replace(str, regex, "");

	return str;
}

/**
 * Remove </sys> tag from string. This is the stop character included in responses from LLM
 */
std::string RocketLeagueBotChat::removeTag(std::string input) {
	std::string result = input;
	std::string toRemove = "</s>"; // The substring to find and remove
	size_t pos = 0;
	// Loop to find and erase all occurrences of the substring
	while ((pos = result.find(toRemove, pos)) != std::string::npos) {
		result.erase(pos, toRemove.length());
	}
	LOG("Result: {}", result);
	return result;
}

/**
 * sanitizeMessage
 *
 * This method removes characters that cannot be displayed in the in game chat, such as emoji characters
 *
 * Also removes the stop character </s> and fixes other issues with the format of the strings returned from the LLM
 */
std::string RocketLeagueBotChat::sanitizeMessage(std::string message) {

	// remove emoji characters
	std::string sanitized_message = message;
	sanitized_message = removeEmojiCharacters(sanitized_message);
	sanitized_message = removeDoubleQuotes(sanitized_message);
	sanitized_message = removeTag(sanitized_message);

	// trim whitespaces on end of string
	sanitized_message.erase(0, sanitized_message.find_first_not_of(" \n\r\t"));
	sanitized_message.erase(sanitized_message.find_last_not_of(" \n\r\t") + 1);
	return sanitized_message;
}

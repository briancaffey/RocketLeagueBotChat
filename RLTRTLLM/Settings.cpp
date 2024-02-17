#include "pch.h"
#include "RocketLeagueBotChat.h"

void RocketLeagueBotChat::RenderSettings() {
    ImGui::TextUnformatted("Control the prompt that will generate words that the bot sends to chat.");

    CVarWrapper systemPromptCvar = cvarManager->getCvar("system_prompt");
    if (!systemPromptCvar) { return; }
    std::string system_prompt = systemPromptCvar.getStringValue();
    if (ImGui::InputTextMultiline("System Prompt", &system_prompt)) {
        systemPromptCvar.setValue(system_prompt);
    }

    CVarWrapper temperatureCvar = cvarManager->getCvar("temperature");
    if (!temperatureCvar) { return; }
    int temperature = temperatureCvar.getIntValue();
    if (ImGui::SliderInt("Temperature", &temperature, 1, 100)) {
        temperatureCvar.setValue(temperature);
    }

    CVarWrapper seedCvar = cvarManager->getCvar("seed");
    if (!seedCvar) { return; }
    int seed = seedCvar.getIntValue();
    if (ImGui::SliderInt("Seed", &seed, 1, 100)) {
        seedCvar.setValue(seed);
    }

    CVarWrapper messageJsonStringCVar = cvarManager->getCvar("message_json_string");
    if (!messageJsonStringCVar) { return; }
    std::string message_json_string = messageJsonStringCVar.getStringValue();

    // set the dimensions of the 
    ImVec2 dimensions = ImVec2(704, 900);
    if (ImGui::InputTextMultiline("MessageJson", &message_json_string, dimensions)) {
        seedCvar.setValue(message_json_string);
    }
}

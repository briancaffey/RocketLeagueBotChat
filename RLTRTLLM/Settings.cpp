#include "pch.h"
#include "RLTRTLLM.h"

void RLTRTLLM::RenderSettings() {
    ImGui::TextUnformatted("Control the prompt that will generate words that the bot sends to chat.");


    CVarWrapper promptCvar = cvarManager->getCvar("prompt");
    if (!promptCvar) { return; }
    std::string prompt = promptCvar.getStringValue();
    if (ImGui::InputTextMultiline("Prompt", &prompt)) {
        promptCvar.setValue(prompt);
    }

    CVarWrapper systemPromptCvar = cvarManager->getCvar("system_prompt");
    if (!systemPromptCvar) { return; }
    std::string system_prompt = systemPromptCvar.getStringValue();
    if (ImGui::InputTextMultiline("System Prompt", &system_prompt)) {
        systemPromptCvar.setValue(system_prompt);
    }
    //if (ImGui::IsItemHovered()) {
    //    ImGui::SetTooltip("Toggle Cool Plugin");
    //}


    CVarWrapper temperatureCvar = cvarManager->getCvar("temperature");
    if (!temperatureCvar) { return; }
    float temperature = temperatureCvar.getIntValue();
    if (ImGui::SliderFloat("Temperature", &temperature, 1, 100)) {
        temperatureCvar.setValue(temperature);
    }

    CVarWrapper seedCvar = cvarManager->getCvar("seed");
    if (!seedCvar) { return; }
    float seed = seedCvar.getIntValue();
    if (ImGui::SliderFloat("Seed", &seed, 1, 100)) {
        seedCvar.setValue(seed);
    }
    //if (ImGui::IsItemHovered()) {
    //    std::string hoverText = "distance is " + std::to_string(distance);
    //    ImGui::SetTooltip(hoverText.c_str());
    //}
    // Examples
    //boolean example
    //if (ImGui::Button("Ball On Top")) {
    //    gameWrapper->Execute([this](GameWrapper* gw) {
    //        LOG("sending body request");
    //        DEBUGLOG("RLTRTLLM making request to local server");
    //        cvarManager->executeCommand("MakeRequest");
    //        });
    //}
    //if (ImGui::IsItemHovered()) {
    //    ImGui::SetTooltip("Activate Ball On Top");
    //}
}
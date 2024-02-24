# Rocket League BotChat

![Rocket League BotChat](/img/cover.png)

## Introduction

Rocket League BotChat gives Rocket League bots that ability to chat based on in-game events. This is a BakkesMod plugin that can be installed via the [BakkesPlugins website](https://bakkesplugins.com/).

It is designed to be used with a large language model running locally on your PC. You can set this up using NVIDIA's TensorRT-LLM library. The plugin requires an OpenAI-compatible API server running on `localhost:5001`.

## Setting up a local LLM powered by TensorRT-LLM

Before installing the plugin, you will need to set up a local inference server. Please see this repository for details on how to set up a TensorRT-LLM-powered large language model on Windows: [https://github.com/NVIDIA/trt-llm-as-openai-windows](https://github.com/NVIDIA/trt-llm-as-openai-windows).

Make sure that the server runs on `localhost:5001` (use `--port 5001` when starting the server).

## Installation

### Install via zip file

This repo contains a file called `RocketLeagueBotChat.zip` which can be used to load the plugin into BakkesMod. To install via zip file, do the following:

- Clone this repo
- Start Rocket League and BakkesMod
- Open the BakkesMod menu in Rocket League (press `F2`)
- Navigate to the `Plugins` tab in the BakkesMod menu
- Click on `Plugin Manager (beta)` at the top of the left-side menu
- Click on the blue `Open pluginmanager` button
- Click on the blue `Install from ZIP` button
- Navigate to the location where you have cloned this repo
- Select the file `RocketLeagueBotChat.zip` in the file browser and click on `ok`

![load plugin with zip file](/img/install.png)

### Install via BakkesPlugins

I have submitted my plugin to BakkesPlugins ([https://bakkesplugins.com](https://bakkesplugins.com)), but it is still pending approval from the BakkesMod maintainers and is not yet publicly available.

Once it is approved, you should be able to install Rocket League BotChat by clicking on `Install with BakkesMod` from this page: [https://bakkesplugins.com/plugins/view/422](https://bakkesplugins.com/plugins/view/422).

## Developer Contest

This project was developed for NVIDIA's [Gen AI on RTX PCs Developer Contest](https://www.nvidia.com/en-us/ai-data-science/generative-ai/rtx-developer-contest/).

Here is my submission post on 𝕏: [https://x.com/briancaffey/status/1760529251072118901](https://x.com/briancaffey/status/1760529251072118901).

For more information about this plugin, please see this article on my blog: [https://briancaffey.github.io/2024/02/17/rocket-league-botchat-nvidia-generative-ai-on-rtx-pcs-developer-contest](https://briancaffey.github.io/2024/02/17/rocket-league-botchat-nvidia-generative-ai-on-rtx-pcs-developer-contest).

## Contributing

Contributions are welcome!

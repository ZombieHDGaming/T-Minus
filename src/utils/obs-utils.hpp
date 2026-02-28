#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QString>

#include "../plugin-support.h"

struct RegisterHotkeyCallbackData {
	std::function<void()> function;
	std::string hotkeyLogMessage;
};

extern std::unordered_map<int, RegisterHotkeyCallbackData *> g_hotkeyCallbackDataMap;

void LoadHotkey(int &id, const char *name, const char *description, std::function<void()> function,
		std::string buttonLogMessage, obs_data_t *savedData = nullptr);
void SaveHotkey(obs_data_t *sv_data, obs_hotkey_id id, const char *name);
void HotkeyCallback(void *incoming_data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
void CleanupHotkey(int id);
QString GenerateUniqueID();

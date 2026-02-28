#include "obs-utils.hpp"

#include <QCryptographicHash>
#include <QUuid>

std::unordered_map<int, RegisterHotkeyCallbackData *> g_hotkeyCallbackDataMap;

void LoadHotkey(int &id, const char *name, const char *description, std::function<void()> function,
		std::string buttonLogMessage, obs_data_t *savedData)
{
	RegisterHotkeyCallbackData *callbackData = new RegisterHotkeyCallbackData{function, buttonLogMessage};

	id = (int)obs_hotkey_register_frontend(name, description, (obs_hotkey_func)HotkeyCallback, callbackData);

	if (id != -1) {
		g_hotkeyCallbackDataMap[id] = callbackData;

		if (savedData) {
			OBSDataArrayAutoRelease array = obs_data_get_array(savedData, name);
			obs_hotkey_load(id, array);
		}
	} else {
		delete callbackData;
	}
}

void CleanupHotkey(int id)
{
	if (id == -1)
		return;

	obs_hotkey_unregister(id);

	auto it = g_hotkeyCallbackDataMap.find(id);
	if (it != g_hotkeyCallbackDataMap.end()) {
		delete it->second;
		g_hotkeyCallbackDataMap.erase(it);
	}
}

void SaveHotkey(obs_data_t *sv_data, obs_hotkey_id id, const char *name)
{
	if ((int)id == -1)
		return;
	OBSDataArrayAutoRelease array = obs_hotkey_save(id);
	obs_data_set_array(sv_data, name, array);
}

void HotkeyCallback(void *incoming_data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed) {
		auto *data = static_cast<RegisterHotkeyCallbackData *>(incoming_data);
		obs_log(LOG_INFO, "%s (hotkey)", data->hotkeyLogMessage.c_str());
		data->function();
	}
}

QString GenerateUniqueID()
{
	QUuid uuid = QUuid::createUuid();
	QByteArray hash = QCryptographicHash::hash(uuid.toByteArray(), QCryptographicHash::Md5);
	return QString(hash.toHex().left(8));
}

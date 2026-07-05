#include "tminus-dock.hpp"
#include "../utils/obs-utils.hpp"
#include "../plugin-support.h"

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QHBoxLayout>
#include <QWidget>

TMinusDock::TMinusDock(QWidget *parent) : OBSDock(parent)
{
	qRegisterMetaType<obs_data_t *>("obs_data_t*");

	SetupUI();

	obs_frontend_add_event_callback(OBSFrontendEventHandler, this);
}

TMinusDock::~TMinusDock()
{
	obs_frontend_remove_event_callback(OBSFrontendEventHandler, this);
	if (!m_exitSaveDone)
		SaveSettings();
	UnregisterGlobalHotkeys();
}

void TMinusDock::SetupUI()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	// Scrollable timer list. No frame and no background fill so the dock
	// takes on the active OBS theme's window background, like native docks.
	m_scrollArea = new QScrollArea(this);
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setFrameShape(QFrame::NoFrame);
	m_scrollArea->viewport()->setAutoFillBackground(false);

	auto *scrollContent = new QWidget(m_scrollArea);
	scrollContent->setAutoFillBackground(false);
	m_timerListLayout = new QVBoxLayout(scrollContent);
	m_timerListLayout->setContentsMargins(4, 4, 4, 4);
	m_timerListLayout->setSpacing(4);
	m_timerListLayout->addStretch();

	m_scrollArea->setWidget(scrollContent);
	mainLayout->addWidget(m_scrollArea, 1);

	// Bottom toolbar, mirroring the native OBS dock toolbars (Sources,
	// Scenes, Audio Mixer). The theme's "icon-*" class rules render these
	// as flat themed icon buttons; themeID covers legacy themes.
	auto *toolbar = new QHBoxLayout();
	toolbar->setContentsMargins(4, 2, 4, 2);
	toolbar->setSpacing(2);

	m_addBtn = new QPushButton(this);
	m_addBtn->setFixedSize(24, 24);
	m_addBtn->setFlat(true);
	m_addBtn->setProperty("themeID", "addIconSmall");
	m_addBtn->setProperty("class", "icon-plus");
	m_addBtn->setToolTip("Add Timer");
	toolbar->addWidget(m_addBtn);

	toolbar->addStretch();

	m_startAllBtn = new QPushButton(this);
	m_startAllBtn->setFixedSize(24, 24);
	m_startAllBtn->setFlat(true);
	m_startAllBtn->setProperty("themeID", "playIcon");
	m_startAllBtn->setProperty("class", "icon-media-play");
	m_startAllBtn->setToolTip("Start All Timers");
	toolbar->addWidget(m_startAllBtn);

	m_stopAllBtn = new QPushButton(this);
	m_stopAllBtn->setFixedSize(24, 24);
	m_stopAllBtn->setFlat(true);
	m_stopAllBtn->setProperty("themeID", "stopIcon");
	m_stopAllBtn->setProperty("class", "icon-media-stop");
	m_stopAllBtn->setToolTip("Stop All Timers");
	toolbar->addWidget(m_stopAllBtn);

	mainLayout->addLayout(toolbar);

	// Connections
	connect(m_addBtn, &QPushButton::clicked, this, [this]() { AddTimer(); });
	connect(m_startAllBtn, &QPushButton::clicked, this, &TMinusDock::StartAllTimers);
	connect(m_stopAllBtn, &QPushButton::clicked, this, &TMinusDock::StopAllTimers);
}

void TMinusDock::AddTimer(obs_data_t *savedData)
{
	auto *timer = new TimerWidget(this, savedData);

	connect(timer, &TimerWidget::removeRequested, this, &TMinusDock::RemoveTimer);
	connect(timer, &TimerWidget::moveRequested, this, &TMinusDock::MoveTimer);
	connect(timer, &TimerWidget::settingsChanged, this, [this]() { SaveSettings(); });

	// Insert before the stretch
	int insertIdx = m_timerListLayout->count() - 1;
	if (insertIdx < 0)
		insertIdx = 0;
	m_timerListLayout->insertWidget(insertIdx, timer);
	m_timers.append(timer);

	obs_log(LOG_INFO, "Timer added: %s", timer->timerId().toUtf8().constData());

	SaveSettings();
}

void TMinusDock::RemoveTimer(const QString &id)
{
	for (int i = 0; i < m_timers.size(); i++) {
		if (m_timers[i]->timerId() == id) {
			TimerWidget *timer = m_timers.takeAt(i);
			m_timerListLayout->removeWidget(timer);
			timer->deleteLater();
			obs_log(LOG_INFO, "Timer removed: %s", id.toUtf8().constData());
			SaveSettings();
			return;
		}
	}
}

void TMinusDock::MoveTimer(Direction dir, const QString &id)
{
	for (int i = 0; i < m_timers.size(); i++) {
		if (m_timers[i]->timerId() != id)
			continue;

		int newIdx = (dir == Direction::Up) ? i - 1 : i + 1;
		if (newIdx < 0 || newIdx >= m_timers.size())
			return;

		m_timers.swapItemsAt(i, newIdx);

		// Rebuild layout order
		for (int j = 0; j < m_timers.size(); j++) {
			m_timerListLayout->removeWidget(m_timers[j]);
		}
		for (int j = 0; j < m_timers.size(); j++) {
			m_timerListLayout->insertWidget(j, m_timers[j]);
		}

		SaveSettings();
		return;
	}
}

void TMinusDock::StartAllTimers()
{
	for (auto *timer : m_timers) {
		timer->StartTimer();
	}
}

void TMinusDock::StopAllTimers()
{
	for (auto *timer : m_timers) {
		timer->PauseTimer();
	}
}

void TMinusDock::SaveSettings()
{
	if (m_loading)
		return;

	obs_data_t *settings = obs_data_create();
	obs_data_array_t *timerArray = obs_data_array_create();

	for (auto *timer : m_timers) {
		obs_data_t *timerObj = obs_data_create();
		timer->SaveData(timerObj);
		obs_data_array_push_back(timerArray, timerObj);
		obs_data_release(timerObj);
	}

	obs_data_set_array(settings, "timers", timerArray);
	obs_data_array_release(timerArray);

	// Save hotkeys
	SaveHotkey(settings, m_startAllHotkeyId, "TMinus_StartAll");
	SaveHotkey(settings, m_stopAllHotkeyId, "TMinus_StopAll");

	// The module config directory is not created by OBS automatically;
	// without it obs_data_save_json fails silently and nothing persists.
	char *configDir = obs_module_config_path("");
	if (configDir) {
		os_mkdirs(configDir);
		bfree(configDir);
	}

	char *configPath = obs_module_config_path(CONFIG_FILE);
	if (configPath) {
		if (!obs_data_save_json_safe(settings, configPath, "tmp", "bak"))
			obs_log(LOG_WARNING, "Failed to save timer settings to %s", configPath);
		bfree(configPath);
	}

	obs_data_release(settings);
}

void TMinusDock::LoadSavedSettings()
{
	m_loading = true;

	obs_data_t *settings = nullptr;

	char *configPath = obs_module_config_path(CONFIG_FILE);
	if (configPath) {
		settings = obs_data_create_from_json_file_safe(configPath, "bak");
		bfree(configPath);
	}

	if (settings) {
		obs_data_array_t *timerArray = obs_data_get_array(settings, "timers");
		if (timerArray) {
			size_t count = obs_data_array_count(timerArray);
			for (size_t i = 0; i < count; i++) {
				obs_data_t *timerObj = obs_data_array_item(timerArray, i);
				AddTimer(timerObj);
				obs_data_release(timerObj);
			}
			obs_data_array_release(timerArray);
		}
	}

	// Register global hotkeys even on a fresh install (no settings file
	// yet), otherwise they never show up in OBS Settings > Hotkeys.
	RegisterGlobalHotkeys(settings);

	obs_data_release(settings);

	m_loading = false;
}

void TMinusDock::RegisterGlobalHotkeys(obs_data_t *savedData)
{
	LoadHotkey(
		m_startAllHotkeyId, "TMinus_StartAll", "T-Minus: Start All Timers", [this]() { StartAllTimers(); },
		"Start All Timers", savedData);

	LoadHotkey(
		m_stopAllHotkeyId, "TMinus_StopAll", "T-Minus: Stop All Timers", [this]() { StopAllTimers(); },
		"Stop All Timers", savedData);
}

void TMinusDock::UnregisterGlobalHotkeys()
{
	CleanupHotkey(m_startAllHotkeyId);
	m_startAllHotkeyId = -1;
	CleanupHotkey(m_stopAllHotkeyId);
	m_stopAllHotkeyId = -1;
}

void TMinusDock::OBSFrontendEventHandler(enum obs_frontend_event event, void *data)
{
	auto *dock = static_cast<TMinusDock *>(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		dock->LoadSavedSettings();
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		for (auto *timer : dock->m_timers) {
			if (timer->data().resetOnStreamStart) {
				timer->RestartTimer();
			}
			if (timer->data().startOnStreamStart) {
				timer->StartTimer();
			}
		}
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
		// Persist timers and current hotkey bindings before OBS tears
		// down and reloads state for the new collection/profile.
		dock->SaveSettings();
		break;

	case OBS_FRONTEND_EVENT_EXIT:
		dock->SaveSettings();
		dock->m_exitSaveDone = true;
		break;

	default:
		break;
	}
}

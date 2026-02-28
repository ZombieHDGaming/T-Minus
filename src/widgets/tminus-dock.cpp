#include "tminus-dock.hpp"
#include "../utils/obs-utils.hpp"
#include "../plugin-support.h"

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>

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
	SaveSettings();
	UnregisterGlobalHotkeys();
}

void TMinusDock::SetupUI()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);

	// OBS QSS strips background/border from QPushButton[class^="icon-"] to make
	// transparent icon buttons. We override that here so toolbar buttons render
	// as normal themed buttons while still carrying the class for icon loading.
	const QString toolbarBtnStyle = "QPushButton {"
					"  background-color: palette(Button);"
					"  border: 1px solid palette(Dark);"
					"  border-radius: 3px;"
					"  padding: 2px;"
					"}"
					"QPushButton:hover { background-color: palette(Midlight); }"
					"QPushButton:pressed { background-color: palette(Mid); }";

	// Toolbar
	auto *toolbar = new QHBoxLayout();

	m_addBtn = new QPushButton(this);
	m_addBtn->setFixedSize(24, 24);
	m_addBtn->setStyleSheet(toolbarBtnStyle);
	m_addBtn->setProperty("themeID", "addIconSmall");
	m_addBtn->setProperty("class", "icon-plus");
	m_addBtn->setToolTip("Add Timer");
	toolbar->addWidget(m_addBtn);

	toolbar->addStretch();

	m_startAllBtn = new QPushButton(this);
	m_startAllBtn->setFixedSize(24, 24);
	m_startAllBtn->setStyleSheet(toolbarBtnStyle);
	m_startAllBtn->setProperty("themeID", "playIcon");
	m_startAllBtn->setProperty("class", "icon-media-play");
	m_startAllBtn->setToolTip("Start All Timers");
	toolbar->addWidget(m_startAllBtn);

	m_stopAllBtn = new QPushButton(this);
	m_stopAllBtn->setFixedSize(24, 24);
	m_stopAllBtn->setStyleSheet(toolbarBtnStyle);
	m_stopAllBtn->setProperty("themeID", "stopIcon");
	m_stopAllBtn->setProperty("class", "icon-media-stop");
	m_stopAllBtn->setToolTip("Stop All Timers");
	toolbar->addWidget(m_stopAllBtn);

	mainLayout->addLayout(toolbar);

	// Scrollable timer list
	m_scrollArea = new QScrollArea(this);
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto *scrollContent = new QWidget(m_scrollArea);
	m_timerListLayout = new QVBoxLayout(scrollContent);
	m_timerListLayout->setContentsMargins(0, 0, 0, 0);
	m_timerListLayout->addStretch();

	m_scrollArea->setWidget(scrollContent);
	mainLayout->addWidget(m_scrollArea);

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

	char *configPath = obs_module_config_path(CONFIG_FILE);
	if (configPath) {
		obs_data_save_json(settings, configPath);
		bfree(configPath);
	}

	obs_data_release(settings);
}

void TMinusDock::LoadSavedSettings()
{
	char *configPath = obs_module_config_path(CONFIG_FILE);
	if (!configPath)
		return;

	obs_data_t *settings = obs_data_create_from_json_file(configPath);
	bfree(configPath);

	if (!settings)
		return;

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

	RegisterGlobalHotkeys(settings);

	// Register per-timer hotkeys
	for (auto *timer : m_timers) {
		timer->RegisterHotkeys(settings);
	}

	obs_data_release(settings);
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

	case OBS_FRONTEND_EVENT_EXIT:
		dock->SaveSettings();
		break;

	default:
		break;
	}
}

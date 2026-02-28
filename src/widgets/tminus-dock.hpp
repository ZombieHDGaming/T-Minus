#pragma once

#include "obs-dock-wrapper.hpp"
#include "timer-widget.hpp"

#include <QMetaType>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QList>

Q_DECLARE_OPAQUE_POINTER(obs_data_t *)

class TMinusDock : public OBSDock {
	Q_OBJECT

public:
	explicit TMinusDock(QWidget *parent = nullptr);
	~TMinusDock();

public slots:
	void AddTimer(obs_data_t *savedData = nullptr);
	void RemoveTimer(const QString &id);
	void MoveTimer(Direction dir, const QString &id);
	void StartAllTimers();
	void StopAllTimers();

private:
	void SetupUI();
	void SaveSettings();
	void LoadSavedSettings();
	void RegisterGlobalHotkeys(obs_data_t *savedData);
	void UnregisterGlobalHotkeys();

	static void OBSFrontendEventHandler(enum obs_frontend_event event, void *data);

	QVBoxLayout *m_timerListLayout;
	QScrollArea *m_scrollArea;
	QPushButton *m_addBtn;
	QPushButton *m_startAllBtn;
	QPushButton *m_stopAllBtn;

	QList<TimerWidget *> m_timers;

	int m_startAllHotkeyId = -1;
	int m_stopAllHotkeyId = -1;

	static constexpr const char *CONFIG_FILE = "config.json";
};

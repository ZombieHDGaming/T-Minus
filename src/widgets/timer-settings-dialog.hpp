#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QGroupBox>

#include "../core/timer-data.hpp"
#include "action-list-widget.hpp"

class TimerSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit TimerSettingsDialog(const TimerData &data, QWidget *parent = nullptr);

	void ApplyToData(TimerData &data) const;

private:
	void SetupUI();
	QWidget *CreateGeneralTab();
	QWidget *CreateDisplayTab();
	QWidget *CreateActionsTab();
	QWidget *CreateHotkeysTab();

	void PopulateTextSourceDropdown();
	void UpdateTypeVisibility();

private slots:
	void OnTimerTypeChanged();
	void OnAddTimeHotkeyAdd();
	void OnAddTimeHotkeyRemove();

private:
	QTabWidget *m_tabs;

	// General tab
	QLineEdit *m_nameEdit;
	QRadioButton *m_countdownRadio;
	QRadioButton *m_stopwatchRadio;
	QRadioButton *m_timeUntilRadio;

	QGroupBox *m_countdownGroup;
	QSpinBox *m_daysSpin;
	QSpinBox *m_hoursSpin;
	QSpinBox *m_minutesSpin;
	QSpinBox *m_secondsSpin;

	QGroupBox *m_timeUntilGroup;
	QDateTimeEdit *m_targetDateTimeEdit;

	QComboBox *m_textSourceCombo;
	QCheckBox *m_startOnStreamCheck;
	QCheckBox *m_resetOnStreamCheck;

	// Display tab
	QCheckBox *m_showDaysCheck;
	QCheckBox *m_showHoursCheck;
	QCheckBox *m_showMinutesCheck;
	QCheckBox *m_showSecondsCheck;
	QCheckBox *m_leadingZeroCheck;
	QCheckBox *m_useFormatStringCheck;
	QLineEdit *m_formatStringEdit;

	// Actions tab
	ActionListWidget *m_actionList;

	// Hotkeys tab
	QListWidget *m_addTimeList;
	QLineEdit *m_addTimeLabelEdit;
	QSpinBox *m_addTimeDeltaSpin;
	QPushButton *m_addTimeAddBtn;
	QPushButton *m_addTimeRemoveBtn;

	// Stored add-time entries (not yet committed)
	QList<AddTimeHotkeyEntry> m_addTimeEntries;
};

#include "timer-settings-dialog.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>
#include <QButtonGroup>

TimerSettingsDialog::TimerSettingsDialog(const TimerData &data, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(QString("Timer Settings - %1").arg(data.timerId));
	setMinimumWidth(450);

	m_addTimeEntries = data.addTimeHotkeys;

	SetupUI();

	// Display name
	m_nameEdit->setText(data.displayName);

	// Load current data into UI
	switch (data.timerType) {
	case TimerType::Countdown:
		m_countdownRadio->setChecked(true);
		break;
	case TimerType::Stopwatch:
		m_stopwatchRadio->setChecked(true);
		break;
	case TimerType::TimeUntil:
		m_timeUntilRadio->setChecked(true);
		break;
	}

	m_daysSpin->setValue(data.countdownDuration.days);
	m_hoursSpin->setValue(data.countdownDuration.hours);
	m_minutesSpin->setValue(data.countdownDuration.minutes);
	m_secondsSpin->setValue(data.countdownDuration.seconds);

	if (data.targetDateTime.isValid())
		m_targetDateTimeEdit->setDateTime(data.targetDateTime);
	else
		m_targetDateTimeEdit->setDateTime(QDateTime::currentDateTime().addSecs(3600));

	// Text source
	PopulateTextSourceDropdown();
	if (!data.selectedTextSource.isEmpty()) {
		int idx = m_textSourceCombo->findText(data.selectedTextSource);
		if (idx >= 0)
			m_textSourceCombo->setCurrentIndex(idx);
	}

	m_startOnStreamCheck->setChecked(data.startOnStreamStart);
	m_resetOnStreamCheck->setChecked(data.resetOnStreamStart);

	// Display
	m_showDaysCheck->setChecked(data.display.showDays);
	m_showHoursCheck->setChecked(data.display.showHours);
	m_showMinutesCheck->setChecked(data.display.showMinutes);
	m_showSecondsCheck->setChecked(data.display.showSeconds);
	m_leadingZeroCheck->setChecked(data.display.showLeadingZero);
	m_useFormatStringCheck->setChecked(data.display.useFormatString);
	m_formatStringEdit->setText(data.display.formatString);

	// Actions
	m_actionList->setActions(data.endActions);

	// Add-time hotkeys
	for (const auto &entry : m_addTimeEntries) {
		m_addTimeList->addItem(QString("%1 (%2%3s)")
					       .arg(entry.label)
					       .arg(entry.deltaSeconds >= 0 ? "+" : "")
					       .arg(entry.deltaSeconds));
	}

	UpdateTypeVisibility();
}

void TimerSettingsDialog::SetupUI()
{
	auto *mainLayout = new QVBoxLayout(this);

	m_tabs = new QTabWidget(this);
	m_tabs->addTab(CreateGeneralTab(), "General");
	m_tabs->addTab(CreateDisplayTab(), "Display");
	m_tabs->addTab(CreateActionsTab(), "Actions");
	m_tabs->addTab(CreateHotkeysTab(), "Hotkeys");

	mainLayout->addWidget(m_tabs);

	auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	mainLayout->addWidget(buttonBox);
}

QWidget *TimerSettingsDialog::CreateGeneralTab()
{
	auto *page = new QWidget();
	auto *layout = new QVBoxLayout(page);

	// Display Name
	auto *nameGroup = new QGroupBox("Display Name", page);
	auto *nameLayout = new QVBoxLayout(nameGroup);
	m_nameEdit = new QLineEdit(nameGroup);
	m_nameEdit->setPlaceholderText("Leave blank to show timer type");
	m_nameEdit->setMaxLength(64);
	nameLayout->addWidget(m_nameEdit);
	layout->addWidget(nameGroup);

	// Timer Type
	auto *typeGroup = new QGroupBox("Timer Type", page);
	auto *typeLayout = new QVBoxLayout(typeGroup);

	m_countdownRadio = new QRadioButton("Countdown", typeGroup);
	m_stopwatchRadio = new QRadioButton("Stopwatch", typeGroup);
	m_timeUntilRadio = new QRadioButton("Time Until", typeGroup);
	m_countdownRadio->setChecked(true);

	auto *btnGroup = new QButtonGroup(this);
	btnGroup->addButton(m_countdownRadio);
	btnGroup->addButton(m_stopwatchRadio);
	btnGroup->addButton(m_timeUntilRadio);

	typeLayout->addWidget(m_countdownRadio);
	typeLayout->addWidget(m_stopwatchRadio);
	typeLayout->addWidget(m_timeUntilRadio);
	layout->addWidget(typeGroup);

	connect(m_countdownRadio, &QRadioButton::toggled, this, &TimerSettingsDialog::OnTimerTypeChanged);
	connect(m_stopwatchRadio, &QRadioButton::toggled, this, &TimerSettingsDialog::OnTimerTypeChanged);
	connect(m_timeUntilRadio, &QRadioButton::toggled, this, &TimerSettingsDialog::OnTimerTypeChanged);

	// Countdown settings
	m_countdownGroup = new QGroupBox("Countdown Duration", page);
	auto *durationLayout = new QHBoxLayout(m_countdownGroup);

	durationLayout->addWidget(new QLabel("Days:"));
	m_daysSpin = new QSpinBox();
	m_daysSpin->setRange(0, 999);
	durationLayout->addWidget(m_daysSpin);

	durationLayout->addWidget(new QLabel("Hours:"));
	m_hoursSpin = new QSpinBox();
	m_hoursSpin->setRange(0, 23);
	durationLayout->addWidget(m_hoursSpin);

	durationLayout->addWidget(new QLabel("Min:"));
	m_minutesSpin = new QSpinBox();
	m_minutesSpin->setRange(0, 59);
	durationLayout->addWidget(m_minutesSpin);

	durationLayout->addWidget(new QLabel("Sec:"));
	m_secondsSpin = new QSpinBox();
	m_secondsSpin->setRange(0, 59);
	durationLayout->addWidget(m_secondsSpin);

	layout->addWidget(m_countdownGroup);

	// Time Until settings
	m_timeUntilGroup = new QGroupBox("Target Date/Time", page);
	auto *timeUntilLayout = new QVBoxLayout(m_timeUntilGroup);

	m_targetDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600));
	m_targetDateTimeEdit->setCalendarPopup(true);
	m_targetDateTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
	timeUntilLayout->addWidget(m_targetDateTimeEdit);

	layout->addWidget(m_timeUntilGroup);

	// Text Source
	auto *sourceGroup = new QGroupBox("Text Source", page);
	auto *sourceLayout = new QVBoxLayout(sourceGroup);

	m_textSourceCombo = new QComboBox();
	m_textSourceCombo->addItem("(None)");
	sourceLayout->addWidget(m_textSourceCombo);

	layout->addWidget(sourceGroup);

	// Stream behavior
	auto *streamGroup = new QGroupBox("Stream Behavior", page);
	auto *streamLayout = new QVBoxLayout(streamGroup);

	m_startOnStreamCheck = new QCheckBox("Start timer on stream start");
	streamLayout->addWidget(m_startOnStreamCheck);

	m_resetOnStreamCheck = new QCheckBox("Reset timer on stream start");
	streamLayout->addWidget(m_resetOnStreamCheck);

	layout->addWidget(streamGroup);

	layout->addStretch();

	return page;
}

QWidget *TimerSettingsDialog::CreateDisplayTab()
{
	auto *page = new QWidget();
	auto *layout = new QVBoxLayout(page);

	auto *showGroup = new QGroupBox("Show Units", page);
	auto *showLayout = new QVBoxLayout(showGroup);

	m_showDaysCheck = new QCheckBox("Days");
	m_showHoursCheck = new QCheckBox("Hours");
	m_showMinutesCheck = new QCheckBox("Minutes");
	m_showSecondsCheck = new QCheckBox("Seconds");
	m_leadingZeroCheck = new QCheckBox("Leading zero");

	m_showHoursCheck->setChecked(true);
	m_showMinutesCheck->setChecked(true);
	m_showSecondsCheck->setChecked(true);
	m_leadingZeroCheck->setChecked(true);

	showLayout->addWidget(m_showDaysCheck);
	showLayout->addWidget(m_showHoursCheck);
	showLayout->addWidget(m_showMinutesCheck);
	showLayout->addWidget(m_showSecondsCheck);
	showLayout->addWidget(m_leadingZeroCheck);

	layout->addWidget(showGroup);

	// Format string
	auto *fmtGroup = new QGroupBox("Format String", page);
	auto *fmtLayout = new QVBoxLayout(fmtGroup);

	m_useFormatStringCheck = new QCheckBox("Use custom format string");
	fmtLayout->addWidget(m_useFormatStringCheck);

	m_formatStringEdit = new QLineEdit();
	m_formatStringEdit->setPlaceholderText("Use %time% for the timer value");
	m_formatStringEdit->setText("%time%");
	fmtLayout->addWidget(m_formatStringEdit);

	fmtLayout->addWidget(new QLabel("Use %time% as placeholder for the formatted timer value."));

	connect(m_useFormatStringCheck, &QCheckBox::toggled, m_formatStringEdit, &QLineEdit::setEnabled);

	layout->addWidget(fmtGroup);

	layout->addStretch();

	return page;
}

QWidget *TimerSettingsDialog::CreateActionsTab()
{
	auto *page = new QWidget();
	auto *layout = new QVBoxLayout(page);

	layout->addWidget(new QLabel("Actions executed when timer ends:"));

	m_actionList = new ActionListWidget(page);
	layout->addWidget(m_actionList);

	return page;
}

QWidget *TimerSettingsDialog::CreateHotkeysTab()
{
	auto *page = new QWidget();
	auto *layout = new QVBoxLayout(page);

	layout->addWidget(new QLabel("Add Time Hotkeys:"));
	layout->addWidget(new QLabel("Create hotkeys that add or subtract time from this timer.\n"
				     "Assign the actual key binding in OBS Settings > Hotkeys."));

	m_addTimeList = new QListWidget(page);
	m_addTimeList->setMaximumHeight(120);
	layout->addWidget(m_addTimeList);

	// Add/remove buttons
	auto *listBtnRow = new QHBoxLayout();
	m_addTimeRemoveBtn = new QPushButton("Remove", page);
	listBtnRow->addWidget(m_addTimeRemoveBtn);
	listBtnRow->addStretch();
	layout->addLayout(listBtnRow);

	// New hotkey form
	auto *newGroup = new QGroupBox("New Add-Time Hotkey", page);
	auto *newLayout = new QVBoxLayout(newGroup);

	auto *labelRow = new QHBoxLayout();
	labelRow->addWidget(new QLabel("Label:"));
	m_addTimeLabelEdit = new QLineEdit();
	m_addTimeLabelEdit->setPlaceholderText("e.g. +30 seconds");
	labelRow->addWidget(m_addTimeLabelEdit);
	newLayout->addLayout(labelRow);

	auto *deltaRow = new QHBoxLayout();
	deltaRow->addWidget(new QLabel("Time (seconds):"));
	m_addTimeDeltaSpin = new QSpinBox();
	m_addTimeDeltaSpin->setRange(-86400, 86400);
	m_addTimeDeltaSpin->setValue(30);
	m_addTimeDeltaSpin->setSuffix(" sec");
	deltaRow->addWidget(m_addTimeDeltaSpin);
	newLayout->addLayout(deltaRow);

	newLayout->addWidget(new QLabel("Positive values add time, negative values subtract."));

	m_addTimeAddBtn = new QPushButton("Add Hotkey", newGroup);
	newLayout->addWidget(m_addTimeAddBtn);

	layout->addWidget(newGroup);

	layout->addStretch();

	connect(m_addTimeAddBtn, &QPushButton::clicked, this, &TimerSettingsDialog::OnAddTimeHotkeyAdd);
	connect(m_addTimeRemoveBtn, &QPushButton::clicked, this, &TimerSettingsDialog::OnAddTimeHotkeyRemove);

	return page;
}

void TimerSettingsDialog::PopulateTextSourceDropdown()
{
	QString current = m_textSourceCombo->currentText();
	m_textSourceCombo->clear();
	m_textSourceCombo->addItem("(None)");

	obs_enum_sources(
		[](void *data, obs_source_t *source) -> bool {
			auto *combo = static_cast<QComboBox *>(data);
			const char *id = obs_source_get_unversioned_id(source);

			// Only add text sources
			if (id && (strcmp(id, "text_gdiplus") == 0 || strcmp(id, "text_gdiplus_v2") == 0 ||
				   strcmp(id, "text_gdiplus_v3") == 0 || strcmp(id, "text_ft2_source") == 0 ||
				   strcmp(id, "text_ft2_source_v2") == 0)) {
				const char *name = obs_source_get_name(source);
				if (name && name[0])
					combo->addItem(name);
			}
			return true;
		},
		m_textSourceCombo);

	if (!current.isEmpty() && current != "(None)") {
		int idx = m_textSourceCombo->findText(current);
		if (idx >= 0)
			m_textSourceCombo->setCurrentIndex(idx);
	}
}

void TimerSettingsDialog::UpdateTypeVisibility()
{
	m_countdownGroup->setVisible(m_countdownRadio->isChecked());
	m_timeUntilGroup->setVisible(m_timeUntilRadio->isChecked());
}

void TimerSettingsDialog::OnTimerTypeChanged()
{
	UpdateTypeVisibility();
}

void TimerSettingsDialog::OnAddTimeHotkeyAdd()
{
	QString label = m_addTimeLabelEdit->text().trimmed();
	int delta = m_addTimeDeltaSpin->value();

	if (label.isEmpty()) {
		label = QString("%1%2s").arg(delta >= 0 ? "+" : "").arg(delta);
	}

	AddTimeHotkeyEntry entry;
	entry.label = label;
	entry.deltaSeconds = delta;
	m_addTimeEntries.append(entry);

	m_addTimeList->addItem(QString("%1 (%2%3s)").arg(label).arg(delta >= 0 ? "+" : "").arg(delta));

	m_addTimeLabelEdit->clear();
	m_addTimeDeltaSpin->setValue(30);
}

void TimerSettingsDialog::OnAddTimeHotkeyRemove()
{
	int row = m_addTimeList->currentRow();
	if (row < 0 || row >= m_addTimeEntries.size())
		return;

	m_addTimeEntries.removeAt(row);
	delete m_addTimeList->takeItem(row);
}

void TimerSettingsDialog::ApplyToData(TimerData &data) const
{
	// Display name
	data.displayName = m_nameEdit->text().trimmed();

	// Timer type
	if (m_countdownRadio->isChecked())
		data.timerType = TimerType::Countdown;
	else if (m_stopwatchRadio->isChecked())
		data.timerType = TimerType::Stopwatch;
	else
		data.timerType = TimerType::TimeUntil;

	// Countdown duration
	data.countdownDuration.days = m_daysSpin->value();
	data.countdownDuration.hours = m_hoursSpin->value();
	data.countdownDuration.minutes = m_minutesSpin->value();
	data.countdownDuration.seconds = m_secondsSpin->value();

	// TimeUntil
	data.targetDateTime = m_targetDateTimeEdit->dateTime();

	// Text source
	QString sourceName = m_textSourceCombo->currentText();
	data.selectedTextSource = (sourceName == "(None)") ? "" : sourceName;

	// Stream behavior
	data.startOnStreamStart = m_startOnStreamCheck->isChecked();
	data.resetOnStreamStart = m_resetOnStreamCheck->isChecked();

	// Display
	data.display.showDays = m_showDaysCheck->isChecked();
	data.display.showHours = m_showHoursCheck->isChecked();
	data.display.showMinutes = m_showMinutesCheck->isChecked();
	data.display.showSeconds = m_showSecondsCheck->isChecked();
	data.display.showLeadingZero = m_leadingZeroCheck->isChecked();
	data.display.useFormatString = m_useFormatStringCheck->isChecked();
	data.display.formatString = m_formatStringEdit->text();

	// End actions
	data.endActions = m_actionList->actions();

	// Add-time hotkeys: compare old vs new
	// Remove hotkeys that were deleted
	QList<AddTimeHotkeyEntry> oldEntries = data.addTimeHotkeys;
	data.addTimeHotkeys = m_addTimeEntries;

	// Preserve hotkeyId and registrationName for entries that still exist
	for (int i = 0; i < data.addTimeHotkeys.size(); i++) {
		if (i < oldEntries.size() &&
		    data.addTimeHotkeys[i].hotkeyRegistrationName == oldEntries[i].hotkeyRegistrationName) {
			data.addTimeHotkeys[i].hotkeyId = oldEntries[i].hotkeyId;
		}
	}
}

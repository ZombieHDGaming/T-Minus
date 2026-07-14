#include "action-list-widget.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>

// The hotkey combos display the translated hotkey description
// (obs_hotkey_get_description) while carrying the registration name in the
// item data; the registration name is what gets saved and triggered.

static QString HotkeyComboValue(const QComboBox *combo)
{
	int idx = combo->currentIndex();
	if (idx >= 0 && combo->currentText() == combo->itemText(idx))
		return combo->itemData(idx).toString();
	// Manually typed registration name
	return combo->currentText();
}

static void SetHotkeyComboValue(QComboBox *combo, const QString &name)
{
	int idx = combo->findData(name);
	if (idx >= 0)
		combo->setCurrentIndex(idx);
	else
		combo->setEditText(name);
}

static void AddHotkeyComboItem(QComboBox *combo, obs_hotkey_t *hotkey)
{
	const char *name = obs_hotkey_get_name(hotkey);
	if (!name || !name[0])
		return;
	const char *desc = obs_hotkey_get_description(hotkey);
	combo->addItem((desc && desc[0]) ? QString::fromUtf8(desc) : QString::fromUtf8(name), QString::fromUtf8(name));
}

// Translated description for a saved registration name, for display in the
// action list; falls back to the name when the hotkey isn't registered right
// now (e.g. its timer or source lives in another scene collection).
static QString HotkeyDisplayName(const QString &name)
{
	if (name.isEmpty())
		return name;

	struct Search {
		QByteArray target;
		QString description;
	} search = {name.toUtf8(), QString()};

	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id, obs_hotkey_t *hotkey) -> bool {
			auto *search = static_cast<Search *>(data);
			const char *hkName = obs_hotkey_get_name(hotkey);
			if (!hkName || search->target != hkName)
				return true;
			const char *desc = obs_hotkey_get_description(hotkey);
			if (desc && desc[0])
				search->description = QString::fromUtf8(desc);
			return false;
		},
		&search);

	return search.description.isEmpty() ? name : search.description;
}

ActionListWidget::ActionListWidget(QWidget *parent) : QWidget(parent)
{
	SetupUI();
	SetupEditForms();
}

void ActionListWidget::SetupUI()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	m_listWidget = new QListWidget(this);
	m_listWidget->setMaximumHeight(150);
	mainLayout->addWidget(m_listWidget);

	// Buttons row
	auto *btnRow = new QHBoxLayout();

	m_addBtn = new QPushButton("Add", this);
	m_removeBtn = new QPushButton("Remove", this);
	m_moveUpBtn = new QPushButton("Up", this);
	m_moveDownBtn = new QPushButton("Down", this);

	btnRow->addWidget(m_addBtn);
	btnRow->addWidget(m_removeBtn);
	btnRow->addStretch();
	btnRow->addWidget(m_moveUpBtn);
	btnRow->addWidget(m_moveDownBtn);
	mainLayout->addLayout(btnRow);

	// Type selector
	auto *typeRow = new QHBoxLayout();
	typeRow->addWidget(new QLabel("Type:", this));
	m_typeCombo = new QComboBox(this);
	m_typeCombo->addItem("Set Text", static_cast<int>(ActionType::SetText));
	m_typeCombo->addItem("Enable/Disable Filter", static_cast<int>(ActionType::EnableFilter));
	m_typeCombo->addItem("Switch Scene", static_cast<int>(ActionType::SwitchScene));
	m_typeCombo->addItem("Delay", static_cast<int>(ActionType::Delay));
	m_typeCombo->addItem("Trigger Global Hotkey", static_cast<int>(ActionType::TriggerGlobalHotkey));
	m_typeCombo->addItem("Trigger Source Hotkey", static_cast<int>(ActionType::TriggerSourceHotkey));
	typeRow->addWidget(m_typeCombo);
	mainLayout->addLayout(typeRow);

	// Stacked edit forms
	m_editStack = new QStackedWidget(this);
	mainLayout->addWidget(m_editStack);

	// Connections
	connect(m_addBtn, &QPushButton::clicked, this, &ActionListWidget::OnAddAction);
	connect(m_removeBtn, &QPushButton::clicked, this, &ActionListWidget::OnRemoveAction);
	connect(m_moveUpBtn, &QPushButton::clicked, this, &ActionListWidget::OnMoveUp);
	connect(m_moveDownBtn, &QPushButton::clicked, this, &ActionListWidget::OnMoveDown);
	connect(m_listWidget, &QListWidget::currentRowChanged, this, &ActionListWidget::OnSelectionChanged);
	connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&ActionListWidget::OnTypeChanged);
}

void ActionListWidget::SetupEditForms()
{
	// Page 0: SetText
	auto *setTextPage = new QWidget();
	auto *setTextLayout = new QVBoxLayout(setTextPage);
	setTextLayout->setContentsMargins(0, 4, 0, 0);
	setTextLayout->addWidget(new QLabel("Text:"));
	m_setTextEdit = new QLineEdit();
	m_setTextEdit->setPlaceholderText("Text to display (supports %time%)");
	setTextLayout->addWidget(m_setTextEdit);
	m_editStack->addWidget(setTextPage);

	connect(m_setTextEdit, &QLineEdit::textChanged, this, &ActionListWidget::OnEditFieldChanged);

	// Page 1: EnableFilter
	auto *filterPage = new QWidget();
	auto *filterLayout = new QVBoxLayout(filterPage);
	filterLayout->setContentsMargins(0, 4, 0, 0);
	filterLayout->addWidget(new QLabel("Source:"));
	m_filterSourceCombo = new QComboBox();
	filterLayout->addWidget(m_filterSourceCombo);
	filterLayout->addWidget(new QLabel("Filter:"));
	m_filterNameCombo = new QComboBox();
	filterLayout->addWidget(m_filterNameCombo);
	m_filterEnabledCheck = new QCheckBox("Enable filter (uncheck to disable)");
	m_filterEnabledCheck->setChecked(true);
	filterLayout->addWidget(m_filterEnabledCheck);
	m_editStack->addWidget(filterPage);

	connect(m_filterSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		PopulateFilterDropdown(m_filterSourceCombo->currentText());
		OnEditFieldChanged();
	});
	connect(m_filterNameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&ActionListWidget::OnEditFieldChanged);
	connect(m_filterEnabledCheck, &QCheckBox::toggled, this, &ActionListWidget::OnEditFieldChanged);

	// Page 2: SwitchScene
	auto *scenePage = new QWidget();
	auto *sceneLayout = new QVBoxLayout(scenePage);
	sceneLayout->setContentsMargins(0, 4, 0, 0);
	sceneLayout->addWidget(new QLabel("Scene:"));
	m_sceneCombo = new QComboBox();
	sceneLayout->addWidget(m_sceneCombo);
	m_editStack->addWidget(scenePage);

	connect(m_sceneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&ActionListWidget::OnEditFieldChanged);

	// Page 3: Delay
	auto *delayPage = new QWidget();
	auto *delayLayout = new QVBoxLayout(delayPage);
	delayLayout->setContentsMargins(0, 4, 0, 0);
	delayLayout->addWidget(new QLabel("Delay (milliseconds):"));
	m_delaySpin = new QSpinBox();
	m_delaySpin->setRange(100, 300000);
	m_delaySpin->setSingleStep(500);
	m_delaySpin->setValue(1000);
	m_delaySpin->setSuffix(" ms");
	delayLayout->addWidget(m_delaySpin);
	m_editStack->addWidget(delayPage);

	connect(m_delaySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionListWidget::OnEditFieldChanged);

	// Page 4: TriggerGlobalHotkey
	auto *globalHkPage = new QWidget();
	auto *globalHkLayout = new QVBoxLayout(globalHkPage);
	globalHkLayout->setContentsMargins(0, 4, 0, 0);
	globalHkLayout->addWidget(new QLabel("Global Hotkey:"));
	m_globalHotkeyCombo = new QComboBox();
	m_globalHotkeyCombo->setEditable(true);
	globalHkLayout->addWidget(m_globalHotkeyCombo);
	m_editStack->addWidget(globalHkPage);

	connect(m_globalHotkeyCombo, &QComboBox::currentTextChanged, this, &ActionListWidget::OnEditFieldChanged);

	// Page 5: TriggerSourceHotkey
	auto *srcHkPage = new QWidget();
	auto *srcHkLayout = new QVBoxLayout(srcHkPage);
	srcHkLayout->setContentsMargins(0, 4, 0, 0);
	srcHkLayout->addWidget(new QLabel("Source:"));
	m_sourceHotkeySourceCombo = new QComboBox();
	srcHkLayout->addWidget(m_sourceHotkeySourceCombo);
	srcHkLayout->addWidget(new QLabel("Hotkey:"));
	m_sourceHotkeyNameCombo = new QComboBox();
	m_sourceHotkeyNameCombo->setEditable(true);
	srcHkLayout->addWidget(m_sourceHotkeyNameCombo);
	m_editStack->addWidget(srcHkPage);

	connect(m_sourceHotkeySourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		PopulateSourceHotkeyDropdown(m_sourceHotkeySourceCombo->currentText());
		OnEditFieldChanged();
	});
	connect(m_sourceHotkeyNameCombo, &QComboBox::currentTextChanged, this, &ActionListWidget::OnEditFieldChanged);
}

void ActionListWidget::setActions(const QList<ActionStep> &actions)
{
	m_actions = actions;
	UpdateListDisplay();
	if (!m_actions.isEmpty()) {
		m_listWidget->setCurrentRow(0);
	}
}

QList<ActionStep> ActionListWidget::actions() const
{
	return m_actions;
}

void ActionListWidget::OnAddAction()
{
	ActionStep step;
	step.type = static_cast<ActionType>(m_typeCombo->currentData().toInt());
	m_actions.append(step);
	UpdateListDisplay();
	m_listWidget->setCurrentRow(m_actions.size() - 1);
}

void ActionListWidget::OnRemoveAction()
{
	int row = m_listWidget->currentRow();
	if (row < 0 || row >= m_actions.size())
		return;

	m_actions.removeAt(row);
	UpdateListDisplay();

	if (!m_actions.isEmpty()) {
		m_listWidget->setCurrentRow(qMin(row, m_actions.size() - 1));
	}
}

void ActionListWidget::OnMoveUp()
{
	int row = m_listWidget->currentRow();
	if (row <= 0)
		return;

	m_actions.swapItemsAt(row, row - 1);
	UpdateListDisplay();
	m_listWidget->setCurrentRow(row - 1);
}

void ActionListWidget::OnMoveDown()
{
	int row = m_listWidget->currentRow();
	if (row < 0 || row >= m_actions.size() - 1)
		return;

	m_actions.swapItemsAt(row, row + 1);
	UpdateListDisplay();
	m_listWidget->setCurrentRow(row + 1);
}

void ActionListWidget::OnSelectionChanged()
{
	int row = m_listWidget->currentRow();
	if (row >= 0 && row < m_actions.size()) {
		LoadStepIntoEditor(row);
	}
}

void ActionListWidget::OnTypeChanged(int)
{
	int row = m_listWidget->currentRow();
	if (row < 0 || row >= m_actions.size() || m_updatingEditor)
		return;

	ActionType newType = static_cast<ActionType>(m_typeCombo->currentData().toInt());
	m_actions[row].type = newType;
	m_editStack->setCurrentIndex(static_cast<int>(newType));
	UpdateListDisplay();
}

void ActionListWidget::OnEditFieldChanged()
{
	int row = m_listWidget->currentRow();
	if (row < 0 || row >= m_actions.size() || m_updatingEditor)
		return;

	SaveEditorIntoStep(row);
	UpdateListDisplay();
}

void ActionListWidget::UpdateListDisplay()
{
	int currentRow = m_listWidget->currentRow();
	m_listWidget->clear();

	for (int i = 0; i < m_actions.size(); i++) {
		m_listWidget->addItem(QString("%1. %2").arg(i + 1).arg(ActionStepToString(m_actions[i])));
	}

	if (currentRow >= 0 && currentRow < m_actions.size()) {
		m_listWidget->setCurrentRow(currentRow);
	}
}

void ActionListWidget::LoadStepIntoEditor(int index)
{
	m_updatingEditor = true;

	const ActionStep &step = m_actions[index];

	// Set type combo
	int typeIdx = m_typeCombo->findData(static_cast<int>(step.type));
	if (typeIdx >= 0)
		m_typeCombo->setCurrentIndex(typeIdx);

	m_editStack->setCurrentIndex(static_cast<int>(step.type));

	switch (step.type) {
	case ActionType::SetText:
		m_setTextEdit->setText(step.text);
		break;

	case ActionType::EnableFilter:
		PopulateSourceDropdowns();
		if (!step.filterSourceName.isEmpty()) {
			int idx = m_filterSourceCombo->findText(step.filterSourceName);
			if (idx >= 0)
				m_filterSourceCombo->setCurrentIndex(idx);
			PopulateFilterDropdown(step.filterSourceName);
			idx = m_filterNameCombo->findText(step.filterName);
			if (idx >= 0)
				m_filterNameCombo->setCurrentIndex(idx);
		}
		m_filterEnabledCheck->setChecked(step.filterEnabled);
		break;

	case ActionType::SwitchScene:
		PopulateSceneDropdown();
		if (!step.sceneName.isEmpty()) {
			int idx = m_sceneCombo->findText(step.sceneName);
			if (idx >= 0)
				m_sceneCombo->setCurrentIndex(idx);
		}
		break;

	case ActionType::Delay:
		m_delaySpin->setValue(step.delayMs);
		break;

	case ActionType::TriggerGlobalHotkey:
		PopulateGlobalHotkeyDropdown();
		if (!step.globalHotkeyName.isEmpty())
			SetHotkeyComboValue(m_globalHotkeyCombo, step.globalHotkeyName);
		break;

	case ActionType::TriggerSourceHotkey:
		PopulateSourceDropdowns();
		if (!step.sourceHotkeySourceName.isEmpty()) {
			int idx = m_sourceHotkeySourceCombo->findText(step.sourceHotkeySourceName);
			if (idx >= 0)
				m_sourceHotkeySourceCombo->setCurrentIndex(idx);
			PopulateSourceHotkeyDropdown(step.sourceHotkeySourceName);
			if (!step.sourceHotkeyName.isEmpty())
				SetHotkeyComboValue(m_sourceHotkeyNameCombo, step.sourceHotkeyName);
		}
		break;
	}

	m_updatingEditor = false;
}

void ActionListWidget::SaveEditorIntoStep(int index)
{
	ActionStep &step = m_actions[index];

	switch (step.type) {
	case ActionType::SetText:
		step.text = m_setTextEdit->text();
		break;

	case ActionType::EnableFilter:
		step.filterSourceName = m_filterSourceCombo->currentText();
		step.filterName = m_filterNameCombo->currentText();
		step.filterEnabled = m_filterEnabledCheck->isChecked();
		break;

	case ActionType::SwitchScene:
		step.sceneName = m_sceneCombo->currentText();
		break;

	case ActionType::Delay:
		step.delayMs = m_delaySpin->value();
		break;

	case ActionType::TriggerGlobalHotkey:
		step.globalHotkeyName = HotkeyComboValue(m_globalHotkeyCombo);
		break;

	case ActionType::TriggerSourceHotkey:
		step.sourceHotkeySourceName = m_sourceHotkeySourceCombo->currentText();
		step.sourceHotkeyName = HotkeyComboValue(m_sourceHotkeyNameCombo);
		break;
	}
}

void ActionListWidget::PopulateSourceDropdowns()
{
	auto populateCombo = [](QComboBox *combo) {
		QString current = combo->currentText();
		combo->clear();

		obs_enum_sources(
			[](void *data, obs_source_t *source) -> bool {
				auto *combo = static_cast<QComboBox *>(data);
				const char *name = obs_source_get_name(source);
				if (name && name[0])
					combo->addItem(name);
				return true;
			},
			combo);

		// Also add scenes as sources
		struct obs_frontend_source_list sceneList = {};
		obs_frontend_get_scenes(&sceneList);
		for (size_t i = 0; i < sceneList.sources.num; i++) {
			const char *name = obs_source_get_name(sceneList.sources.array[i]);
			if (name && combo->findText(name) < 0)
				combo->addItem(name);
		}
		obs_frontend_source_list_free(&sceneList);

		if (!current.isEmpty()) {
			int idx = combo->findText(current);
			if (idx >= 0)
				combo->setCurrentIndex(idx);
		}
	};

	populateCombo(m_filterSourceCombo);
	populateCombo(m_sourceHotkeySourceCombo);
}

void ActionListWidget::PopulateFilterDropdown(const QString &sourceName)
{
	m_filterNameCombo->clear();
	if (sourceName.isEmpty())
		return;

	obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
	if (!source)
		return;

	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *data) {
			auto *combo = static_cast<QComboBox *>(data);
			combo->addItem(obs_source_get_name(filter));
		},
		m_filterNameCombo);

	obs_source_release(source);
}

void ActionListWidget::PopulateSceneDropdown()
{
	QString current = m_sceneCombo->currentText();
	m_sceneCombo->clear();

	struct obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		const char *name = obs_source_get_name(sceneList.sources.array[i]);
		if (name)
			m_sceneCombo->addItem(name);
	}
	obs_frontend_source_list_free(&sceneList);

	if (!current.isEmpty()) {
		int idx = m_sceneCombo->findText(current);
		if (idx >= 0)
			m_sceneCombo->setCurrentIndex(idx);
	}
}

void ActionListWidget::PopulateGlobalHotkeyDropdown()
{
	QString current = HotkeyComboValue(m_globalHotkeyCombo);
	m_globalHotkeyCombo->clear();

	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id, obs_hotkey_t *hotkey) -> bool {
			auto *combo = static_cast<QComboBox *>(data);
			// Only frontend-registered hotkeys are global; source
			// hotkeys are covered by Trigger Source Hotkey.
			if (obs_hotkey_get_registerer_type(hotkey) == OBS_HOTKEY_REGISTERER_FRONTEND)
				AddHotkeyComboItem(combo, hotkey);
			return true;
		},
		m_globalHotkeyCombo);

	if (!current.isEmpty())
		SetHotkeyComboValue(m_globalHotkeyCombo, current);
}

void ActionListWidget::PopulateSourceHotkeyDropdown(const QString &sourceName)
{
	m_sourceHotkeyNameCombo->clear();
	if (sourceName.isEmpty())
		return;

	obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
	if (!source)
		return;

	// Enumerate all hotkeys and find ones associated with this source.
	// Source hotkeys are registered with the source's weak reference as
	// the registerer, so that is what the comparison has to use.
	obs_weak_source_t *weak = obs_source_get_weak_source(source);

	struct SourceHotkeyEnumData {
		QComboBox *combo;
		obs_weak_source_t *weak;
	};

	SourceHotkeyEnumData enumData = {m_sourceHotkeyNameCombo, weak};

	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id, obs_hotkey_t *hotkey) -> bool {
			auto *enumData = static_cast<SourceHotkeyEnumData *>(data);
			if (obs_hotkey_get_registerer_type(hotkey) == OBS_HOTKEY_REGISTERER_SOURCE &&
			    obs_hotkey_get_registerer(hotkey) == enumData->weak)
				AddHotkeyComboItem(enumData->combo, hotkey);
			return true;
		},
		&enumData);

	obs_weak_source_release(weak);
	obs_source_release(source);
}

QString ActionListWidget::ActionStepToString(const ActionStep &step)
{
	switch (step.type) {
	case ActionType::SetText:
		return QString("Set Text: \"%1\"").arg(step.text.left(30));
	case ActionType::EnableFilter:
		return QString("%1 Filter: \"%2\" on \"%3\"")
			.arg(step.filterEnabled ? "Enable" : "Disable")
			.arg(step.filterName)
			.arg(step.filterSourceName);
	case ActionType::SwitchScene:
		return QString("Switch Scene: \"%1\"").arg(step.sceneName);
	case ActionType::Delay:
		return QString("Delay: %1 ms").arg(step.delayMs);
	case ActionType::TriggerGlobalHotkey:
		return QString("Global Hotkey: \"%1\"").arg(HotkeyDisplayName(step.globalHotkeyName));
	case ActionType::TriggerSourceHotkey:
		return QString("Source Hotkey: \"%1\" on \"%2\"")
			.arg(HotkeyDisplayName(step.sourceHotkeyName))
			.arg(step.sourceHotkeySourceName);
	}
	return "Unknown";
}

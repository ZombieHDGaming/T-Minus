#pragma once

#include <QObject>
#include <QList>
#include <QTimer>

#include "timer-data.hpp"

class ActionExecutor : public QObject {
	Q_OBJECT

public:
	explicit ActionExecutor(QObject *parent = nullptr);
	~ActionExecutor();

	void execute(const QList<ActionStep> &actions, const QString &textSourceName);
	void cancel();

signals:
	void sequenceComplete();

private:
	void executeNextStep();
	void executeSetText(const ActionStep &step);
	void executeEnableFilter(const ActionStep &step);
	void executeSwitchScene(const ActionStep &step);
	void executeTriggerGlobalHotkey(const ActionStep &step);
	void executeTriggerSourceHotkey(const ActionStep &step);

	QList<ActionStep> m_actions;
	QString m_textSourceName;
	int m_currentStep = 0;
	QTimer *m_delayTimer;
};

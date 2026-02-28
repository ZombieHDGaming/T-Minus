#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>

#include "timer-data.hpp"

class TimerEngine : public QObject {
	Q_OBJECT

public:
	explicit TimerEngine(QObject *parent = nullptr);
	~TimerEngine();

	void configure(TimerType type, const DurationValue &countdown, const QDateTime &target);
	void start();
	void pause();
	void restart();
	void stop();
	void addTime(int deltaMs);

	TimerState state() const { return m_state; }
	TimerType timerType() const { return m_type; }
	long long currentTimeMs() const { return m_timeMs; }

	void setTimeMs(long long ms);

signals:
	void tick(long long currentMs);
	void finished();
	void stateChanged(TimerState newState);

private slots:
	void onTick();

private:
	void setState(TimerState s);

	QTimer *m_tickTimer;
	TimerType m_type = TimerType::Countdown;
	TimerState m_state = TimerState::Stopped;

	// Configuration
	long long m_initialMs = 0;
	QDateTime m_targetDateTime;

	// Runtime
	long long m_timeMs = 0;
	QDateTime m_wallClockStart;
	long long m_msAtStart = 0;
	long long m_lastDisplayedSeconds = -1;
};

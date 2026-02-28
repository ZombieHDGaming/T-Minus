#include "timer-engine.hpp"
#include "../utils/format-utils.hpp"

TimerEngine::TimerEngine(QObject *parent) : QObject(parent)
{
	m_tickTimer = new QTimer(this);
	m_tickTimer->setInterval(TIMER_TICK_INTERVAL);
	connect(m_tickTimer, &QTimer::timeout, this, &TimerEngine::onTick);
}

TimerEngine::~TimerEngine()
{
	m_tickTimer->stop();
}

void TimerEngine::configure(TimerType type, const DurationValue &countdown, const QDateTime &target)
{
	m_type = type;
	m_targetDateTime = target;
	m_lastDisplayedSeconds = -1;

	switch (type) {
	case TimerType::Countdown:
		m_initialMs = DurationToMs(countdown);
		m_timeMs = m_initialMs;
		break;
	case TimerType::Stopwatch:
		m_initialMs = 0;
		m_timeMs = 0;
		break;
	case TimerType::TimeUntil:
		m_initialMs = 0;
		m_timeMs = QDateTime::currentDateTime().msecsTo(m_targetDateTime);
		if (m_timeMs < 0)
			m_timeMs = 0;
		break;
	}
}

void TimerEngine::start()
{
	if (m_state == TimerState::Running)
		return;

	m_lastDisplayedSeconds = -1;
	m_wallClockStart = QDateTime::currentDateTime();
	m_msAtStart = m_timeMs;
	m_tickTimer->start(TIMER_TICK_INTERVAL);
	setState(TimerState::Running);
}

void TimerEngine::pause()
{
	if (m_state != TimerState::Running)
		return;

	m_tickTimer->stop();
	setState(TimerState::Paused);
}

void TimerEngine::restart()
{
	m_tickTimer->stop();

	switch (m_type) {
	case TimerType::Countdown:
		m_timeMs = m_initialMs;
		break;
	case TimerType::Stopwatch:
		m_timeMs = 0;
		break;
	case TimerType::TimeUntil:
		m_timeMs = QDateTime::currentDateTime().msecsTo(m_targetDateTime);
		if (m_timeMs < 0)
			m_timeMs = 0;
		break;
	}

	setState(TimerState::Stopped);
	emit tick(m_timeMs);
}

void TimerEngine::stop()
{
	m_tickTimer->stop();
	setState(TimerState::Stopped);
}

void TimerEngine::addTime(int deltaMs)
{
	m_timeMs += deltaMs;
	if (m_timeMs < 0)
		m_timeMs = 0;

	if (m_state == TimerState::Running) {
		m_wallClockStart = QDateTime::currentDateTime();
		m_msAtStart = m_timeMs;
	}

	m_lastDisplayedSeconds = m_timeMs / 1000;
	emit tick(m_timeMs);
}

void TimerEngine::setTimeMs(long long ms)
{
	m_timeMs = ms;
	if (m_timeMs < 0)
		m_timeMs = 0;

	if (m_state == TimerState::Running) {
		m_wallClockStart = QDateTime::currentDateTime();
		m_msAtStart = m_timeMs;
	}

	m_lastDisplayedSeconds = m_timeMs / 1000;
	emit tick(m_timeMs);
}

void TimerEngine::setState(TimerState s)
{
	if (m_state == s)
		return;
	m_state = s;
	emit stateChanged(m_state);
}

void TimerEngine::onTick()
{
	long long elapsed = m_wallClockStart.msecsTo(QDateTime::currentDateTime());

	switch (m_type) {
	case TimerType::Countdown:
		m_timeMs = m_msAtStart - elapsed;
		if (m_timeMs <= 0) {
			m_timeMs = 0;
			m_tickTimer->stop();
			m_lastDisplayedSeconds = 0;
			emit tick(m_timeMs);
			setState(TimerState::Finished);
			emit finished();
			return;
		}
		break;

	case TimerType::Stopwatch:
		m_timeMs = m_msAtStart + elapsed;
		break;

	case TimerType::TimeUntil:
		m_timeMs = QDateTime::currentDateTime().msecsTo(m_targetDateTime);
		if (m_timeMs <= 0) {
			m_timeMs = 0;
			m_tickTimer->stop();
			m_lastDisplayedSeconds = 0;
			emit tick(m_timeMs);
			setState(TimerState::Finished);
			emit finished();
			return;
		}
		break;
	}

	long long currentSeconds = m_timeMs / 1000;
	if (currentSeconds != m_lastDisplayedSeconds) {
		m_lastDisplayedSeconds = currentSeconds;
		emit tick(m_timeMs);
	}
}

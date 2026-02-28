#pragma once

#include <QString>
#include <QTime>

#include "../core/timer-data.hpp"

long long DurationToMs(const DurationValue &d);
DurationValue MsToDuration(long long ms);
QString FormatTimerDisplay(const DisplayFormat &fmt, long long timeMs);

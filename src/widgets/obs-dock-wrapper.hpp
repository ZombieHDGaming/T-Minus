#pragma once

#include <QWidget>
#include <obs.h>

class OBSDock : public QWidget {
	Q_OBJECT

public:
	explicit inline OBSDock(QWidget *parent = nullptr) : QWidget(parent) {}
	virtual ~OBSDock() = default;
};

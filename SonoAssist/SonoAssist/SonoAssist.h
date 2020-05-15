#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SonoAssist.h"

class SonoAssist : public QMainWindow
{
	Q_OBJECT

public:
	SonoAssist(QWidget *parent = Q_NULLPTR);

private:
	Ui::SonoAssistClass ui;
};

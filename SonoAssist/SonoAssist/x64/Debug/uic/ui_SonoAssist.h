/********************************************************************************
** Form generated from reading UI file 'SonoAssist.ui'
**
** Created by: Qt User Interface Compiler version 5.14.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SONOASSIST_H
#define UI_SONOASSIST_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SonoAssistClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *SonoAssistClass)
    {
        if (SonoAssistClass->objectName().isEmpty())
            SonoAssistClass->setObjectName(QString::fromUtf8("SonoAssistClass"));
        SonoAssistClass->resize(600, 400);
        menuBar = new QMenuBar(SonoAssistClass);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        SonoAssistClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(SonoAssistClass);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        SonoAssistClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(SonoAssistClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        SonoAssistClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(SonoAssistClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        SonoAssistClass->setStatusBar(statusBar);

        retranslateUi(SonoAssistClass);

        QMetaObject::connectSlotsByName(SonoAssistClass);
    } // setupUi

    void retranslateUi(QMainWindow *SonoAssistClass)
    {
        SonoAssistClass->setWindowTitle(QCoreApplication::translate("SonoAssistClass", "SonoAssist", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SonoAssistClass: public Ui_SonoAssistClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SONOASSIST_H

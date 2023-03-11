#pragma once

#include "ui_ParamEditor.h"

#include <string>
#include <algorithm>

#include <QString>
#include <QWidget>
#include <QDialog>

#define PARAM_NAME_WIDTH 200
#define PARAM_VALUE_WIDTH 650

using config_map = std::map<std::string, std::string>;

/**
* Graphical window class for the editing of configuration parameters
*/
class ParamEditor : public QDialog {

    Q_OBJECT

    public:
        ParamEditor(config_map&, QWidget* parent=0);

    private:

        void build_param_table(void);

        Ui::ParamEditWindow ui;
        config_map& m_config_params;

    private slots:
        void update_config_param(int row, int column);

};
#include "ParamEditor.h"
#include "SonoAssist.h"

/*******************************************************************************
* CONSTRUCTOR & DESTRUCTOR
******************************************************************************/

ParamEditor::ParamEditor(config_map& config_params, QWidget* parent)
	:QDialog(parent), m_config_params(config_params) {

	ui.setupUi(this);
    build_param_table();

}


/*******************************************************************************
* GRAPHICAL FUNCTIONS
******************************************************************************/

void ParamEditor::build_param_table(void) {

    // setting up the columns
    ui.param_table->setColumnCount(2);
    ui.param_table->setColumnWidth(0, PARAM_NAME_WIDTH);
    ui.param_table->setColumnWidth(1, PARAM_VALUE_WIDTH);
    ui.param_table->setRowCount(m_config_params.size());
    ui.param_table->setHorizontalHeaderLabels(QStringList{"Parameter names", "Parameter values"});
    
    // filling the table with param names & values
    int count = 0;
    std::for_each(m_config_params.begin(), m_config_params.end(), [&](auto pair) {

        QTableWidgetItem* param_name_field = new QTableWidgetItem(QString(pair.first.c_str()));
        param_name_field->setFlags(param_name_field->flags() ^ Qt::ItemIsEditable ^ Qt::ItemIsSelectable ^ Qt::ItemIsEnabled);
        ui.param_table->setItem(count, 0, param_name_field);

        QTableWidgetItem* param_value_field = new QTableWidgetItem(pair.second.c_str());
        param_name_field->setFlags(param_value_field->flags() & Qt::ItemIsEditable & Qt::ItemIsSelectable & Qt::ItemIsEnabled);
        ui.param_table->setItem(count, 1, param_value_field);

        count ++;

    });

    connect(ui.param_table, &QTableWidget::cellChanged, this, &ParamEditor::update_config_param);
    
}

void ParamEditor::update_config_param(int row, int col) {

    std::string param_name = ui.param_table->item(row, 0)->text().toStdString();
    std::string param_value = ui.param_table->item(row, 1)->text().toStdString();

    m_config_params[param_name] = param_value;

}
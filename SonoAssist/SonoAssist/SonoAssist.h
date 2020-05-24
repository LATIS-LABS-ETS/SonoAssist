#pragma once
#include <memory>

#include <QFile>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <QtXML/QDomDocument>
#include <QtWidgets/QMainWindow>

#include "ui_SonoAssist.h"
#include "MetaWearBluetoothClient.h"

typedef std::map<std::string, std::string> config_map;

class SonoAssist : public QMainWindow{
	
	Q_OBJECT

	public:
		SonoAssist(QWidget *parent = Q_NULLPTR);

	public slots:
		
		// device connection
		void on_gyro_connect_button_clicked(void);
		void on_gyro_status_change(bool device_satus);

		// stream data (start/stop)
		void on_start_acquisition_button_clicked();
		void on_stop_acquisition_button_clicked();

		// config loading
		void on_param_file_browse_clicked();
		void on_param_file_input_textChanged(const QString& text);

	private:

		Ui::SonoAssist ui;
		bool m_config_is_loaded = false;
		std::shared_ptr<config_map> m_app_params;
		std::shared_ptr<MetaWearBluetoothClient> m_metawear_client_p;

		bool load_config_file(QString param_file_path);
		void display_warning_message(QString title, QString message);

};

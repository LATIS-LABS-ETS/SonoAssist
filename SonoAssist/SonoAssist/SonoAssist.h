#pragma once

#include <memory>

#include <QFile>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <QtXML/QDomDocument>
#include <QtWidgets/QMainWindow>

#include "GazeTracker.h"
#include "ui_SonoAssist.h"
#include "MetaWearBluetoothClient.h"

#define MAIN_OUTPUT_EXTENSION ".sono"

typedef std::map<std::string, std::string> config_map;

class SonoAssist : public QMainWindow {
	
	Q_OBJECT

	public:
		SonoAssist(QWidget *parent = Q_NULLPTR);
		~SonoAssist();

	public slots:
		
		// device connection
		void on_sensor_connect_button_clicked(void);
		void on_gyro_status_change(bool device_status);
		void on_eye_tracker_status_change(bool device_status);

		// stream data (start/stop)
		void on_start_acquisition_button_clicked(void);
		void on_stop_acquisition_button_clicked(void);

		// loading files
		void on_param_file_browse_clicked(void);
		void on_output_file_browse_clicked(void);
		void on_param_file_input_textChanged(const QString& text);
		void on_output_file_input_textChanged(const QString& text);

	private:

		Ui::MainWindow ui;

		// requirements check vars
		bool m_stream_is_active = false;
		bool m_config_is_loaded = false;
		bool m_output_is_loaded = false;

		// config vars
		std::shared_ptr<config_map> m_app_params;
		std::string m_output_file_path = "";
		
		// data streaming vars
		std::shared_ptr<GazeTracker> m_tracker_client_p;
		std::shared_ptr<MetaWearBluetoothClient> m_metawear_client_p;

		bool load_config_file(QString param_file_path);
		void set_acquisition_label(bool active);
		void display_warning_message(QString title, QString message);

};

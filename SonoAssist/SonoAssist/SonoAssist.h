#pragma once

#define _WINSOCKAPI_
#include <windows.h>

#include <memory>

#include <QFile>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <QtXML/QDomDocument>
#include <QGraphicsPixmapItem>
#include <QtWidgets/QMainWindow>

#include "GazeTracker.h"
#include "WindowPainter.h"
#include "ui_SonoAssist.h"
#include "RGBDCameraClient.h"
#include "ClariusProbeClient.h"
#include "MetaWearBluetoothClient.h"

#define RED_TEXT "#cc0000"
#define GREEN_TEXT "#71ff3d"

// US probe display
#define PROBE_DISPLAY_WIDTH 640
#define PROBE_DISPLAY_HEIGHT 360
#define PROBE_DISPLAY_X_OFFSET 650
#define PROBE_DISPLAY_Y_OFFSET 0

// RGB D camera display
#define CAMERA_DISPLAY_WIDTH 640
#define CAMERA_DISPLAY_HEIGHT 360
#define CAMERA_DISPLAY_X_OFFSET 0
#define CAMERA_DISPLAY_Y_OFFSET 0

// Eyetracker crosshairs dimensions
#define EYETRACKER_CROSSHAIRS_WIDTH 50
#define EYETRACKER_CROSSHAIRS_HEIGHT 50

enum sensor_device_t {GYROSCOPE=0, EYETRACKER=1, CAMERA=2, US_PROBE=3, US_WINDOW=4};
typedef std::map<std::string, std::string> config_map;

class SonoAssist : public QMainWindow {
	
	Q_OBJECT

	public:
		SonoAssist(QWidget *parent = Q_NULLPTR);
		~SonoAssist();

	public slots:
		
		// device connection
		void on_sensor_connect_button_clicked(void);
		void on_camera_status_change(bool status);
		void on_gyro_status_change(bool device_status);
		void on_us_probe_status_change(bool device_status);
		void on_us_window_status_change(bool device_status);
		void on_eye_tracker_status_change(bool device_status);

		// stream data (start/stop)
		void on_start_acquisition_button_clicked(void);
		void on_stop_acquisition_button_clicked(void);

		// ui update slots
		void on_new_camera_image(QImage);
		void on_new_gaze_point(float, float);
		void on_new_us_screen_capture(QImage);

		// loading file slots 
		void on_param_file_browse_clicked(void);
		void on_output_folder_browse_clicked(void);
		void on_param_file_input_textChanged(const QString& text);
		void on_output_folder_input_textChanged(const QString& text);

	private:

		Ui::MainWindow ui;

		// main display vars 
		std::unique_ptr<QGraphicsScene> m_main_scene_p;
		
		// RGB D camera display vars
		std::unique_ptr<QPixmap> m_camera_bg_i_p;
		std::unique_ptr<QGraphicsPixmapItem> m_camera_bg_p;
		std::unique_ptr<QGraphicsPixmapItem> m_camera_pixmap_p;

		// eye tracker display vars
		std::unique_ptr<QPixmap> m_eye_tracker_bg_i_p;
		std::unique_ptr<QGraphicsPixmapItem> m_eye_tracker_bg_p;
		std::unique_ptr<QGraphicsPixmapItem> m_eyetracker_pixmap_p;
		std::unique_ptr<QGraphicsPixmapItem> m_eyetracker_crosshair_p;

		// requirements check vars
		bool m_sync_is_active = false;
		bool m_stream_is_active = false;
		bool m_config_is_loaded = false;
		bool m_output_is_loaded = false;

		// config vars
		std::string m_output_file_path = "";
		std::shared_ptr<config_map> m_app_params;
		
		// sensor devices
		std::shared_ptr<GazeTracker> m_tracker_client_p;
		std::shared_ptr<WindowPainter> m_us_window_client_p;
		std::shared_ptr<RGBDCameraClient> m_camera_client_p;
		std::shared_ptr<ClariusProbeClient> m_us_probe_client_p;
		std::shared_ptr<MetaWearBluetoothClient> m_metawear_client_p;
		std::vector<std::shared_ptr<SensorDevice>> m_sensor_devices;

		// custom event handler (Clarius probe)
		bool event(QEvent* event);

		// graphic functions
		void build_sensor_panel(void);
		void set_acquisition_label(bool active);
		bool load_config_file(QString param_file_path);
		void set_device_status(bool device_status, sensor_device_t device);
		void display_warning_message(QString title, QString message);

};

#pragma once

#include "ui_SonoAssist.h"

#include <tuple>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
#include <Lmcons.h>

#include <QFile>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QtXML/QDomDocument>
#include <QGraphicsPixmapItem>
#include <QtWidgets/QMainWindow>

#include "GazeTracker.h"
#include "OSKeyDetector.h"
#include "ScreenRecorder.h"
#include "RGBDCameraClient.h"
#include "ClariusProbeClient.h"
#include "MetaWearBluetoothClient.h"

#include "CUGNModel.h"

#include "MlModel.h"
#include "SensorDevice.h"
#include "process_management.h"
#include "ParamEditor.h"

#define RED_TEXT "#cc0000"
#define GREEN_TEXT "#008000"
#define IMG_PLACE_HOLDER_COLOR "#000000"
#define ACTIVE_SENSOR_FIELD_COLOR "#D3D3D3"
#define INACTIVE_SENSOR_FIELD_COLOR "#ffffff"

// main display constants
#define MAIN_DISPLAY_DEFAULT_WIDTH 1260
#define MAIN_DISPLAY_DEFAULT_HEIGHT 720

// right preview display constants
#define PREVIEW_RIGHT_DISPLAY_WIDTH 640
#define PREVIEW_RIGHT_DISPLAY_HEIGHT 360
#define PREVIEW_RIGHT_DISPLAY_X_OFFSET 650
#define PREVIEW_RIGHT_DISPLAY_Y_OFFSET 0

// left preview display constants
#define PREVIEW_LEFT_DISPLAY_WIDTH 640
#define PREVIEW_LEFT_DISPLAY_HEIGHT 360
#define PREVIEW_LEFT_DISPLAY_X_OFFSET 0
#define PREVIEW_LEFT_DISPLAY_Y_OFFSET 0

// Eyetracker crosshairs dimensions constants
#define EYETRACKER_N_ACC_TARGETS 4
#define EYETRACKER_ACC_TARGET_WIDTH 20
#define EYETRACKER_ACC_TARGET_HEIGHT 20
#define EYETRACKER_CROSSHAIRS_WIDTH 50
#define EYETRACKER_CROSSHAIRS_HEIGHT 50

// defining default config file path
#define DEFAULT_LOG_PATH "C:/Users/<username>/AppData/SonoAssist/"
#define DEFAULT_CONFIG_PATH "C:/Program Files (x86)/SonoAssist/resources/params.xml"

using config_map = std::map<std::string, std::string>;


/**
* Main graphical window class handling user interaction
*/
class SonoAssist : public QMainWindow {
	
	Q_OBJECT

	public:

		SonoAssist(QWidget *parent = Q_NULLPTR);
		~SonoAssist();

	public slots:
		
		/*******************************************************************************
		* DISPLAY RELATED SLOTS
		******************************************************************************/

		void update_main_display(QImage);
		void on_new_clarius_image(QImage);
		void update_left_preview_display(QImage);
		void update_right_preview_display(QImage);

		void on_new_gaze_point(float, float);
		void on_device_warning_message(const QString& title, const QString& message);

		/*******************************************************************************
		* STATUS RELATED SLOTS
		******************************************************************************/

		void add_debug_text(const QString&);
		void set_device_status(int device_id, bool device_status);

		/*******************************************************************************
		* TIME MARKER HANDLING SLOTS
		******************************************************************************/

		/*
		* This method captures the key press codes emited by the (OSKeyDetector) SensorDevice instance
		* and creates (A) / deletes (D) time markers based on the presses.
		*
		* \param key The key code emitedby the (OSKeyDetector).
		*/
		void on_new_os_key_detected(int key);

	private slots:

		/*******************************************************************************
		* USER INPUT RELATED SLOTS
		******************************************************************************/
		 
		void on_pass_through_box_clicked(void);
		void on_eye_t_targets_box_clicked(void);
		void on_acquisition_preview_box_clicked(void);
		void on_start_acquisition_button_clicked(void);
		void on_stop_acquisition_button_clicked(void);
		void on_sensor_connect_button_clicked(void);
		 

		void on_param_edit_clicked(void);
		void on_param_file_reload_clicked(void);
		void on_param_file_browse_clicked(void);
		void on_output_folder_browse_clicked(void);

		void on_udp_port_input_editingFinished(void);
		void sensor_panel_selection_handler(int row, int column);
		
	private:

		/*******************************************************************************
		* GRAPHICAL FUNCTIONS
		******************************************************************************/
		
		void build_sensor_panel(void);
		void set_acquisition_label(bool active);
		void display_warning_message(const QString& title, const QString& message);

		/*******************************************************************************
		* GRAPHICS SCENCE FUNCTIONS
		******************************************************************************/

		void clean_main_display(void);
		void remove_main_display(void);
		void generate_main_display(void);
		void configure_main_display(void);

		void clean_preview_displays(void);
		void remove_preview_displays(void);
		void generate_preview_displays(void);
		
		void generate_eye_tracker_targets(void);

		/*******************************************************************************
		* PARAMETER LOADING & WRITING FUNCTIONS
		******************************************************************************/

		/**
		* Creates the logging directory according to (DEFAULT_LOG_PATH).
		* \return The path to the log file.
		*/
		std::string create_log_folder(void);

		void write_output_params(void);
		bool create_output_folder(void);
		void apply_config(void);
		bool load_config_file(const QString& param_file_path);

		/*******************************************************************************
		* UTILITY FUNCTIONS
		******************************************************************************/

		/**
		* Checks if all used devices are ready for acquisition.
		* \return (true) if all used devices are connected.
		*/
		bool check_devices_streaming(void);
		
		/**
		* Checks if all used devices are streaming.
		* \return (true) id all used devices are streaming.
		*/
		bool check_device_connections(void);

		/*******************************************************************************
		* TIME MARKER HANDLING SLOTS
		******************************************************************************/

		void clear_time_markers(void);

		// MAIN DISPLAY VARS

		Ui::MainWindow ui;

		std::unique_ptr<QGraphicsScene> m_main_scene_p;
		int m_main_us_img_width = MAIN_DISPLAY_DEFAULT_WIDTH;
		int m_main_us_img_height = MAIN_DISPLAY_DEFAULT_HEIGHT;
		
		std::unique_ptr<QGraphicsPixmapItem> m_main_pixmap_p;
		std::unique_ptr<QPixmap> m_main_bg_i_p;
		std::unique_ptr<QGraphicsPixmapItem> m_main_bg_p;

		std::unique_ptr<QPixmap> m_preview_right_bg_i_p;
		std::unique_ptr<QGraphicsPixmapItem> m_preview_right_bg_p;
		std::unique_ptr<QGraphicsPixmapItem> m_preview_right_pixmap_p;

		std::unique_ptr<QPixmap> m_preview_left_bg_i_p;
		std::unique_ptr<QGraphicsPixmapItem> m_preview_left_bg_p;
		std::unique_ptr<QGraphicsPixmapItem> m_preview_left_pixmap_p;

		std::unique_ptr<QGraphicsPixmapItem> m_eyetracker_crosshair_p;

		// state check vars
		bool m_stream_is_active = false;
		bool m_preview_is_active = false;
		bool m_config_is_loaded = false;
		bool m_eye_tracker_targets = false;

		// config and output vars
		QJsonObject m_output_params;
		std::string m_output_folder_path = "";
		std::shared_ptr<config_map> m_app_params;

		// time marker handling vars
		QJsonArray m_time_markers_json;

		// sensor devices

		std::shared_ptr<RGBDCameraClient> m_camera_client_p;
		std::shared_ptr<GazeTracker> m_gaze_tracker_client_p;
		std::shared_ptr<OSKeyDetector> m_key_detector_client_p;
		std::shared_ptr<ClariusProbeClient> m_us_probe_client_p;
		std::shared_ptr<ScreenRecorder> m_screen_recorder_client_p;
		std::shared_ptr<MetaWearBluetoothClient> m_metawear_client_p;
		
		std::vector<int> m_sensor_conn_updates;
		std::vector<std::shared_ptr<SensorDevice>> m_sensor_devices;

		// ML models
		std::vector<std::shared_ptr<MLModel>> m_ml_models;

		// redis process info
		PROCESS_INFORMATION m_redis_process;

};
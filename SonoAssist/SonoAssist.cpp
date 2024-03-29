#include "SonoAssist.h"

/*******************************************************************************
* CONSTRUCTOR & DESTRUCTOR
******************************************************************************/

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

    // predefining the parameters in the config file
    m_app_params = std::make_shared<config_map>();
    *m_app_params = {
        {"test_list", ""},
        {"ext_imu_ble_address", ""}, {"ext_imu_to_redis", ""}, {"ext_imu_redis_entry", ""}, {"ext_imu_redis_rate_div", ""},
        {"eye_tracker_to_redis", ""}, {"eye_tracker_redis_entry", ""}, {"eye_tracker_redis_rate_div", ""},
        {"sc_to_redis", ""}, {"sc_img_redis_entry", ""}, {"sc_redis_rate_div", ""},
        {"us_probe_ip_address", ""}, {"us_probe_to_redis", ""}, {"us_probe_imu_redis_entry", ""}, {"us_probe_img_redis_entry", ""} , {"us_probe_redis_rate_div", ""},
        {"redis_server_path", ""},
        {"eye_tracker_crosshairs_path", ""}, {"eye_tracker_target_path", ""},
        {"us_image_main_display_height", ""}, {"us_image_main_display_width", ""},
        {"cugn_active", ""}, {"cugn_model_path", ""}, {"cugn_to_redis", ""}, {"cugn_redis_entry", ""},
        {"cugn_sample_frequency", ""}, {"cugn_sequence_lenght", ""},  {"cugn_n_gru_cells", ""}, {"cugn_n_gru_neurons", ""}, {"cugn_pixel_mean", ""}, {"cugn_pixel_std_div", ""},
        {"cugn_us_template", ""}, {"cugn_input_h", "" }, {"cugn_input_w", ""}
    };

    std::string log_file_path = create_log_folder();

    /*******************************************************************************
    * CREATING THE SENSOR DEVICES (BEGIN)
    ******************************************************************************/

    m_sensor_devices = std::vector<std::shared_ptr<SensorDevice>>();
    
    m_gaze_tracker_client_p = std::make_shared<GazeTracker>(m_sensor_devices.size(), "Eye Tracker", "eye_tracker_to_redis", log_file_path);
    connect(m_gaze_tracker_client_p.get(), &GazeTracker::new_gaze_point, this, &SonoAssist::on_new_gaze_point);
    m_sensor_devices.push_back(m_gaze_tracker_client_p);
    
    m_camera_client_p = std::make_shared<RGBDCameraClient>(m_sensor_devices.size(), "RGBD Camera", "", log_file_path);
    connect(m_camera_client_p.get(), &RGBDCameraClient::new_video_frame, this, &SonoAssist::update_left_preview_display);
    m_sensor_devices.push_back(m_camera_client_p);
    
    m_metawear_client_p = std::make_shared<MetaWearBluetoothClient>(m_sensor_devices.size(), "External IMU", "ext_imu_to_redis", log_file_path);
    m_sensor_devices.push_back(m_metawear_client_p);
    
    m_us_probe_client_p = std::make_shared<ClariusProbeClient>(m_sensor_devices.size(), "Clarius Probe", "us_probe_to_redis", log_file_path);
    connect(m_us_probe_client_p.get(), &ClariusProbeClient::new_us_image, this, &SonoAssist::on_new_clarius_image);
    connect(m_us_probe_client_p.get(), &ClariusProbeClient::new_us_preview_image, this, &SonoAssist::update_right_preview_display);
    connect(m_us_probe_client_p.get(), &ClariusProbeClient::no_imu_data, this, &SonoAssist::on_device_warning_message);
    m_sensor_devices.push_back(m_us_probe_client_p);
    
    m_screen_recorder_client_p = std::make_shared<ScreenRecorder>(m_sensor_devices.size(), "Screen Recorder", "sc_to_redis", log_file_path);
    connect(m_screen_recorder_client_p.get(), &ScreenRecorder::new_window_capture, this, &SonoAssist::update_right_preview_display);
    m_sensor_devices.push_back(m_screen_recorder_client_p);

    m_key_detector_client_p = std::make_shared<OSKeyDetector>(m_sensor_devices.size(), "OS key detector", "", log_file_path);
    connect(m_key_detector_client_p.get(), &OSKeyDetector::key_detected, this, &SonoAssist::on_new_os_key_detected);
    m_sensor_devices.push_back(m_key_detector_client_p);
    
    // filling the connection state update counter list
    // connecting to the sensors (debug output) signals and sensors status change signals
    for (auto i = 0; i < m_sensor_devices.size(); i++) {
        m_sensor_conn_updates.push_back(0);
        connect(m_sensor_devices[i].get(), &SensorDevice::debug_output, this, &SonoAssist::add_debug_text, Qt::QueuedConnection);
        connect(m_sensor_devices[i].get(), &SensorDevice::device_status_change, this, &SonoAssist::set_device_status);
    }

    /*******************************************************************************
    * CREATING THE SENSOR DEVICES (END)
    ******************************************************************************/

    /*******************************************************************************
    * CREATING THE ML MODELS (BEGIN)
    ******************************************************************************/

    m_ml_models = std::vector<std::shared_ptr<MLModel>>();

    m_ml_models.emplace_back(std::make_shared<CUGNModel>(m_ml_models.size(),
        "CUGN Model", "cugn_active", "cugn_to_redis", "cugn_model_path", log_file_path, m_screen_recorder_client_p));
    connect(m_ml_models[m_ml_models.size() - 1].get(), &CUGNModel::new_us_img_detection, this, &SonoAssist::update_main_display);
    
    // connecting to the models (debug output) signal
    for (auto i = 0; i < m_ml_models.size(); i++) {
        connect(m_ml_models[i].get(), &MLModel::debug_output, this, &SonoAssist::add_debug_text, Qt::QueuedConnection);
    }

    /*******************************************************************************
    * CREATING THE ML MODELS (END)
    ******************************************************************************/
	
    // setting up the graphical interface
    ui.setupUi(this);
    build_sensor_panel();
    set_acquisition_label(false);

    // setting up the image display
    m_main_scene_p = std::make_unique<QGraphicsScene>(ui.graphicsView);
    ui.graphicsView->setScene(m_main_scene_p.get());
    generate_main_display();

    // writting screen dimensions to the output params
    int screen_width = 0, screen_height = 0;
    m_screen_recorder_client_p->get_screen_dimensions(screen_width, screen_height);
    m_output_params["screen_width"] = screen_width;
    m_output_params["screen_height"] = screen_height;

    // filling in the default config path
    ui.param_file_input->setText(QString(DEFAULT_CONFIG_PATH));
    on_param_file_reload_clicked();

}

SonoAssist::~SonoAssist(){

    try {

        // making sure all streams are shut off
        
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                m_sensor_devices[i]->stop_stream();
                m_sensor_devices[i]->disconnect_device();
            }
        }

        for (auto i = 0; i < m_ml_models.size(); i++) {
            if (m_ml_models[i]->get_model_status()) {
                m_ml_models[i]->stop_stream();
            }
        }

        remove_main_display();
        remove_preview_displays();

    } catch (...) {
        qDebug() << "\nSonoAssist - failed to stop the devices from treaming (in destructor)";
    }

}

/*******************************************************************************
* DISPLAY RELATED SLOTS
******************************************************************************/

void SonoAssist::update_main_display(QImage new_image) {

    if (!m_preview_is_active) {
    
        // creating a pix map from the incoming image
        QPixmap new_pixmap = QPixmap::fromImage(new_image);

        // creating / updating the QGraphicsPixmapItem item
        if (m_main_pixmap_p.get() != nullptr) {
            m_main_pixmap_p->setPixmap(new_pixmap);
        } else {
            m_main_pixmap_p = std::make_unique<QGraphicsPixmapItem>(new_pixmap);
            QPointF bg_img_pos = m_main_bg_p->pos();
            m_main_pixmap_p->setPos(bg_img_pos.x(), bg_img_pos.y());
            m_main_scene_p->addItem(m_main_pixmap_p.get());
        }

        // when acquisition is stoped during display
        if (!m_stream_is_active) {
            m_main_scene_p->removeItem(m_main_pixmap_p.get());
            m_main_pixmap_p.reset();
        }
    
    }

}

void SonoAssist::update_right_preview_display(QImage new_image) {

    if (m_preview_is_active) {

        // creating a pix map from the incoming image
        QPixmap new_pixmap = QPixmap::fromImage(new_image);

        // creating / updating the QGraphicsPixmapItem item
        if (m_preview_right_pixmap_p.get() != nullptr) {
            m_preview_right_pixmap_p->setPixmap(new_pixmap);
        } else {
            m_preview_right_pixmap_p = std::make_unique<QGraphicsPixmapItem>(new_pixmap);
            m_preview_right_pixmap_p->setPos(PREVIEW_RIGHT_DISPLAY_X_OFFSET, PREVIEW_RIGHT_DISPLAY_Y_OFFSET);
            m_preview_right_pixmap_p->setZValue(2);
            m_main_scene_p->addItem(m_preview_right_pixmap_p.get());
        }

        // when acquisition is stoped during display
        if (!m_stream_is_active) {
            m_main_scene_p->removeItem(m_preview_right_pixmap_p.get());
            m_preview_right_pixmap_p.reset();
        }

    }

}

void SonoAssist::update_left_preview_display(QImage new_image) {

    if (m_preview_is_active) {
    
        // creating a pix map from the incoming image
        QPixmap new_pixmap = QPixmap::fromImage(new_image);

        // creating / updating the QGraphicsPixmapItem item
        if (m_preview_left_pixmap_p.get() != nullptr) {
            m_preview_left_pixmap_p->setPixmap(new_pixmap);
        } else {
            m_preview_left_pixmap_p = std::make_unique<QGraphicsPixmapItem>(new_pixmap);
            m_preview_left_pixmap_p->setPos(PREVIEW_LEFT_DISPLAY_X_OFFSET, PREVIEW_LEFT_DISPLAY_Y_OFFSET);
            m_preview_left_pixmap_p->setZValue(2);
            m_main_scene_p->addItem(m_preview_left_pixmap_p.get());
        }

        // when acquisition is stoped during display
        if (!m_stream_is_active) {
            m_main_scene_p->removeItem(m_preview_left_pixmap_p.get());
            m_preview_left_pixmap_p.reset();
        }
    
    }

}

void SonoAssist::on_new_clarius_image(QImage new_image){

    if (!m_preview_is_active) {
    
        // dropping incoming image if display is locked
        if (!m_us_probe_client_p->m_display_locked) {

            m_us_probe_client_p->m_display_locked = true;

            update_main_display(new_image);

            // writing the output data
            m_us_probe_client_p->m_display_time = m_us_probe_client_p->get_micro_timestamp();
            m_us_probe_client_p->write_output_data();

            m_us_probe_client_p->m_handler_locked = false;

        }

    }

}

void SonoAssist::on_new_gaze_point(float x, float y) {

    if (x > 1) x = 1;
    if (x < 0) x = 0;
    if (y > 1) y = 1;
    if (y < 0) y = 0;

    // updating the eyetracker crosshair position
    if (m_eyetracker_crosshair_p != nullptr) {
        int x_pos = PREVIEW_RIGHT_DISPLAY_X_OFFSET + (PREVIEW_RIGHT_DISPLAY_WIDTH * x) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
        int y_pos = PREVIEW_RIGHT_DISPLAY_Y_OFFSET + (PREVIEW_RIGHT_DISPLAY_HEIGHT * y) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
        m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
    }
    
}

void SonoAssist::on_device_warning_message(const QString& title, const QString& message) {
    display_warning_message(title, message);
}

/*******************************************************************************
* USER INPUT RELATED SLOTS
******************************************************************************/

void SonoAssist::on_sensor_connect_button_clicked(){
    
    if (!m_stream_is_active && m_config_is_loaded) {

        // making sure all devices are disconnected (for proper state update check)
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_connection_status()) {
                m_sensor_devices[i]->disconnect_device();
            }
        }

        // preventing multiple connection button clicks

        int n_sensors_used = 0;
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            m_sensor_conn_updates[i] = 0;
            if (m_sensor_devices[i]->get_sensor_used()) {
                n_sensors_used++;
            }
        }
        
        if (n_sensors_used > 0) {
            ui.sensor_connect_button->setEnabled(false);
        }
        
        // connecting the devices
        ui.debug_text_edit->clear();
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                m_sensor_devices[i]->connect_device();
            }
        }

    } else {
        QString title = "Devices can not be connected";
        QString message = "Make sure that the acquisition is off and the paths to the config and output files are defined";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_acquisition_preview_box_clicked() {
   
    // devices should be connected
    if (!m_stream_is_active && check_device_connections()) {
        
        m_preview_is_active = ui.acquisition_preview_box->isChecked();

        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                m_sensor_devices[i]->set_stream_preview_status(m_preview_is_active);
            }
        }

        // toggling between the 2 display types
        if (m_preview_is_active) {
            remove_main_display();
            generate_preview_displays();
        } else {
            remove_preview_displays();
            generate_main_display();
        }
  
    } else {

        // reverting the check box selection
        ui.acquisition_preview_box->setChecked(!ui.acquisition_preview_box->isChecked());
        
        QString title = "Stream preview mode can not be toggled";
        QString message = "Make sure devices are connected and are not currently streaming.";
        display_warning_message(title, message);
    
    }

}

void SonoAssist::on_eye_t_targets_box_clicked() {

    if (!m_stream_is_active && m_config_is_loaded) {

        // updating the UI
        m_eye_tracker_targets = ui.eye_t_targets_box->isChecked();
        if (!m_preview_is_active) {
            remove_main_display();
            generate_main_display();
        }

    } else {

        // reverting the check box selection
        ui.eye_t_targets_box->setChecked(!ui.eye_t_targets_box->isChecked());

        QString title = "Eye tracker targets can not be displayed";
        QString message = "Make sure the configurations are loaded and that the tool is not streaming.";
        display_warning_message(title, message);

    }

}

void SonoAssist::on_pass_through_box_clicked() {

    if (!m_stream_is_active && m_config_is_loaded) {

        // updating the state for all devices
        bool pass_through_active = ui.pass_through_box->isChecked();
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            m_sensor_devices[i]->set_pass_through(pass_through_active);
        }
    
    } else {
    
        // reverting the check box selection
        ui.pass_through_box->setChecked(!ui.pass_through_box->isChecked());

        QString title = "Stream pass through mode can not be toggled";
        QString message = "Make sure the configurations are loaded and that the tool is not streaming.";
        display_warning_message(title, message);

    }

}

void SonoAssist::on_start_acquisition_button_clicked() {
    
    int n_used_sensors = 0;

    // making sure the device isnt already streaming
    if (!m_stream_is_active && create_output_folder()) {

        // if all used devices are ready, start the acquisition streams
        if (check_device_connections()) {

            m_stream_is_active = true;
            set_acquisition_label(true);

            clear_time_markers();

            // graying out the (start acquisition button while streaming)
            ui.start_acquisition_button->setEnabled(false);

            // launching the sensors and models

            for (auto i = 0; i < m_sensor_devices.size(); i++) {
                if (m_sensor_devices[i]->get_sensor_used()) {
                    n_used_sensors++;
                    m_sensor_devices[i]->start_stream();
                }
            }

            for (auto i = 0; i < m_ml_models.size(); i++) {
                if (m_ml_models[i]->get_model_status()) {
                    m_ml_models[i]->start_stream();
                }
            }
           
            // making sure devices are acquiring
            if (!check_devices_streaming()) {

                // stopping the acquisition
                on_stop_acquisition_button_clicked();
                ui.start_acquisition_button->setEnabled(true);
                
                QString title = "Streaming failed";
                QString message = "Ensure that all sensors are correclty configured.";
                display_warning_message(title, message);

            }

        } else {

            QString message;
            QString title = "Stream can not be started";

            // indicating the problematic sensors
            message = "The following devices are not ready for acquisition : [ ";
            for (auto i = 0; i < m_sensor_devices.size(); i++) {
                if (!m_sensor_devices[i]->get_connection_status() && m_sensor_devices[i]->get_sensor_used()) {
                    message += QString::fromStdString(m_sensor_devices[i]->get_device_description()) + ", ";
                }
            }
            message += "]"; 
            display_warning_message(title, message);

        }

    } else {
        QString title = "Stream can not be started";
        QString message = "Data streaming is already in progress. End current stream.";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_stop_acquisition_button_clicked() {
    
    if(m_stream_is_active) {
       
        // updating app state
        m_stream_is_active = false;
        set_acquisition_label(false);

        // stopping the sensors and models

        for (auto i = 0; i < m_ml_models.size(); i++) {
            if (m_ml_models[i]->get_model_status()) {
                m_ml_models[i]->stop_stream();
            }
        }

        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                m_sensor_devices[i]->stop_stream();
            }
        }

        // writing output params
        if (!m_preview_is_active) write_output_params();

        // cleaning the appropriate display
        if (m_preview_is_active) clean_preview_displays();
        else clean_main_display();

        // enabling the start acquisition button
        ui.start_acquisition_button->setEnabled(true);

    } else {
        QString title = "Stream can not be stoped";
        QString message = "The data stream is not currently active.";
        display_warning_message(title, message);
    }
    
}

void SonoAssist::on_param_file_reload_clicked() {

    // making sure devices arent streaming
    if (!m_stream_is_active) {

        // a successful load updates all sensors and models

        m_config_is_loaded = load_config_file(ui.param_file_input->text());

        if (m_config_is_loaded) {

            apply_config();
            
        } else {
            QString title = "Parameters were not loaded";
            QString message = "An invalid file path was specified or the config file is ill-formatted.";
            display_warning_message(title, message);
        }

    } else {
        QString title = "Parameters can not be loaded";
        QString message = "The stream has to be shut off.";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_param_file_browse_clicked(){

    // if user does not make a selection, dont override
    QString new_path = QFileDialog::getOpenFileName(this, "Select parameter file", QString(), 
        ".XML files (*.xml)", 0, QFileDialog::DontUseNativeDialog);

    if (!new_path.isEmpty()) {
        ui.param_file_input->setText("");
        ui.param_file_input->setText(new_path);
    }

}

void SonoAssist::on_param_edit_clicked(void) {

    ParamEditor config_editor(*m_app_params, this);
    config_editor.exec();

    apply_config();

}


void SonoAssist::on_output_folder_browse_clicked(void) {
   
    // if user does not make a selection, dont override
    QString new_path = QFileDialog::getSaveFileName(this, "Select output file path", QString(), QString(), 
        0, QFileDialog::DontUseNativeDialog);

    if (!new_path.isEmpty()) {
        ui.output_folder_input->setText(new_path);
    }

}

void SonoAssist::on_udp_port_input_editingFinished(void) {
    m_us_probe_client_p->set_udp_port(ui.udp_port_input->text().toInt());    
}

void SonoAssist::sensor_panel_selection_handler(int row, int column) {

    int device_id;

    if (!m_stream_is_active && m_config_is_loaded) {
       
        // disconnecting all devices
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if(m_sensor_devices[i]->get_connection_status()) m_sensor_devices[i]->disconnect_device();
        } 

        // toggling the state of the device with the corresponding row-id
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            device_id = m_sensor_devices[i]->get_device_id();
            if (device_id == row) {
                
                m_sensor_devices[i]->set_sensor_used(!m_sensor_devices[i]->get_sensor_used());

                if (m_sensor_devices[i]->get_sensor_used()) {
                    ui.sensor_status_table->item(device_id, 0)->setBackground(QBrush(ACTIVE_SENSOR_FIELD_COLOR));
                } else {
                    ui.sensor_status_table->item(device_id, 0)->setBackground(QBrush(INACTIVE_SENSOR_FIELD_COLOR));
                }

            }
        }

        // making sure the si only one US image source
        if (m_us_probe_client_p->get_sensor_used() && m_screen_recorder_client_p->get_sensor_used()) {

            m_screen_recorder_client_p->set_sensor_used(false);
            ui.sensor_status_table->item(m_screen_recorder_client_p->get_device_id(), 0)->setBackground(QBrush(INACTIVE_SENSOR_FIELD_COLOR));

            QString title = "Too many US image sources";
            QString message = "The screen recorder has been deactivated because there can be only one US image source.";
            display_warning_message(title, message);

        }

    } else {
        QString title = "Sensor statuses can not be updated";
        QString message = "Make sure that the acquisition is off and that paths to the config and output files are defined.";
        display_warning_message(title, message);
    }

}

/*******************************************************************************
* STATUS RELATED SLOTS
******************************************************************************/

void SonoAssist::set_device_status(int device_id, bool device_status) {

    // pointing to the target table entry
    QTableWidgetItem* device_field = ui.sensor_status_table->item(device_id, 0);

    // applying changes to the UI (sensor table)

    QString color_str = "";
    QString status_str = "";
    if (device_status) {
        status_str += "connected";
        color_str = QString(GREEN_TEXT);
    } else {
        status_str += "disconnected";
        color_str = QString(RED_TEXT);
    }

    device_field->setText(status_str);
    device_field->setForeground(QBrush(QColor(color_str)));

    if (m_sensor_conn_updates.size() > device_id) {

        // taking note of the device state update
        m_sensor_conn_updates[device_id] ++;

        // checking if all used devices have produced a state update

        bool updates_complete = true;
        for (auto i = 0; i < m_sensor_conn_updates.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                updates_complete = updates_complete && (m_sensor_conn_updates[i] > 0);
            }
        }

        if (updates_complete) {
            ui.sensor_connect_button->setEnabled(true);
        }

    }

}

void SonoAssist::add_debug_text(const QString& debug_str) {
    ui.debug_text_edit->setText(ui.debug_text_edit->toPlainText() + "\n" + debug_str);
}

/*******************************************************************************
* TIME MARKER HANDLING
******************************************************************************/

void SonoAssist::on_new_os_key_detected(int key) {

    if (m_stream_is_active) {

        // adding a time marker
        if (key == OS_A_KEY) {

            // adding the marker to the display list + json time marker list
            QString marker_str = "Time marker #" + QString::number(ui.time_marker_list->count()) +
                " - " + QString(SensorDevice::get_micro_timestamp().c_str());
            ui.time_marker_list->addItem(new QListWidgetItem(marker_str));
            m_time_markers_json.push_back(QJsonValue(marker_str));
            
            // updating the output params
            m_output_params["time_markers"] = m_time_markers_json;

        }

        // removing a time marker
        else if (key == OS_D_KEY) {

            // removing the latest time marker
            if (ui.time_marker_list->count() > 0) {
                m_time_markers_json.pop_back();
                delete ui.time_marker_list->takeItem(ui.time_marker_list->count() - 1);
            }
            
            // updating the output params
            m_output_params["time_markers"] = m_time_markers_json;

        }

    }

}

void SonoAssist::clear_time_markers(void) {

    if (ui.time_marker_list->count() > 0) {
        ui.time_marker_list->clear();
    }
    
}

/*******************************************************************************
* GRAPHICAL FUNCTIONS
******************************************************************************/

void SonoAssist::build_sensor_panel(void) {

    // defining table dimensions and headers

    ui.sensor_status_table->setColumnCount(1);
    ui.sensor_status_table->setRowCount(m_sensor_devices.size());
    ui.sensor_status_table->setHorizontalHeaderLabels(QStringList{"Sensor status"});
    
    QStringList sensor_descriptions;
    for (auto i = 0; i < m_sensor_devices.size(); i++) {
        sensor_descriptions.append(QString::fromStdString(m_sensor_devices[i]->get_device_description()));
    }
    ui.sensor_status_table->setVerticalHeaderLabels(sensor_descriptions);

    // adding the widgets to the table
    for (auto i = 0; i < ui.sensor_status_table->rowCount(); i++) {
        QTableWidgetItem* table_field = new QTableWidgetItem();
        table_field->setFlags(table_field->flags()^Qt::ItemIsEditable^Qt::ItemIsSelectable^Qt::ItemIsEnabled);
        ui.sensor_status_table->setItem(i, 0, table_field);
    }

    // streatching the headers + ajusting the height of the side panel
    ui.sensor_status_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto panelHeight = ui.sensor_status_table->horizontalHeader()->height();
    for (auto i = 0; i < ui.sensor_status_table->rowCount(); i++) {
        panelHeight += ui.sensor_status_table->rowHeight(i);
    }
    
    // setting default widget content (once the panel is built)
    for (auto i = 0; i < m_sensor_devices.size(); i++) {
        set_device_status(m_sensor_devices[i]->get_device_id(), false);
    }

    // connecting the item selection handler
    connect(ui.sensor_status_table, &QTableWidget::cellClicked,
        this, &SonoAssist::sensor_panel_selection_handler);

}

void SonoAssist::set_acquisition_label(bool active) {

    QString style_sheet;
    QString label_text = "acquisition status : ";
    if(active) {
        label_text += "(in progress)";
        style_sheet = QString("QLabel {color : %1;}").arg(QString(GREEN_TEXT));
    } else {
        label_text += "(inactive)";
        style_sheet = QString("QLabel {color : %1;}").arg(QString(RED_TEXT));
    }
    ui.acquisition_label->setText(label_text);
    ui.acquisition_label->setStyleSheet(style_sheet);

}

void SonoAssist::display_warning_message(const QString& title, const QString& message) {
    QMessageBox::warning(this, title, message);
}

/*******************************************************************************
* GRAPHICS SCENCE FUNCTIONS
******************************************************************************/

void SonoAssist::configure_main_display(void) {

    // loading the main display dimensions
    try {

        m_main_us_img_width = std::stoi((*m_app_params)["us_image_main_display_width"]);
        if (m_main_us_img_width > MAIN_DISPLAY_DEFAULT_WIDTH) {
            m_main_us_img_width = MAIN_DISPLAY_DEFAULT_WIDTH;
        }

        m_main_us_img_height = std::stoi((*m_app_params)["us_image_main_display_height"]);
        if (m_main_us_img_height > MAIN_DISPLAY_DEFAULT_HEIGHT) {
            m_main_us_img_height = MAIN_DISPLAY_DEFAULT_HEIGHT;
        }
    
    } catch (...) {
        m_main_us_img_width = MAIN_DISPLAY_DEFAULT_WIDTH;
        m_main_us_img_height = MAIN_DISPLAY_DEFAULT_HEIGHT;
    }

    // rebuild the main display
    if (!m_preview_is_active) {
        remove_main_display();
        generate_main_display();
    }

}

void SonoAssist::generate_main_display(void) {

    // creating the main background
    m_main_bg_i_p = std::make_unique<QPixmap>(m_main_us_img_width, m_main_us_img_height);
    m_main_bg_i_p->fill(QColor(IMG_PLACE_HOLDER_COLOR));
    m_main_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_main_bg_i_p.get()));
    m_main_scene_p->addItem(m_main_bg_p.get());

    // centering the background
    int x_pos = (m_main_scene_p->width() / 2) - (m_main_us_img_width / 2);
    int y_pos = (m_main_scene_p->height() / 2) - (m_main_us_img_height / 2);
    m_main_bg_p->setPos(x_pos, y_pos);

    generate_eye_tracker_targets();

    // writing the background position (screen coordinates) to the output params
    QPoint view_point = ui.graphicsView->mapFromScene(m_main_bg_p->pos());
    QPoint screen_point = ui.graphicsView->viewport()->mapToGlobal(view_point);
    m_output_params["display_x"] = screen_point.x();
    m_output_params["display_y"] = screen_point.y();
    m_output_params["display_width"] = m_main_us_img_width;
    m_output_params["display_height"] = m_main_us_img_height;

}

void SonoAssist::remove_main_display(void) {

    m_main_bg_p.release();
    m_main_pixmap_p.release();
    m_main_scene_p->clear();

}

void SonoAssist::clean_main_display(void) {

    if (m_main_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_main_pixmap_p.get());
        m_main_pixmap_p.reset();
    }

}

void SonoAssist::generate_preview_displays(void) {

    // creating the background for the left preview image
    m_preview_left_bg_i_p = std::make_unique<QPixmap>(PREVIEW_LEFT_DISPLAY_WIDTH, PREVIEW_LEFT_DISPLAY_HEIGHT);
    m_preview_left_bg_i_p->fill(QColor(IMG_PLACE_HOLDER_COLOR));
    m_preview_left_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_preview_left_bg_i_p.get()));
    m_main_scene_p->addItem(m_preview_left_bg_p.get());
    m_preview_left_bg_p->setPos(PREVIEW_LEFT_DISPLAY_X_OFFSET, PREVIEW_LEFT_DISPLAY_Y_OFFSET);
    m_preview_left_bg_p->setZValue(1);

    // creating the backgroud for the right preview image
    m_preview_right_bg_i_p = std::make_unique<QPixmap>(PREVIEW_RIGHT_DISPLAY_WIDTH, PREVIEW_RIGHT_DISPLAY_HEIGHT);
    m_preview_right_bg_i_p->fill(QColor(IMG_PLACE_HOLDER_COLOR));
    m_preview_right_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_preview_right_bg_i_p.get()));
    m_main_scene_p->addItem(m_preview_right_bg_p.get());
    m_preview_right_bg_p->setPos(PREVIEW_RIGHT_DISPLAY_X_OFFSET, PREVIEW_RIGHT_DISPLAY_Y_OFFSET);
    m_preview_right_bg_p->setZValue(1);

    // loading the eyetracking crosshair
    QPixmap crosshair_img(QString((*m_app_params)["eye_tracker_crosshairs_path"].c_str()));
    crosshair_img = crosshair_img.scaled(EYETRACKER_CROSSHAIRS_WIDTH, EYETRACKER_CROSSHAIRS_HEIGHT, Qt::KeepAspectRatio);
    m_eyetracker_crosshair_p = std::make_unique<QGraphicsPixmapItem>(crosshair_img);

    // placing the crosshair in the middle of the display
    int x_pos = PREVIEW_RIGHT_DISPLAY_X_OFFSET + (PREVIEW_RIGHT_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
    int y_pos = PREVIEW_RIGHT_DISPLAY_Y_OFFSET + (PREVIEW_RIGHT_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
    m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
    m_eyetracker_crosshair_p->setZValue(3);
    m_main_scene_p->addItem(m_eyetracker_crosshair_p.get());

}

void SonoAssist::remove_preview_displays(void) {
    
    // removing background + crosshairs
    m_preview_left_bg_p.release();
    m_preview_right_bg_p.release();
    m_eyetracker_crosshair_p.release();
    
    // removing images
    m_preview_left_pixmap_p.release();
    m_preview_right_pixmap_p.release();
    
    m_main_scene_p->clear();
}

void SonoAssist::clean_preview_displays() {

    // removing the left preview
    if (m_preview_left_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_preview_left_pixmap_p.get());
        m_preview_left_pixmap_p.reset();
    }
    // removing the right preview
    if (m_preview_right_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_preview_right_pixmap_p.get());
        m_preview_right_pixmap_p.reset();
    }
    // recentering the gaze crosshairs
    if (m_eyetracker_crosshair_p.get() != nullptr) {
        int x_pos = PREVIEW_RIGHT_DISPLAY_X_OFFSET + (PREVIEW_RIGHT_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
        int y_pos = PREVIEW_RIGHT_DISPLAY_Y_OFFSET + (PREVIEW_RIGHT_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
        m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
    }

}

void SonoAssist::generate_eye_tracker_targets() {
    
    // placing eye tracker targets (accuracy measurement mode)
    if (m_eye_tracker_targets) {

        // getting main display coordinates
        QPointF display_pos = m_main_bg_p->pos();
        int display_x_pos = display_pos.x();
        int display_y_pos = display_pos.y();

        for (auto i = 0; i < EYETRACKER_N_ACC_TARGETS; i++) {

            // loading the eyetracker target
            QPixmap crosshair_img(QString((*m_app_params)["eye_tracker_target_path"].c_str()));
            crosshair_img = crosshair_img.scaled(EYETRACKER_ACC_TARGET_WIDTH, EYETRACKER_ACC_TARGET_HEIGHT, Qt::KeepAspectRatio);
            QGraphicsPixmapItem* curr_target_p = new QGraphicsPixmapItem(crosshair_img);

            // defining the current target position (on display)
            int target_x_pos = 0, target_y_pos = 0;
            switch (i) {
                // top left
                case 0:
                    target_x_pos = display_x_pos - EYETRACKER_ACC_TARGET_WIDTH/2;
                    target_y_pos = display_y_pos - EYETRACKER_ACC_TARGET_HEIGHT/2;
                break;
                // bottom left
                case 1:
                    target_x_pos = display_x_pos - EYETRACKER_ACC_TARGET_WIDTH/2;
                    target_y_pos = display_y_pos + m_main_us_img_height - EYETRACKER_ACC_TARGET_HEIGHT/2;
                break;
                // top rigth
                case 2:
                    target_x_pos = display_x_pos + m_main_us_img_width - EYETRACKER_ACC_TARGET_WIDTH/2;
                    target_y_pos = display_y_pos - EYETRACKER_ACC_TARGET_HEIGHT/2;
                break;
                // bottom right
                case 3:
                    target_x_pos = display_x_pos + m_main_us_img_width - EYETRACKER_ACC_TARGET_WIDTH/2;
                    target_y_pos = display_y_pos + m_main_us_img_height - EYETRACKER_ACC_TARGET_HEIGHT/2;
                break;
            }

            // placing the crosshair in the middle of the display
            curr_target_p->setPos(target_x_pos, target_y_pos);
            curr_target_p->setZValue(3);
            m_main_scene_p->addItem(curr_target_p);

        }

    }

}

/*******************************************************************************
* PARAMETER LOADING & WRITING FUNCTIONS
******************************************************************************/

std::string SonoAssist::create_log_folder(void) {

    std::string log_file_path = "";

    // getting current user name
    // https://stackoverflow.com/questions/11587426/get-current-username-in-c-on-windows
    TCHAR username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    GetUserName((TCHAR*)username, &size);
    std::wstring temp1(&username[0]);
    
    // building the path to the log folder
    std::string log_folder_path(DEFAULT_LOG_PATH);
    log_folder_path.replace(log_folder_path.find("<username>"), 10, std::string(temp1.begin(), temp1.end()).c_str());

    // converting str to proper windows format
    // creating the output folder and updating the clients
    std::wstring temp2 = std::wstring(log_folder_path.begin(), log_folder_path.end());
    if (CreateDirectory(temp2.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError()) {

       // creating the log file
        log_file_path = log_folder_path + "/acquisition_log.log";
        std::ofstream log_file;
        log_file.open(log_file_path);
        log_file.close();

    }

    return log_file_path; 
   
}

bool SonoAssist::create_output_folder() {

    std::wstring stemp;
    bool valid_output_folder = false;

    std::string parent_folder_path = ui.output_folder_input->text().toStdString();
    std::string output_folder_path = parent_folder_path + "/acquisition_" + SensorDevice::get_micro_timestamp();

    // making sure the parent folder is created
    // https://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode
    stemp = std::wstring(parent_folder_path.begin(), parent_folder_path.end());
    valid_output_folder = CreateDirectory(stemp.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError(); 

    // creating the output folder (in the parent folder)
    stemp = std::wstring(output_folder_path.begin(), output_folder_path.end());
    valid_output_folder = valid_output_folder && (CreateDirectory(stemp.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError());
    
    // updating the devices
    if (valid_output_folder) {
        
        m_output_folder_path = output_folder_path;

        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            m_sensor_devices[i]->set_output_file(output_folder_path);
        }

    } else {
        valid_output_folder = false;
        QString title = "Unable to create output folder";
        QString message = "The application failed to create the output folder for data files.";
        display_warning_message(title, message);
    }

    return valid_output_folder;

}

bool SonoAssist::load_config_file(const QString& param_file_path) {

    // loading the contents of the param file
    QDomDocument doc("params");
    QFile file(param_file_path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    // trying to get xml from the file
    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }
    file.close();

    // getting the root element of the xml document
    QDomNodeList children;
    QDomElement docElem = doc.documentElement();

    // filling the parameter map with xml content
    for (auto& parameter : *m_app_params) {
        children = docElem.elementsByTagName(QString(parameter.first.c_str()));
        if (children.count() == 1) {

            // handling single elements
            if (children.at(0).childNodes().length() == 1) {
                parameter.second = children.at(0).firstChild().nodeValue().toStdString();
            }
            
            // handling 2 levels of elements
            else {
                std::string param_value = "";
                for (auto i(0); i < children.at(0).childNodes().length(); i++) {
                    param_value += children.at(0).childNodes().at(i).firstChild().nodeValue().toStdString();
                    if (i != children.at(0).childNodes().length() - 1) param_value += ", ";
                }
                parameter.second = param_value;
            }
            
        }
    }

    return true;

}

void SonoAssist::apply_config(void) {

    int entities_with_redis = 0;

    for (auto i = 0; i < m_sensor_devices.size(); i++) {
        if (m_sensor_devices[i]->get_connection_status()) {
            m_sensor_devices[i]->disconnect_device();
        }
        m_sensor_devices[i]->set_configuration(m_app_params);
        if (m_sensor_devices[i]->get_redis_state()) entities_with_redis++;
    }

    for (auto i = 0; i < m_ml_models.size(); i++) {
        m_ml_models[i]->set_configuration(m_app_params);
        if (m_ml_models[i]->get_redis_state()) entities_with_redis++;
    }

    // applying config changes to the UI
    configure_main_display();

    // launching redis server, if required
    if (entities_with_redis > 0) {
        if (!process_startup((*m_app_params)["redis_server_path"], m_redis_process)) {
            QString debug_str = "\nSonoAssist - failed to launch redis server";
            qDebug() << debug_str << GetLastError();
            add_debug_text(debug_str);
        }
    }

}

void SonoAssist::write_output_params(void) {

    // opening the output file
    QString output_file_path = QString(m_output_folder_path.c_str()) + "/sono_assist_output_params.json";
    QFile output_file(output_file_path);

    // writing the output params
    if (output_file.open(QFile::WriteOnly | QFile::Text | QFile::Truncate))
        output_file.write(QJsonDocument(m_output_params).toJson());
    output_file.close();

    // clearing the time markers
    m_time_markers_json = QJsonArray();
    m_output_params["time_markers"] = m_time_markers_json;

}

/*******************************************************************************
* UTILITY FUNCTIONS
******************************************************************************/

bool SonoAssist::check_device_connections() {
    
    int n_used_devices = 0;
    bool used_devices_connected = true;

    for (auto i = 0; i < m_sensor_devices.size(); i++) {
        if (m_sensor_devices[i]->get_sensor_used()) {
            used_devices_connected = m_sensor_devices[i]->get_connection_status();
            if (!used_devices_connected) break;
            n_used_devices++;
        }
    }

    return used_devices_connected && (n_used_devices > 0);
}

bool SonoAssist::check_devices_streaming() {

    int n_used_devices = 0;
    bool used_devices_streaming = true;

    for (auto i = 0; i < m_sensor_devices.size(); i++) {
        if (m_sensor_devices[i]->get_sensor_used()) {
            used_devices_streaming = m_sensor_devices[i]->get_stream_status();
            if (!used_devices_streaming) break;
            n_used_devices++;
        }
    }

    return used_devices_streaming && (n_used_devices > 0);

}
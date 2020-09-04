#include "SonoAssist.h"

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

	// setting up the gui
	ui.setupUi(this);
    build_sensor_panel();
    set_acquisition_label(false);
   
    // setting the graphics scene widget
    m_main_scene_p = std::make_unique<QGraphicsScene>(ui.graphicsView);
    ui.graphicsView->setScene(m_main_scene_p.get());
    
    // generating the US image acquisition display
    generate_normal_display();

    // creating the sensor clients
    m_gaze_tracker_client_p = std::make_shared<GazeTracker>();
    m_camera_client_p = std::make_shared<RGBDCameraClient>();
    m_screen_recorder_client_p = std::make_shared<ScreenRecorder>();
    m_us_probe_client_p = std::make_shared<ClariusProbeClient>();
    m_metawear_client_p = std::make_shared<MetaWearBluetoothClient>();
    m_sensor_devices = std::vector<std::shared_ptr<SensorDevice>>({ m_gaze_tracker_client_p, m_camera_client_p , m_screen_recorder_client_p, 
                                                                    m_us_probe_client_p, m_metawear_client_p});
	// predefining the parameters in the config file
    m_app_params = std::make_shared<config_map>();
    *m_app_params = {{"ext_imu_ble_address", ""}, {"ext_imu_to_redis", ""}, 
                     {"us_probe_ip_address", ""}, {"us_probe_udp_port", ""},
                     {"ext_imu_redis_entry", ""}, {"ext_imu_redis_rate_div", ""}, 
                     {"eye_tracker_to_redis", ""},  {"eye_tracker_redis_rate_div", ""},
                     {"eye_tracker_redis_entry", ""},  {"eye_tracker_crosshairs_path", ""},
                     {"rgb_camera_active", ""}, {"ext_imu_active" , ""}, {"eye_tracker_active", ""}, 
                     {"screen_recorder_active", ""}, {"us_probe_active", ""} };

}

SonoAssist::~SonoAssist(){

    try {

        // making sur the streams are shut off
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_stream_status()) {
                m_sensor_devices[i]->stop_stream();
                m_sensor_devices[i]->disconnect_device();
            }
        }

        // cleaning and removing the displays
        remove_normal_display();
        remove_preview_display();

    } catch (...) {
        qDebug() << "n\SonoAssist - failed to stop the devices from treaming (in destructor)";
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// slots

void SonoAssist::on_new_us_screen_capture(QImage new_image) {

    // removing the old image
    if (m_eyetracker_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_eyetracker_pixmap_p.get());
        m_eyetracker_pixmap_p.reset();
    }

    // defining the new image and adding it to the scene (under the gazepoint)
    m_eyetracker_pixmap_p = std::make_unique<QGraphicsPixmapItem>(QPixmap::fromImage(new_image));
    m_eyetracker_pixmap_p->setPos(PREVIEW_PROBE_DISPLAY_X_OFFSET, PREVIEW_PROBE_DISPLAY_Y_OFFSET);
    m_eyetracker_pixmap_p->setZValue(2);
    m_main_scene_p->addItem(m_eyetracker_pixmap_p.get());

    // when acquisition is stoped during execution
    if (!m_stream_is_active) {
        m_main_scene_p->removeItem(m_eyetracker_pixmap_p.get());
        m_eyetracker_pixmap_p.reset();
    }

}

void SonoAssist::on_new_camera_image(QImage new_image) {
   
    // removing the old image
    if (m_camera_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_camera_pixmap_p.get());
        m_camera_pixmap_p.reset();
    }

    // defining the new image and adding it to the scene
    m_camera_pixmap_p = std::make_unique<QGraphicsPixmapItem>(QPixmap::fromImage(new_image));
    m_camera_pixmap_p->setPos(CAMERA_DISPLAY_X_OFFSET, CAMERA_DISPLAY_Y_OFFSET);
    m_camera_pixmap_p->setZValue(2);
    m_main_scene_p->addItem(m_camera_pixmap_p.get());

    // when acquisition is stoped during execution
    if (!m_stream_is_active) {
        m_main_scene_p->removeItem(m_camera_pixmap_p.get());
        m_camera_pixmap_p.reset();
    }
  
}

void SonoAssist::on_new_gaze_point(float x, float y) {

    // processing incoming coordinates
    if (x > 1) x = 1;
    if (x < 0) x = 0;
    if (y > 1) y = 1;
    if (y < 0) y = 0;

    // updating the eyetracker crosshair position
    if (m_eyetracker_crosshair_p != nullptr) {
        int x_pos = PREVIEW_PROBE_DISPLAY_X_OFFSET + (PREVIEW_PROBE_DISPLAY_WIDTH * x) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
        int y_pos = PREVIEW_PROBE_DISPLAY_Y_OFFSET + (PREVIEW_PROBE_DISPLAY_HEIGHT * y) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
        m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
    }
    
}

void SonoAssist::on_sensor_connect_button_clicked(){

    // make sure requirements are filled
    if (!m_stream_is_active && m_config_is_loaded && m_output_is_loaded) {
     
        // loading the latest version of the configs
        if (load_config_file(ui.param_file_input->text())) {

            configure_device_clients();

            for (auto i = 0; i < m_sensor_devices.size(); i++) {
                if (m_sensor_devices[i]->get_sensor_used()) {
                    m_sensor_devices[i]->set_configuration(m_app_params);
                    m_sensor_devices[i]->set_output_file(m_output_folder_path);
                    m_sensor_devices[i]->connect_device();
                }
            }

        } else {
            QString title = "Error while loading configs";
            QString message = "The config file contains ill-formatted XML.";
            display_warning_message(title, message);
        }
            
    } else {
        QString title = "Devices can not be connected";
        QString message = "Make sure that the acquisition is off and that paths to the config and output files are defined";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_acquisition_preview_box_clicked() {
   
    // acquisition preview mode requirements filled
    if (!m_stream_is_active && check_device_connections()) {
        
        m_preview_is_active = ui.acquisition_preview_box->isChecked();

        // setting the preview state for the used devices
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                m_sensor_devices[i]->set_stream_preview_status(m_preview_is_active);
            }
        }

        // toggling between the 2 display types
        if (m_preview_is_active) {
            remove_normal_display();
            generate_preview_display();
        } else {
            remove_preview_display();
            generate_normal_display();
        }
  
    } else {

        // reverting the check box selection
        ui.acquisition_preview_box->setChecked(!ui.acquisition_preview_box->isChecked());
        
        QString title = "Stream preview mode can not be toggled";
        QString message = "Make sure devices are connected and are not currently streaming.";
        display_warning_message(title, message);
    
    }

}

void SonoAssist::on_start_acquisition_button_clicked() {
    
    // making sure the device isnt already streaming
    if (!m_stream_is_active) {

        // if all used devices are ready, start the acquisition
        if (check_device_connections()) {

            // updating the app state
            m_stream_is_active = true;
            set_acquisition_label(true);

            // starting the sensor streams
            for (auto i = 0; i < m_sensor_devices.size(); i++) {
                if (m_sensor_devices[i]->get_sensor_used()) 
                    m_sensor_devices[i]->start_stream();
            }

        } else {
            QString title = "Stream can not be started";
            QString message = "The following devices are not ready for acquisition : [ ";
            if (!m_us_probe_client_p->get_connection_status()) message += "US Probe (Clarius),";
            if (!m_screen_recorder_client_p->get_connection_status()) message += "Screen recorder,";
            if (!m_gaze_tracker_client_p->get_connection_status()) message += "Tobii 4C (eye tracker), ";
            if (!m_metawear_client_p->get_connection_status()) message += "MetaMotionC (external IMU), ";
            if (!m_camera_client_p->get_connection_status()) message += "Intel Realsens camera (RGBD camera) ";
            message += "].";
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

        // stoping the sensor streams
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used())
                m_sensor_devices[i]->stop_stream();
        }

        // cleaning the appropriate display
        if (m_preview_is_active) {
            clean_preview_display();
        } else {
            clean_normal_display();
        }

    } else {
        QString title = "Stream can not be stoped";
        QString message = "The data stream is not currently active.";
        display_warning_message(title, message);
    }
    
}

void SonoAssist::on_param_file_input_editingFinished(){
   
    m_config_is_loaded = load_config_file(ui.param_file_input->text());
        
    if (!m_config_is_loaded) {
        QString title = "Parameters were not loaded";
        QString message = "An invalid file path was specified or the config file is ill-formatted.";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_output_folder_input_editingFinished() {

    std::string output_folder_path = ui.output_folder_input->text().toStdString();

    // creating the output folder (for data files)
    if (CreateDirectory(output_folder_path.c_str(), NULL) ||
        ERROR_ALREADY_EXISTS == GetLastError()){

        m_output_is_loaded = true;
        m_output_folder_path = output_folder_path;

    } else {
        m_output_is_loaded = false;
        QString title = "Unable to create output folder";
        QString message = "The application failed to create the output folder for data files.";
        display_warning_message(title, message);
    }
    
}

void SonoAssist::on_param_file_browse_clicked(){

    // if user does not make a selection, dont override
    QString new_path = QFileDialog::getOpenFileName(this, "Select parameter file", QString(), ".XML files (*.xml)");
    if (!new_path.isEmpty()) {
        ui.param_file_input->setText("");
        ui.param_file_input->setText(new_path);
    }

    on_param_file_input_editingFinished();

}

void SonoAssist::on_output_folder_browse_clicked(void) {
   
    // if user does not make a selection, dont override
    QString new_path = QFileDialog::getSaveFileName(this, "Select output file path", QString());
    if (!new_path.isEmpty()) {
        ui.output_folder_input->setText(new_path);
    }

    on_output_folder_input_editingFinished();

}

void SonoAssist::on_ext_imu_status_change(bool device_status){
    set_device_status(device_status, EXT_IMU);
}

void SonoAssist::on_eye_tracker_status_change(bool device_status) {
    set_device_status(device_status, EYE_TRACKER);
}

void SonoAssist::on_rgbd_camera_status_change(bool device_status) {
    set_device_status(device_status, RGBD_CAMERA);
}

void SonoAssist::on_screen_recorder_status_change(bool device_status) {
    set_device_status(device_status, SCREEN_RECORDER);
}

void SonoAssist::on_us_probe_status_change(bool device_status) {
    set_device_status(device_status, US_PROBE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// graphical functions

void SonoAssist::build_sensor_panel(void) {

    int index;

    // defining table dimensions and headers
    ui.sensor_status_table->setRowCount(5);
    ui.sensor_status_table->setColumnCount(1);
    ui.sensor_status_table->setHorizontalHeaderLabels(QStringList{"Sensor status"});
    ui.sensor_status_table->setVerticalHeaderLabels(QStringList{"Ext IMU", "Eye tracker", "RGBD camera", "US Probe", "Screen recorder"});

    // adding the widgets to the table
    for (index = 0; index < ui.sensor_status_table->rowCount(); index++) {
        ui.sensor_status_table->setItem(index, 0, new QTableWidgetItem());
    }

    // streatching the headers
    ui.sensor_status_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // ajusting the height of the side panel
    auto panelHeight = ui.sensor_status_table->horizontalHeader()->height();
    for (index = 0; index < ui.sensor_status_table->rowCount(); index++) {
        panelHeight += ui.sensor_status_table->rowHeight(index);
    }
    
    // setting default widget content
    set_device_status(false, EXT_IMU);
    set_device_status(false, US_PROBE);
    set_device_status(false, RGBD_CAMERA);
    set_device_status(false, EYE_TRACKER);
    set_device_status(false, SCREEN_RECORDER);

}

void SonoAssist::set_device_status(bool device_status, sensor_device_t device) {

    // pointing to the target table entry
    QTableWidgetItem* device_field = ui.sensor_status_table->item(device, 0);
    
    // preparing widget text and color
    QString color_str = "";
    QString status_str = "";
    if (device_status) {
        status_str += "connected";
        color_str = QString(GREEN_TEXT);
    }
    else {
        status_str += "disconnected";
        color_str = QString(RED_TEXT);
    }

    // applying changes to the widget
    device_field->setText(status_str);
    device_field->setForeground(QBrush(QColor(color_str)));
    
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

void SonoAssist::display_warning_message(QString title, QString message) {
    QMessageBox::warning(this, title, message);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// Graphics scene functions

void SonoAssist::generate_preview_display(void) {

    // creating the placeholder image for the camera viewfinder
    m_camera_bg_i_p = std::make_unique<QPixmap>(CAMERA_DISPLAY_WIDTH, CAMERA_DISPLAY_HEIGHT);
    m_camera_bg_i_p->fill(QColor("#FFFFFF"));
    m_camera_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_camera_bg_i_p.get()));
    m_main_scene_p->addItem(m_camera_bg_p.get());
    m_camera_bg_p->setPos(CAMERA_DISPLAY_X_OFFSET, CAMERA_DISPLAY_Y_OFFSET);
    m_camera_bg_p->setZValue(1);

    // creating the backgroud image for the (probe image / gaze point) display
    m_eye_tracker_bg_i_p = std::make_unique<QPixmap>(PREVIEW_PROBE_DISPLAY_WIDTH, PREVIEW_PROBE_DISPLAY_HEIGHT);
    m_eye_tracker_bg_i_p->fill(QColor("#FFFFFF"));
    m_eye_tracker_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_eye_tracker_bg_i_p.get()));
    m_main_scene_p->addItem(m_eye_tracker_bg_p.get());
    m_eye_tracker_bg_p->setPos(PREVIEW_PROBE_DISPLAY_X_OFFSET, PREVIEW_PROBE_DISPLAY_Y_OFFSET);
    m_eye_tracker_bg_p->setZValue(1);

    // loading the eyetracking crosshair
    QPixmap crosshair_img(QString((*m_app_params)["eye_tracker_crosshairs_path"].c_str()));
    crosshair_img = crosshair_img.scaled(EYETRACKER_CROSSHAIRS_WIDTH, EYETRACKER_CROSSHAIRS_HEIGHT, Qt::KeepAspectRatio);
    m_eyetracker_crosshair_p = std::make_unique<QGraphicsPixmapItem>(crosshair_img);

    // placing the crosshair in the middle of the display
    int x_pos = PREVIEW_PROBE_DISPLAY_X_OFFSET + (PREVIEW_PROBE_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
    int y_pos = PREVIEW_PROBE_DISPLAY_Y_OFFSET + (PREVIEW_PROBE_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
    m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
    m_eyetracker_crosshair_p->setZValue(3);
    m_main_scene_p->addItem(m_eyetracker_crosshair_p.get());

}

void SonoAssist::remove_preview_display(void) {
    
    // placeholders + crosshairs
    m_camera_bg_p.release();
    m_eye_tracker_bg_p.release();
    m_eyetracker_crosshair_p.release();
    
    // images
    m_camera_pixmap_p.release();
    m_eyetracker_pixmap_p.release();
    
    m_main_scene_p->clear();
}

void SonoAssist::clean_preview_display() {

    // removing the last camera image
    if (m_camera_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_camera_pixmap_p.get());
        m_camera_pixmap_p.reset();
    }
    // removing the last US image
    if (m_eyetracker_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_eyetracker_pixmap_p.get());
        m_eyetracker_pixmap_p.reset();
    }
    // recentering the gaze crosshairs
    if (m_eyetracker_crosshair_p.get() != nullptr) {
        int x_pos = PREVIEW_PROBE_DISPLAY_X_OFFSET + (PREVIEW_PROBE_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
        int y_pos = PREVIEW_PROBE_DISPLAY_Y_OFFSET + (PREVIEW_PROBE_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
        m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
    }

}

void SonoAssist::generate_normal_display(void) {

    // creating the placeholder image US image
    m_us_bg_i_p = std::make_unique<QPixmap>(PROBE_DISPLAY_WIDTH, PROBE_DISPLAY_HEIGHT);
    m_us_bg_i_p->fill(QColor("#FFFFFF"));
    m_us_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_us_bg_i_p.get()));
    m_main_scene_p->addItem(m_us_bg_p.get());
    
    // centering the placeholder
    int x_pos = (m_main_scene_p->width() / 2) - (PROBE_DISPLAY_WIDTH / 2);
    int y_pos = (m_main_scene_p->height() / 2) - (PROBE_DISPLAY_HEIGHT / 2);
    m_us_bg_p->setPos(x_pos, y_pos);

}

void SonoAssist::remove_normal_display(void) {
    m_us_bg_p.release();
    m_us_pixmap_p.release();
    m_main_scene_p->clear();
}

void SonoAssist::clean_normal_display(void) {

    // removing the last US image
    if (m_us_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_us_pixmap_p.get());
        m_us_pixmap_p.reset();
    }
 
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// utility functions

bool SonoAssist::load_config_file(QString param_file_path) {

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
    QDomElement docElem = doc.documentElement();
    QDomNodeList children;

    // filling the parameter hash with xml content
    for (auto& parameter : *m_app_params) {
        children = docElem.elementsByTagName(QString(parameter.first.c_str()));
        if (children.count() == 1) {
            parameter.second = children.at(0).firstChild().nodeValue().toStdString();
        } else {
            return false;
        }
    }

    return true;

}

/*
* Checks if all used devices are ready for acquisition (synchronisation)
*/
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

/*
* Configures the different clients according to the config file
*/
void SonoAssist::configure_device_clients() {

    // making sure requirements are filled
    if (!m_stream_is_active && m_config_is_loaded) {
    
        // resetting the relevant client configuration
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            m_sensor_devices[i]->disconnect_device();
            m_sensor_devices[i]->set_sensor_used(false);
        }

        // configuring the ext IMU client
        if (QString((*m_app_params)["ext_imu_active"].c_str()) == "true") {
            m_metawear_client_p->set_sensor_used(true);
            connect(m_metawear_client_p.get(), &MetaWearBluetoothClient::device_status_change,
                this, &SonoAssist::on_ext_imu_status_change);
        }

        // configuring the eye tracker client
        if (QString((*m_app_params)["eye_tracker_active"].c_str()) == "true") {
            m_gaze_tracker_client_p->set_sensor_used(true);
            connect(m_gaze_tracker_client_p.get(), &GazeTracker::device_status_change,
                this, &SonoAssist::on_eye_tracker_status_change);
            connect(m_gaze_tracker_client_p.get(), &GazeTracker::new_gaze_point,
                this, &SonoAssist::on_new_gaze_point);
        }

        // configuring the RGB D camera client
        if (QString((*m_app_params)["rgb_camera_active"].c_str()) == "true") {
            m_camera_client_p->set_sensor_used(true);
            connect(m_camera_client_p.get(), &RGBDCameraClient::device_status_change,
                this, &SonoAssist::on_rgbd_camera_status_change);
            connect(m_camera_client_p.get(), &RGBDCameraClient::new_video_frame,
                this, &SonoAssist::on_new_camera_image);
        }

        // configuring the screen recorder client
        if (QString((*m_app_params)["screen_recorder_active"].c_str()) == "true") {
            m_screen_recorder_client_p->set_sensor_used(true);
            connect(m_screen_recorder_client_p.get(), &ScreenRecorder::device_status_change,
                this, &SonoAssist::on_screen_recorder_status_change);
            connect(m_screen_recorder_client_p.get(), &ScreenRecorder::new_window_capture,
                this, &SonoAssist::on_new_us_screen_capture);
        }

        // configuring the US probe client
        if (QString((*m_app_params)["us_probe_active"].c_str()) == "true") {
            m_us_probe_client_p->set_sensor_used(true);
            connect(m_us_probe_client_p.get(), &ScreenRecorder::device_status_change,
                this, &SonoAssist::on_us_probe_status_change);
        }

        // there can only be one US image source
        if (m_us_probe_client_p->get_sensor_used() && m_screen_recorder_client_p->get_sensor_used()) {
            m_screen_recorder_client_p->set_sensor_used(false);
            QString title = "Too many US image sources";
            QString message = "The screen recorder has been deactivated because there can be only on US image source.";
            display_warning_message(title, message);
        }

    }

}
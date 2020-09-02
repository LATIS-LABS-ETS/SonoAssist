#include "SonoAssist.h"

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

	// setting up the gui
	ui.setupUi(this);
    build_sensor_panel();
    set_acquisition_label(false);
   
    // setting the scene widget
    m_main_scene_p = std::make_unique<QGraphicsScene>(ui.graphicsView);
    ui.graphicsView->setScene(m_main_scene_p.get());

    // creating the placeholder image for the camera viewfinder
    // positioning the placeholderon the scene
    m_camera_bg_i_p = std::make_unique<QPixmap>(CAMERA_DISPLAY_WIDTH, CAMERA_DISPLAY_HEIGHT);
    m_camera_bg_i_p->fill(QColor("#FFFFFF"));
    m_camera_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_camera_bg_i_p.get()));
    m_main_scene_p->addItem(m_camera_bg_p.get());
    m_camera_bg_p->setPos(CAMERA_DISPLAY_X_OFFSET, CAMERA_DISPLAY_Y_OFFSET);
    m_camera_bg_p->setZValue(1);

    // creating the backgroud image for the eyetracker display
    // positioning the image on the scene
    m_eye_tracker_bg_i_p = std::make_unique<QPixmap>(PROBE_DISPLAY_WIDTH, PROBE_DISPLAY_HEIGHT);
    m_eye_tracker_bg_i_p->fill(QColor("#FFFFFF"));
    m_eye_tracker_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_eye_tracker_bg_i_p.get()));
    m_main_scene_p->addItem(m_eye_tracker_bg_p.get());
    m_eye_tracker_bg_p->setPos(PROBE_DISPLAY_X_OFFSET, PROBE_DISPLAY_Y_OFFSET);
    m_eye_tracker_bg_p->setZValue(1);
 
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

    } catch (...) {
        qDebug() << "n\SonoAssist - failed to stop the devices from treaming (in destructor)";
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// slots

void SonoAssist::on_new_camera_image(QImage new_image) {
   
    // removing the old image
    if (m_camera_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_camera_pixmap_p.get());
        m_camera_pixmap_p.reset();
    }

    // defining the new image and adding it to the scene
    m_camera_pixmap_p = std::make_unique<QGraphicsPixmapItem>(QPixmap::fromImage(new_image));
    m_main_scene_p->addItem(m_camera_pixmap_p.get());
    m_camera_pixmap_p->setZValue(2);
    m_camera_pixmap_p->setPos(CAMERA_DISPLAY_X_OFFSET, CAMERA_DISPLAY_Y_OFFSET);
  
}

void SonoAssist::on_new_gaze_point(float x, float y) {

    // processing incoming coordinates
    if (x > 1) x = 1;
    if (x < 0) x = 0;
    if (y > 1) y = 1;
    if (y < 0) y = 0;

    // updating the eyetracker crosshair position
    int x_pos = PROBE_DISPLAY_X_OFFSET + (PROBE_DISPLAY_WIDTH * x) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
    int y_pos = PROBE_DISPLAY_Y_OFFSET + (PROBE_DISPLAY_HEIGHT * y) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
    m_eyetracker_crosshair_p->setPos(x_pos, y_pos);

}

void SonoAssist::on_new_us_screen_capture(QImage new_image) {

    // removing the old image
    if (m_eyetracker_pixmap_p.get() != nullptr) {
        m_main_scene_p->removeItem(m_eyetracker_pixmap_p.get());
        m_eyetracker_pixmap_p.reset();
    }

    // defining the new image and adding it to the scene (under the gazepoint)
    m_eyetracker_pixmap_p = std::make_unique<QGraphicsPixmapItem>(QPixmap::fromImage(new_image));
    m_eyetracker_pixmap_p->setPos(PROBE_DISPLAY_X_OFFSET, PROBE_DISPLAY_Y_OFFSET);
    m_eyetracker_pixmap_p->setZValue(2);
    m_main_scene_p->addItem(m_eyetracker_pixmap_p.get());

}

void SonoAssist::on_sensor_connect_button_clicked(){

    // getting the output folder path
    std::string output_folder_path = ui.output_folder_input->text().toStdString();

    // connecting to the devices once requirements are filled
    if (m_config_is_loaded && m_output_is_loaded) {
      
        // creating the output folder (for data files)
        if (CreateDirectory(output_folder_path.c_str(), NULL) ||
            ERROR_ALREADY_EXISTS == GetLastError()){
            
            for (auto i = 0; i < m_sensor_devices.size(); i++) {
                if (m_sensor_devices[i]->get_sensor_used()) {
                    m_sensor_devices[i]->set_configuration(m_app_params);
                    m_sensor_devices[i]->set_output_file(output_folder_path);
                    m_sensor_devices[i]->connect_device();
                }
            }
            
        } else {
            QString title = "Unable to create output folder";
            QString message = "The application failed to create the output folder for data files.";
            display_warning_message(title, message);
        }
     
    } else {
        QString title = "File paths not defined";
        QString message = "Paths to the config and output files must be defined.";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_start_acquisition_button_clicked() {
    
    // making sure the device isnt already streaming
    if (!m_stream_is_active) {

        // making sure devices are ready for acquisition (synchronisation)
        bool devices_ready = true;
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used()) {
                devices_ready = m_sensor_devices[i]->get_connection_status();
                if (!devices_ready) break;
            }
        }
        
        // if all used devices are ready, start the acquisition
        if (devices_ready) {

            for (auto i = 0; i < m_sensor_devices.size(); i++) {
                if (m_sensor_devices[i]->get_sensor_used()) 
                    m_sensor_devices[i]->start_stream();
            }

            m_stream_is_active = true;
            set_acquisition_label(true);

        } else {
            QString title = "Acquisition can not be started";
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
       
        // stoping the sensor streams
        for (auto i = 0; i < m_sensor_devices.size(); i++) {
            if (m_sensor_devices[i]->get_sensor_used())
                m_sensor_devices[i]->stop_stream();
        }
        
        // updating app state
        m_stream_is_active = false;
        set_acquisition_label(false);

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

        // placing the gaze crosshair back to the center
        int x_pos = PROBE_DISPLAY_X_OFFSET + (PROBE_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
        int y_pos = PROBE_DISPLAY_Y_OFFSET + (PROBE_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
        m_eyetracker_crosshair_p->setPos(x_pos, y_pos);

    } else {
        QString title = "Stream can not be stoped";
        QString message = "The data stream is not currently active.";
        display_warning_message(title, message);
    }
    
}

void SonoAssist::on_param_file_input_textChanged(const QString& text){
    
    if (!text.isEmpty()) {

        // loading the configuration
        m_config_is_loaded = load_config_file(text);
        
        // loading config dependant components
        if (m_config_is_loaded) {

            // initializing the gyroroscope client
            if (QString((*m_app_params)["ext_imu_active"].c_str()) == "true") {
                m_metawear_client_p->set_sensor_used(true);
                connect(m_metawear_client_p.get(), &MetaWearBluetoothClient::device_status_change,
                    this, &SonoAssist::on_ext_imu_status_change);
            }

            // initializing the eye tracker client
            if (QString((*m_app_params)["eye_tracker_active"].c_str()) == "true") {
                m_gaze_tracker_client_p->set_sensor_used(true);
                connect(m_gaze_tracker_client_p.get(), &GazeTracker::device_status_change,
                    this, &SonoAssist::on_eye_tracker_status_change);
                connect(m_gaze_tracker_client_p.get(), &GazeTracker::new_gaze_point,
                    this, &SonoAssist::on_new_gaze_point);
            }

            // initializing the camera client
            if (QString((*m_app_params)["rgb_camera_active"].c_str()) == "true") {
                m_camera_client_p->set_sensor_used(true);
                connect(m_camera_client_p.get(), &RGBDCameraClient::device_status_change,
                    this, &SonoAssist::on_rgbd_camera_status_change);
                connect(m_camera_client_p.get(), &RGBDCameraClient::new_video_frame,
                    this, &SonoAssist::on_new_camera_image);
            }

            // initializing the US window capture client
            if (QString((*m_app_params)["screen_recorder_active"].c_str()) == "true") {
                m_screen_recorder_client_p->set_sensor_used(true);
                connect(m_screen_recorder_client_p.get(), &ScreenRecorder::device_status_change,
                    this, &SonoAssist::on_screen_recorder_status_change);
                connect(m_screen_recorder_client_p.get(), &ScreenRecorder::new_window_capture,
                    this, &SonoAssist::on_new_us_screen_capture);
            }

            // initializing the US probe client
            if (QString((*m_app_params)["us_probe_active"].c_str()) == "true") {
                m_us_probe_client_p->set_sensor_used(true);
                connect(m_us_probe_client_p.get(), &ScreenRecorder::device_status_change,
                    this, &SonoAssist::on_us_probe_status_change);
            }

            // loading the eyetracking crosshair
            QPixmap crosshair_img(QString((*m_app_params)["eye_tracker_crosshairs_path"].c_str()));
            crosshair_img = crosshair_img.scaled(EYETRACKER_CROSSHAIRS_WIDTH, EYETRACKER_CROSSHAIRS_HEIGHT, Qt::KeepAspectRatio);
            m_eyetracker_crosshair_p = std::make_unique<QGraphicsPixmapItem>(crosshair_img);
            
            // placing the crosshair in the middle of the display
            int x_pos = PROBE_DISPLAY_X_OFFSET + (PROBE_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
            int y_pos = PROBE_DISPLAY_Y_OFFSET + (PROBE_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
            m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
            m_eyetracker_crosshair_p->setZValue(3);
            m_main_scene_p->addItem(m_eyetracker_crosshair_p.get());
           
        }

    }

}

void SonoAssist::on_output_folder_input_textChanged(const QString& text) {
    if(!text.isEmpty()) {
        m_output_is_loaded = true;
    }
}

void SonoAssist::on_param_file_browse_clicked(){

    QString new_path = QFileDialog::getOpenFileName(this, "Select parameter file", QString(), ".XML files (*.xml)");
    
    // if user does not make a selection, dont override
    if (!new_path.isEmpty()) {
        ui.param_file_input->setText("");
        ui.param_file_input->setText(new_path);
    }

}

void SonoAssist::on_output_folder_browse_clicked(void) {
   
    QString new_path = QFileDialog::getSaveFileName(this, "Select output file path", QString());

    // if user does not make a selection, dont override
    if (!new_path.isEmpty()) {
        ui.output_folder_input->setText(new_path);
    }

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


///////////////////////////////////////////////////////////////////////////////////////////////////////////// custom event handler

/// handles custom events posted by listener api callbacks (Clarius)
bool SonoAssist::event(QEvent* event){ 

    if (event->type() == IMAGE_EVENT) {
        auto evt = static_cast<event::Image*>(event);
        //newProcessedImage(evt->data(), evt->width(), evt->height(), evt->bpp());
        qDebug() << "\n\n IMAGE EVENT \n\n";
        return true;
    }
    else if (event->type() == PRESCAN_EVENT) {
        auto evt = static_cast<event::PreScanImage*>(event);
        //newRawImage(evt->data(), evt->width(), evt->height(), evt->bpp(), evt->jpeg());
        qDebug() << "\n\n PRESCAN EVENT \n\n";
        return true;
    }
    else if (event->type() == FREEZE_EVENT) {
        //setFreeze((static_cast<event::Freeze*>(event))->frozen());
        qDebug() << "\n\n FREEZE EVENT \n\n";
        return true;
    }
    else if (event->type() == BUTTON_EVENT) {
        auto evt = static_cast<event::Button*>(event);
        //onButton(evt->button(), evt->clicks());
        qDebug() << "\n\n BUTTON EVENT \n\n";
        return true;
    }
    else if (event->type() == PROGRESS_EVENT) {
        //setProgress((static_cast<event::Progress*>(event))->progress());
        qDebug() << "\n\n PROGRESS EVENT \n\n";
        return true;
    }
    else if (event->type() == RAWDATA_EVENT){
        //rawDataReady((static_cast<event::RawData*>(event))->success());
        qDebug() << "\n\n RAWDATA EVENT \n\n";
        return true;
    }
    else if (event->type() == ERROR_EVENT){
        //setError((static_cast<event::Error*>(event))->error());
        qDebug() << "\n\n ERROR EVENT \n\n";
        return true;
    }

    // letting the event pass through
    return QMainWindow::event(event);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////// utility / graphic functions

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

void SonoAssist::display_warning_message(QString title, QString message){
    QMessageBox::warning(this, title, message);
}
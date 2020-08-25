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
    m_eye_tracker_image_p = std::make_unique<QPixmap>(PROBE_DISPLAY_WIDTH, PROBE_DISPLAY_HEIGHT);
    m_eye_tracker_image_p->fill(QColor("#FFFFFF"));
    m_eye_tracker_bg_p = std::make_unique<QGraphicsPixmapItem>(*(m_eye_tracker_image_p.get()));
    m_main_scene_p->addItem(m_eye_tracker_bg_p.get());
    m_eye_tracker_bg_p->setPos(PROBE_DISPLAY_X_OFFSET, PROBE_DISPLAY_Y_OFFSET);
    m_eye_tracker_bg_p->setZValue(1);
 
	// predefining the parameters in the config file
    m_app_params = std::make_shared<config_map>();
    *m_app_params = {{"gyroscope_ble_address", ""}, {"gyroscope_to_redis", ""}, 
                     {"gyroscope_redis_entry", ""}, {"gyroscope_redis_rate_div", ""}, 
                     {"eye_tracker_to_redis", ""},  {"eye_tracker_redis_rate_div", ""},
                     {"eye_tracker_redis_entry", ""},  {"eye_tracker_crosshairs_path", ""}, {"us_window_name", ""},
                     {"camera_active", ""}, {"gyroscope_active" , ""}, {"eye_tracker_active", ""}, {"us_window_capture_active", ""} };

}

SonoAssist::~SonoAssist(){

    // making sur the streams are shut off
    try {

        if (m_camera_client_p->get_stream_status()) m_camera_client_p->stop_stream();
        if (m_tracker_client_p->get_stream_status()) m_tracker_client_p->stop_stream();
        if (m_metawear_client_p->get_stream_status()) m_metawear_client_p->stop_stream();
        if (m_us_window_client_p->get_stream_status()) m_us_window_client_p->stop_stream();

    } catch (...) {
        qDebug() << "n\SonoAssist -failed to stop the devices from treaming (in destructor)";
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
    m_camera_pixmap_p->setZValue(2);
    m_camera_pixmap_p->setPos(CAMERA_DISPLAY_X_OFFSET, CAMERA_DISPLAY_Y_OFFSET);
    m_main_scene_p->addItem(m_camera_pixmap_p.get());

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

void SonoAssist::on_sensor_connect_button_clicked(){

    // getting the output folder path
    std::string outout_folder_path = ui.output_folder_input->text().toStdString();

    // connecting to the gyro device once requirements are filled
    if (m_config_is_loaded && m_output_is_loaded) {
      
        // creating the output folder (for data files)
        if (CreateDirectory(outout_folder_path.c_str(), NULL) ||
            ERROR_ALREADY_EXISTS == GetLastError()){
            
            // connecting the eye tracker client
            if (m_tracker_client_p->get_sensor_used()) {
                m_tracker_client_p->set_configuration(m_app_params);
                m_tracker_client_p->set_output_file(outout_folder_path);
                m_tracker_client_p->connect_device();
            }
            
            // connecting the camera client
            if (m_camera_client_p->get_sensor_used()) {
                m_camera_client_p->set_configuration(m_app_params);
                m_camera_client_p->set_output_file(outout_folder_path);
                m_camera_client_p->connect_device();
            }

            // connecting the gyroscope client
            if (m_metawear_client_p->get_sensor_used()) {
                m_metawear_client_p->set_configuration(m_app_params);
                m_metawear_client_p->set_output_file(outout_folder_path);
                m_metawear_client_p->connect_device();
            }

            // connecting the US screen capture client
            if (m_us_window_client_p->get_sensor_used()) {
                m_us_window_client_p->set_configuration(m_app_params);
                m_us_window_client_p->connect_device();
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
        std::vector<std::shared_ptr<SensorDevice>> sensor_devices = {m_metawear_client_p, m_camera_client_p, m_tracker_client_p, m_us_window_client_p};
        for (auto i = 0; i < sensor_devices.size(); i++) {
            if (sensor_devices[i]->get_sensor_used()) {
                devices_ready = sensor_devices[i]->get_connection_status();
                if (!devices_ready) break;
            }
        }
        
        if (devices_ready) {
            
            if (m_camera_client_p->get_sensor_used()) m_camera_client_p->start_stream();
            if (m_tracker_client_p->get_sensor_used()) m_tracker_client_p->start_stream();
            if (m_metawear_client_p->get_sensor_used()) m_metawear_client_p->start_stream();
            if (m_us_window_client_p->get_sensor_used()) m_us_window_client_p->start_stream();

            m_stream_is_active = true;
            set_acquisition_label(true);
        
        } else {
            QString title = "Acquisition can not be started";
            QString message = "The following devices are not ready for acquisition : [";
            if (!m_metawear_client_p->get_connection_status()) message += "MetaMotionC (gyroscope), ";
            if (!m_tracker_client_p->get_connection_status()) message += "Tobii 4C (eye tracker), ";
            if (!m_camera_client_p->get_connection_status()) message += "Intel Realsens camera (RGBD), ";
            if (!m_us_window_client_p->get_connection_status()) message += "US window capture ";
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
        if (m_camera_client_p->get_sensor_used()) m_camera_client_p->stop_stream();
        if (m_tracker_client_p->get_sensor_used()) m_tracker_client_p->stop_stream();
        if (m_metawear_client_p->get_sensor_used()) m_metawear_client_p->stop_stream();
        if (m_us_window_client_p->get_sensor_used()) m_us_window_client_p->stop_stream();
        
        // updating app state
        m_stream_is_active = false;
        set_acquisition_label(false);

        // updating the UI
        if (m_camera_pixmap_p.get() != nullptr) {
            m_main_scene_p->removeItem(m_camera_pixmap_p.get());
            m_camera_pixmap_p.reset();
        }
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
            m_metawear_client_p = std::make_shared<MetaWearBluetoothClient>();
            if (QString((*m_app_params)["gyroscope_active"].c_str()) == "true") {
                m_metawear_client_p->set_sensor_used(true);
                connect(m_metawear_client_p.get(), &MetaWearBluetoothClient::device_status_change,
                    this, &SonoAssist::on_gyro_status_change);
            }

            // initializing the eye tracker client
            m_tracker_client_p = std::make_shared<GazeTracker>();
            if (QString((*m_app_params)["eye_tracker_active"].c_str()) == "true") {
                m_tracker_client_p->set_sensor_used(true);
                connect(m_tracker_client_p.get(), &GazeTracker::device_status_change,
                    this, &SonoAssist::on_eye_tracker_status_change);
                connect(m_tracker_client_p.get(), &GazeTracker::new_gaze_point,
                    this, &SonoAssist::on_new_gaze_point);
            }

            // initializing the camera client
            m_camera_client_p = std::make_shared<RGBDCameraClient>();
            if (QString((*m_app_params)["camera_active"].c_str()) == "true") {
                m_camera_client_p->set_sensor_used(true);
                connect(m_camera_client_p.get(), &RGBDCameraClient::device_status_change,
                    this, &SonoAssist::on_camera_status_change);
                connect(m_camera_client_p.get(), &RGBDCameraClient::new_video_frame,
                    this, &SonoAssist::on_new_camera_image);
            }

            // initializing the US window capture client
            m_us_window_client_p = std::make_shared<WindowPainter>();
            if (QString((*m_app_params)["us_window_capture_active"].c_str()) == "true") {
                m_us_window_client_p->set_sensor_used(true);
                connect(m_us_window_client_p.get(), &WindowPainter::device_status_change,
                    this, &SonoAssist::on_us_window_status_change);
            }

            // loading the eyetracking crosshair
            QPixmap crosshair_img(QString((*m_app_params)["eye_tracker_crosshairs_path"].c_str()));
            crosshair_img = crosshair_img.scaled(EYETRACKER_CROSSHAIRS_WIDTH, EYETRACKER_CROSSHAIRS_HEIGHT, Qt::KeepAspectRatio);
            m_eyetracker_crosshair_p = std::make_unique<QGraphicsPixmapItem>(crosshair_img);
            
            // placing the crosshair in the middle of the display
            int x_pos = PROBE_DISPLAY_X_OFFSET + (PROBE_DISPLAY_WIDTH / 2) - (EYETRACKER_CROSSHAIRS_WIDTH / 2);
            int y_pos = PROBE_DISPLAY_Y_OFFSET + (PROBE_DISPLAY_HEIGHT / 2) - (EYETRACKER_CROSSHAIRS_HEIGHT / 2);
            m_main_scene_p->addItem(m_eyetracker_crosshair_p.get());
            m_eyetracker_crosshair_p->setZValue(3);
            m_eyetracker_crosshair_p->setPos(x_pos, y_pos);
           
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

void SonoAssist::on_gyro_status_change(bool device_status){
    set_device_status(device_status, GYROSCOPE);
}

void SonoAssist::on_eye_tracker_status_change(bool device_status) {
    set_device_status(device_status, EYETRACKER);
}

void SonoAssist::on_camera_status_change(bool device_status) {
    set_device_status(device_status, CAMERA);
}

void SonoAssist::on_us_window_status_change(bool device_status) {
    set_device_status(device_status, US_WINDOW);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// utility / graphic functions

void SonoAssist::build_sensor_panel(void) {

    int index;

    // defining table dimensions and headers
    ui.sensor_status_table->setRowCount(4);
    ui.sensor_status_table->setColumnCount(1);
    ui.sensor_status_table->setHorizontalHeaderLabels(QStringList{"Sensor status"});
    ui.sensor_status_table->setVerticalHeaderLabels(QStringList{"Gyroscope", "Eye tracker", "Camera", "US Capture"});

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
    set_device_status(false, CAMERA);
    set_device_status(false, US_WINDOW);
    set_device_status(false, GYROSCOPE);
    set_device_status(false, EYETRACKER);

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
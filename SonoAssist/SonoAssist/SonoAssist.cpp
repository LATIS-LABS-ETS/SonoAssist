#include "SonoAssist.h"

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

	// setting up the gui
	ui.setupUi(this);
    build_sensor_panel();
    set_acquisition_label(false);
   
    // setting the scene widget
    m_main_scene_p = std::make_unique<QGraphicsScene>(ui.graphicsView);
    ui.graphicsView->setScene(m_main_scene_p.get());
    ui.graphicsView->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	// predefining the parameters in the config file
    m_app_params = std::make_shared<config_map>();
    *m_app_params = {{"gyroscope_ble_address", ""}, {"gyroscope_to_redis", ""}, 
                     {"gyroscope_redis_entry", ""}, {"gyroscope_redis_rate_div", ""}, 
                     {"eye_tracker_to_redis", ""},  {"eye_tracker_redis_rate_div", ""},
                     {"eye_tracker_redis_entry", ""}};

	// initialising the gyroroscope client
	m_metawear_client_p = std::make_shared<MetaWearBluetoothClient>();
    connect(m_metawear_client_p.get(), &MetaWearBluetoothClient::device_status_change,
        this, &SonoAssist::on_gyro_status_change);

    // initializing the eye tracker client
    m_tracker_client_p = std::make_shared<GazeTracker>();
    connect(m_tracker_client_p.get(), &GazeTracker::device_status_change, 
        this, &SonoAssist::on_eye_tracker_status_change);

    // initializing the camera client
    m_camera_client_p = std::make_shared<RGBDCameraClient>();
    connect(m_camera_client_p.get(), &RGBDCameraClient::device_status_change, 
        this, &SonoAssist::on_camera_status_change);
    connect(m_camera_client_p.get(), &RGBDCameraClient::new_video_frame,
        this, &SonoAssist::on_new_camera_image);

}

SonoAssist::~SonoAssist(){

    // making sur the streams are shut off
    try {

        if (m_camera_client_p->get_stream_status()) m_camera_client_p->stop_stream();
        if (m_tracker_client_p->get_stream_status()) m_tracker_client_p->stop_stream();
        if (m_metawear_client_p->get_stream_status()) m_metawear_client_p->stop_stream();

    } catch (...) {
        qDebug() << "n\SonoAssist -failed to stop the devices from treaming (in destructor)";
    }
 
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// slots

void SonoAssist::on_new_camera_image(QImage new_image) {
    // updating the image on the main scene
    m_main_scene_p->clear();
    m_main_scene_p->addItem(new QGraphicsPixmapItem(QPixmap::fromImage(new_image)));
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
            m_tracker_client_p->set_configuration(m_app_params);
            m_tracker_client_p->set_output_file(outout_folder_path);
            m_tracker_client_p->connect_device();

            // connecting the camera client
            m_camera_client_p->set_configuration(m_app_params);
            m_camera_client_p->set_output_file(outout_folder_path);
            m_camera_client_p->connect_device();

            // connecting the gyroscope client
            m_metawear_client_p->set_configuration(m_app_params);
            m_metawear_client_p->set_output_file(outout_folder_path);
            m_metawear_client_p->connect_device();
            
        } 

        // displaying warning message (folder creation)
        else {
            QString title = "Unable to create output folder";
            QString message = "The application failed to create the output folder for data files.";
            display_warning_message(title, message);
        }
     
    } 
    
    // displaying warning message (file paths)
    else {
        QString title = "File paths not defined";
        QString message = "Paths to the config and output files must be defined.";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_start_acquisition_button_clicked() {
    
    // making sure the device isnt already streaming
    if (!m_stream_is_active) {

        // making sure devices are ready for acquisition (synchronisation)
        if (m_metawear_client_p->get_connection_status() && m_tracker_client_p->get_connection_status()
            && m_camera_client_p->get_connection_status()) {
            
            m_camera_client_p->start_stream();
            m_tracker_client_p->start_stream();
            m_metawear_client_p->start_stream();

            m_stream_is_active = true;
            set_acquisition_label(true);
        
        }

        // displaying warning message
        else {
            QString title = "Acquisition can not be started";
            QString message = "The following devices are not ready for acquisition : [";
            if (!m_metawear_client_p->get_connection_status()) message += "MetaMotionC (gyroscope) ,";
            if (!m_tracker_client_p->get_connection_status()) message += "Tobii 4C (eye tracker) ,";
            if (!m_camera_client_p->get_connection_status()) message += "Intel Realsens camera (RGBD) ";
            message += "].";
            display_warning_message(title, message);
        }

    }

    // displaying warning message
    else {
        QString title = "Stream can not be started";
        QString message = "Data streaming is already in progress. End current stream.";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_stop_acquisition_button_clicked() {
    
    // stopping the stream
    if(m_stream_is_active) {
       
        // stoping the sensor streams
        m_camera_client_p->stop_stream();
        m_tracker_client_p->stop_stream();
        m_metawear_client_p->stop_stream();
        
        // updating state and UI
        m_stream_is_active = false;
        m_main_scene_p->clear();
        set_acquisition_label(false);

    } 
    
    // displaying warning message
    else {
        QString title = "Stream can not be stoped";
        QString message = "The data stream is not currently active.";
        display_warning_message(title, message);
    }
    
}

void SonoAssist::on_param_file_input_textChanged(const QString& text){
    if (!text.isEmpty()) {
        m_config_is_loaded = load_config_file(text);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////// utility / graphic functions

void SonoAssist::build_sensor_panel(void) {

    int index;

    // defining table dimensions and headers
    ui.sensor_status_table->setRowCount(3);
    ui.sensor_status_table->setColumnCount(1);
    ui.sensor_status_table->setHorizontalHeaderLabels(QStringList{"Sensor status"});
    ui.sensor_status_table->setVerticalHeaderLabels(QStringList{"Gyroscope", "Eye tracker", "Camera"});

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
    set_device_status(false, GYROSCOPE);
    set_device_status(false, EYETRACKER);
    set_device_status(false, CAMERA);

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
        color_str = "red";
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
        style_sheet = "QLabel {color : red;}";
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
#include "SonoAssist.h"

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

	// setting up the gui
	ui.setupUi(this);
    set_acquisition_label(false);
    on_gyro_status_change(false);
    on_camera_status_change(false);
    on_eye_tracker_status_change(false);
   
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

void SonoAssist::on_sensor_connect_button_clicked(){

    // connecting to the gyro device once requirements are filled
    if (m_config_is_loaded && m_output_is_loaded) {
      
        // connecting the eye tracker client
        m_tracker_client_p->set_configuration(m_app_params);
        m_tracker_client_p->set_output_file(ui.output_file_input->text().toStdString(), MAIN_OUTPUT_EXTENSION);
        m_tracker_client_p->connect_device();

        // connecting the camera client
        m_camera_client_p->set_configuration(m_app_params);
        m_camera_client_p->set_output_file(ui.output_file_input->text().toStdString(), MAIN_OUTPUT_EXTENSION);
        m_camera_client_p->connect_device();

        // connecting the gyroscope client
        m_metawear_client_p->set_configuration(m_app_params);
        m_metawear_client_p->set_output_file(ui.output_file_input->text().toStdString(), MAIN_OUTPUT_EXTENSION);
        m_metawear_client_p->connect_device();
      
    } 
    
    // displaying warning message
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
       
        m_camera_client_p->stop_stream();
        m_tracker_client_p->stop_stream();
        m_metawear_client_p->stop_stream();
        
        m_stream_is_active = false;
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

void SonoAssist::on_output_file_input_textChanged(const QString& text) {
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

void SonoAssist::on_output_file_browse_clicked(void) {
   
    QString target_extension;
    target_extension += (QString(MAIN_OUTPUT_EXTENSION) + " files (*" + QString(MAIN_OUTPUT_EXTENSION) + ")");
    QString new_path = QFileDialog::getSaveFileName(this, "Select output file path", QString(), target_extension);

    // if user does not make a selection, dont override
    if (!new_path.isEmpty()) {
        ui.output_file_input->setText(new_path);
    }

}

void SonoAssist::on_gyro_status_change(bool device_status) {

    // updating the gyroscope label
    QString style_sheet;
    QString gyro_label = "gyroscope status : ";
    if(device_status) {
        gyro_label += "(connected)";
        style_sheet = QString("QLabel {color : %1;}").arg(QString(GREEN_TEXT));
    } else {
        gyro_label += "(disconnected)";
        style_sheet = "QLabel {color : red;}";
    }
    ui.gyro_status_label->setText(gyro_label);
    ui.gyro_status_label->setStyleSheet(style_sheet);

}

void SonoAssist::on_eye_tracker_status_change(bool device_status) {

    // updating the gyroscope label
    QString style_sheet;
    QString tracker_label = "eye tracker status : ";
    if (device_status) {
        tracker_label += "(connected)";
        style_sheet = QString("QLabel {color : %1;}").arg(QString(GREEN_TEXT));
    }
    else {
        tracker_label += "(disconnected)";
        style_sheet = "QLabel {color : red;}";
    }
    ui.eye_tracker_status_label->setText(tracker_label);
    ui.eye_tracker_status_label->setStyleSheet(style_sheet);

}

void SonoAssist::on_camera_status_change(bool device_status) {

    // updating the gyroscope label
    QString style_sheet;
    QString tracker_label = "camera status : ";
    if (device_status) {
        tracker_label += "(connected)";
        style_sheet = QString("QLabel {color : %1;}").arg(QString(GREEN_TEXT));
    }
    else {
        tracker_label += "(disconnected)";
        style_sheet = "QLabel {color : red;}";
    }
    ui.camera_status_label->setText(tracker_label);
    ui.camera_status_label->setStyleSheet(style_sheet);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// utility functions

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


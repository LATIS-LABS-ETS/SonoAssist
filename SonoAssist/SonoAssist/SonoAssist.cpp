#include "SonoAssist.h"

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

	// setting up the gui
	ui.setupUi(this);
    on_gyro_status_change(false);

	// predefining the parameters in the config file
    m_app_params = std::make_shared<config_map>();
    *m_app_params = {{"gyroscope_ble_address", ""}, {"gyroscope_to_redis", ""}, 
                     {"gyroscope_redis_entry", ""}, {"gyroscope_redis_rate_div", ""}};

	// initialising the bluetooth interface
	m_metawear_client_p = std::make_shared<MetaWearBluetoothClient>();
    connect(m_metawear_client_p.get(), &MetaWearBluetoothClient::device_status_change,
        this, &SonoAssist::on_gyro_status_change);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// slots

void SonoAssist::on_gyro_connect_button_clicked(){

    // connecting to the gyro device once requirements are filled
    if (m_config_is_loaded && m_output_is_loaded) {
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
        if (m_metawear_client_p->get_connection_status()) {
            m_stream_is_active = true;
            set_acquisition_label(m_stream_is_active);
            m_metawear_client_p->start_stream();
        }

        // displaying warning message
        else {
            QString title = "Acquisition can not be started";
            QString message = "The following devices are not ready for acquisition : [";
            if (!m_metawear_client_p->get_connection_status()) message += "MetaMotionC (gyroscope) ,";
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
        m_stream_is_active = false;
        set_acquisition_label(false);
        m_metawear_client_p->stop_stream();
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

void SonoAssist::on_gyro_status_change(bool device_satus) {

    // updating the gyroscope label
    QString gyro_label = "gyroscope status : ";
    if(device_satus) {
        gyro_label += "connected";
    } else {
        gyro_label += "disconnected";
    }
    ui.gyro_status_label->setText(gyro_label);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// utility functions

void SonoAssist::set_acquisition_label(bool active) {

    QString label_text = "acquisition status : ";
    if(active) {
        label_text += "in progress";
    } else {
        label_text += "inactive";
    }
    ui.acquisition_label->setText(label_text);

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


#include "SonoAssist.h"

SonoAssist::SonoAssist(QWidget *parent) : QMainWindow(parent){

	// setting up the gui
	ui.setupUi(this);
    on_gyro_status_change(false);

	// predefining the parameters in the config file
    m_app_params = std::make_shared<config_map>();
    *m_app_params = { {"gyroscope_ble_address", ""}, {"gyroscope_output_file", ""}, 
                      {"gyroscope_to_redis", ""} };

	// initialising the bluetooth interface
	m_metawear_client_p = std::make_shared<MetaWearBluetoothClient>();
    connect(m_metawear_client_p.get(), &MetaWearBluetoothClient::device_status_change,
        this, &SonoAssist::on_gyro_status_change);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////// slots

void SonoAssist::on_gyro_connect_button_clicked(){

    if (m_config_is_loaded) {
        m_metawear_client_p->set_configuration(m_app_params);
        m_metawear_client_p->connect_to_metawear_board();

    } else {
        QString title = "Parameter file Error";
        QString message = "Provided path to the parameter file is invalid";
        display_warning_message(title, message);
    }

}

void SonoAssist::on_start_acquisition_button_clicked() {
    
    // making sure devices are ready for acquisition (synchronisation)
    if(m_metawear_client_p->get_device_status()) {
        m_metawear_client_p->start_data_stream();
    }

}

void SonoAssist::on_stop_acquisition_button_clicked() {
    
    // making sure devices are ready for acquisition    
    m_metawear_client_p->stop_data_stream();

}

void SonoAssist::on_param_file_input_textChanged(const QString& text){
    m_config_is_loaded = load_config_file(text);
}

void SonoAssist::on_param_file_browse_clicked(){

    // saving old path string and get user chosen path
    QString oldPath = ui.param_file_input->text();
    QString new_path = QFileDialog::getOpenFileName(this, "Select parameter file", QString(), "XML files (*.xml)");
    
    // if user does not make a selection, dont override
    if (!new_path.isEmpty()) {
        ui.param_file_input->setText(new_path);
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
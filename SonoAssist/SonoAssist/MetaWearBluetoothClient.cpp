#include "MetaWearBluetoothClient.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// MblMwBtleConnection structure functions (wrappers)

void read_gatt_char_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler) {
	(static_cast<MetaWearBluetoothClient*>(context))->read_gatt_char(caller, characteristic, handler);
}

void write_gatt_char_wrap(void* context, const void* caller, MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic,
	const uint8_t* value, uint8_t length) {
	(static_cast<MetaWearBluetoothClient*>(context))->write_gatt_char(caller, writeType, characteristic, value, length);
}

void enable_notifications_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler,
	MblMwFnVoidVoidPtrInt ready) {
	(static_cast<MetaWearBluetoothClient*>(context))->enable_notifications(caller, characteristic, handler, ready);
}

void on_disconnect_wrap(void* context, const void* caller, MblMwFnVoidVoidPtrInt handler) {
	(static_cast<MetaWearBluetoothClient*>(context))->on_disconnect(caller, handler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// methods for the MblMwBtleConnection interface

void MetaWearBluetoothClient::read_gatt_char(const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler) {

	// proceed if the required characteristic is valid
	int service_index;
	QLowEnergyCharacteristic target_characteristic = find_characteristic(characteristic, service_index, "read_gatt_char");
	if (target_characteristic.isValid()) {

		// inserting the required information in the characteristic call back queue
		m_char_read_callback_queue.push(std::tuple<const void*, MblMwFnIntVoidPtrArray>(caller, handler));

		// sending the read request
		m_metawear_services_p[service_index]->readCharacteristic(target_characteristic);

	}

}

void MetaWearBluetoothClient::write_gatt_char(const void* caller, MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic,
	const uint8_t* value, uint8_t length) {

	// proceed if the required characteristic is valid
	int service_index;
	QLowEnergyCharacteristic target_characteristic = find_characteristic(characteristic, service_index, "write_gatt_char");
	if (target_characteristic.isValid()) {

		// converting the payload
		QByteArray data;
		for (auto i = 0; i < length; i++) data.append((char)value[i]);

		// converting the write type
		QLowEnergyService::WriteMode write_mode;
		if (writeType == MBL_MW_GATT_CHAR_WRITE_WITH_RESPONSE) {
			write_mode = QLowEnergyService::WriteWithResponse;
		} else {
			write_mode = QLowEnergyService::WriteWithoutResponse;
		}

		// sending the write request
		m_metawear_services_p[service_index]->writeCharacteristic(target_characteristic, data, write_mode);

	}

}

void MetaWearBluetoothClient::enable_notifications(const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler,
	MblMwFnVoidVoidPtrInt ready) {

	// proceed if the required characteristic is valid
	int service_index;
	QLowEnergyCharacteristic target_characteristic = find_characteristic(characteristic, service_index, "enable_notifications");
	if (target_characteristic.isValid()) {

		// making sure the characteristic descriptor is valid
		QLowEnergyDescriptor notification = target_characteristic.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
		if (notification.isValid()) {

			// enabling notification for the descriptor
			m_metawear_services_p[service_index]->writeDescriptor(notification, QByteArray::fromHex("0100"));

			// adding an entry in the characteristic-callback map
			m_char_update_callback_map[target_characteristic.uuid().toString()] =
				std::tuple<const void*, MblMwFnIntVoidPtrArray>(caller, handler);

		}

	}

	// notifying that the enable notify task is completed
	QThread::msleep(DESCRIPTOR_WRITE_DELAY);
	ready(caller, 0);

}

void MetaWearBluetoothClient::on_disconnect(const void* caller, MblMwFnVoidVoidPtrInt handler) {

	// setting the handler and enabling it
	m_disconnect_handler = handler;
	m_disconnect_event_caller = caller;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// MetaWearBluetoothClient public interface

MetaWearBluetoothClient::MetaWearBluetoothClient(){

	// configuring the discovery agent
	m_discovery_agent.setLowEnergyDiscoveryTimeout(DISCOVERYTIMEOUT);

	// hooking scanning events to their handler
	QObject::connect(&m_discovery_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, 
		this, &MetaWearBluetoothClient::device_discovered);
	QObject::connect(&m_discovery_agent,
		static_cast<void (QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error),
		[this](QBluetoothDeviceDiscoveryAgent::Error error) {
			qDebug() << "\nMetaWearBluetoothClient - ble device discovery level error occured, code : " << error;
		});

}

MetaWearBluetoothClient::~MetaWearBluetoothClient() {
	disconnect_device();
}

void MetaWearBluetoothClient::connect_device() {

	// making sure that requirements have been loaded
	if (m_config_loaded && m_output_file_loaded) {
		
		// disconnect the device if already connected
		if (m_device_connected) disconnect_device();
		
		// launching device discovery
		m_discovery_agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
		qDebug() << "\nMetaWearBluetoothClient - starting device scan\n";
	}

}

void MetaWearBluetoothClient::start_stream() {

	// making sure device is ready to stream
	if (m_device_connected && !m_device_streaming) {

		MblMwFnData stream_callback;

		// opening the output file
		m_output_file.open(m_output_file_str, std::fstream::app);
		
		// connecting to redis (if redis enabled)
		if ((*m_config_ptr)["gyroscope_to_redis"] == "true") {
			m_redis_entry = (*m_config_ptr)["gyroscope_redis_entry"];
			m_redis_rate_div = std::atoi((*m_config_ptr)["gyroscope_redis_rate_div"].c_str());
			connect_to_redis();
		}
			
		stream_callback = [](void* context, const MblMwData* data) {

			MetaWearBluetoothClient* context_p = static_cast<MetaWearBluetoothClient*>(context);

			// pulling orientation data
			MblMwEulerAngles* euler_angles = (MblMwEulerAngles*)data->value;
		
			// formatting the data
			std::string output_str = context_p->get_millis_timestamp() + "," + std::to_string(euler_angles->heading) + ','
				+ std::to_string(euler_angles->pitch) + "," + std::to_string(euler_angles->roll) + "," + std::to_string(euler_angles->yaw)
				+ "\n";

			// writing to the output file and redis (if redis enabled)
			context_p->write_to_redis(output_str);
			context_p->m_output_file << output_str;

		};
		
		// defining the signal data type and the associated callback
		auto euler_angles = mbl_mw_sensor_fusion_get_data_signal(m_metawear_board_p, MBL_MW_SENSOR_FUSION_DATA_EULER_ANGLE);
		mbl_mw_datasignal_subscribe(euler_angles, this, stream_callback);
			
		// hooking the callback to the data signal
		mbl_mw_sensor_fusion_enable_data(m_metawear_board_p, MBL_MW_SENSOR_FUSION_DATA_EULER_ANGLE);
		mbl_mw_sensor_fusion_start(m_metawear_board_p);

		m_device_streaming = true;
	}

}

void MetaWearBluetoothClient::stop_stream(void){

	// making sure device is streaming
	if (m_device_connected && m_device_streaming) {

		// stoping stream from board
		mbl_mw_sensor_fusion_stop(m_metawear_board_p);

		// closing the output file and redis connection
		m_output_file.close();
		if ((*m_config_ptr)["gyroscope_to_redis"] == "true") {
			disconnect_from_redis();
		}

		m_device_streaming = false;
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// MetaWearBluetoothClient private slots

void MetaWearBluetoothClient::device_discovered(const QBluetoothDeviceInfo& device) {
	
	// only interested in the target device
	QString incoming_adress_str = device.address().toString();
	QString target_device_adress((*m_config_ptr)["gyroscope_ble_address"].c_str());
	if((incoming_adress_str == target_device_adress) && !m_metawear_device_controller_p) {
		
		qDebug() << "\nMetaWearBluetoothClient - device with address : " << incoming_adress_str << "discovered\n";

		// creating a controller to interface with the device
		m_metawear_device_controller_p = std::shared_ptr<QLowEnergyController>(QLowEnergyController::createCentral(device));

		// hooking the relevant device event handlers
		
		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::serviceDiscovered,
			this, &MetaWearBluetoothClient::service_discovered);
		
		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::connected,
			[this](void) {
				m_metawear_device_controller_p->discoverServices();
			});

		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::disconnected,
			this, &MetaWearBluetoothClient::device_disconnected);
		
		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::discoveryFinished, 
			this, &MetaWearBluetoothClient::service_discovery_finished);
		
		connect(m_metawear_device_controller_p.get(), 
			static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
			[this](QLowEnergyController::Error error) {
				this->disconnect_device();
				qDebug() << "\nMetaWearBluetoothClient - ble device communication level error occured, code : " << error;
			});
	
		// connecting to the target device
		m_metawear_device_controller_p->connectToDevice();
	
	}

}

void MetaWearBluetoothClient::device_disconnected(){

	// calling the registered call back for the MblMwBtleConnection structure
	if(m_disconnect_handler != nullptr) {
		m_disconnect_handler(m_disconnect_event_caller, 0);
	}

	// moving to a disconnected device state
	if(m_device_connected) disconnect_device();
}

void MetaWearBluetoothClient::service_discovered(const QBluetoothUuid& gatt_uuid){

	// creating the service object from the uuid
	QLowEnergyService* service_p = m_metawear_device_controller_p->createServiceObject(gatt_uuid, this);
	
	// storing and configuring the object if valid
	if(service_p) {

		// service objects are stored in a shared pointer vector
		int service_index = m_metawear_services_p.size();
		m_metawear_services_p.emplace_back(std::shared_ptr<QLowEnergyService>(service_p));

		// connecting the service object to the necessary call backs
		connect(m_metawear_services_p[service_index].get(), &QLowEnergyService::characteristicRead,
			this, &MetaWearBluetoothClient::service_characteristic_read);
		connect(m_metawear_services_p[service_index].get(), &QLowEnergyService::characteristicChanged,
			this, &MetaWearBluetoothClient::service_characteristic_changed);
		connect(m_metawear_services_p[service_index].get(), &QLowEnergyService::characteristicWritten,
			this, &MetaWearBluetoothClient::service_characteristic_changed);
		connect(m_metawear_services_p[service_index].get(), 
			QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error),
			[this](QLowEnergyService::ServiceError error){
				qDebug() << "\nble service level error occured : " << error << "\n";
			});

		// discovering the characteristics associated to the service
		m_metawear_services_p[service_index]->discoverDetails();

		// debug service description
		qDebug() << "\n";
		qDebug() << "service with uuid : " << gatt_uuid.toString() << "discovered, info : ";
		qDebug() << "service state : " << m_metawear_services_p[service_index]->state();
		qDebug() << "service name : " << m_metawear_services_p[service_index]->serviceName();
		qDebug() << "\n";

	}

}

void MetaWearBluetoothClient::service_discovery_finished() {
	
	// making sure services were detected
	if (!m_metawear_services_p.empty()) {

		// waiting some time so that service details can be acquired
		QThread::msleep(DISCOVER_DETAILS_DELAY);

		// creating the object for interfacing with the metawear board
		m_metawear_ble_interface = {this, write_gatt_char_wrap, read_gatt_char_wrap, enable_notifications_wrap, on_disconnect_wrap};
		m_metawear_board_p = mbl_mw_metawearboard_create(&m_metawear_ble_interface);

		// initializing the board and defining a call back upon initialisation
		mbl_mw_metawearboard_initialize(m_metawear_board_p, this, [](void* context, MblMwMetaWearBoard* board, int32_t status) -> void {
			
			// configuring the device output stream
			mbl_mw_sensor_fusion_set_mode(board, MBL_MW_SENSOR_FUSION_MODE_NDOF);
			mbl_mw_sensor_fusion_set_acc_range(board, MBL_MW_SENSOR_FUSION_ACC_RANGE_8G);
			mbl_mw_sensor_fusion_write_config(board);
		
			// notifying the main window
			(static_cast<MetaWearBluetoothClient*>(context))->set_connection_status(true);
			emit(static_cast<MetaWearBluetoothClient*>(context))->device_status_change(true);
		});
	
	}
}

void MetaWearBluetoothClient::service_characteristic_read(const QLowEnergyCharacteristic& descriptor, const QByteArray& value) {
	
	// making sure request information is available
	if(!m_char_read_callback_queue.empty()) {
	
		// getting the callback and context for the corresponding (oldest) read request
		std::tuple<const void*, MblMwFnIntVoidPtrArray> info_tuple = m_char_read_callback_queue.front();
		m_char_read_callback_queue.pop();

		// calling the associated call back function
		std::get<1>(info_tuple)(
			std::get<0>(info_tuple), 
			(uint8_t*) value.data(),
			(uint8_t) value.length()
		);
			
	}
	
}

void MetaWearBluetoothClient::service_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue) {

	// checking if the changed characteristic is registered to a call back
	auto callback_map_entry = m_char_update_callback_map.find(characteristic.uuid().toString());
	if (callback_map_entry != m_char_update_callback_map.end()) {
	
		// converting the service characteristic to metawear format
		const char* byte_array_data = newValue.data();
		uint8_t* converted_data = new uint8_t[newValue.length()];
		for (auto i = 0; i < newValue.length(); i++)
			converted_data[i] = (uint8_t) byte_array_data[i];
		
		// calling the proper call back (specified in the map)
		std::get<1>(callback_map_entry->second)(
			std::get<0>(callback_map_entry->second),
			converted_data,
			newValue.length()
		);
			
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// setters and getters

void MetaWearBluetoothClient::set_output_file(std::string output_file_path, std::string extension){
	
	try {

		// defining the output file path
		auto extension_pos = output_file_path.find(extension);
		m_output_file_str = output_file_path.replace(extension_pos, extension.length(), "_gyro.csv");
		if (m_output_file.is_open()) m_output_file.close();

		// writing the output file header
		m_output_file.open(m_output_file_str);
		m_output_file << "Time" << "," << "Heading" << "," << "Pitch" << "," << "Roll" << "," << "Yaw" << std::endl;
		m_output_file.close();
		m_output_file_loaded = true;

	} catch(...) {
		qDebug() << "\nMetaWearBluetoothClient - error occured while setting the output file";
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// conveniance functions

void MetaWearBluetoothClient::disconnect_device() {

	// stopping the data stream
	stop_stream();
	m_device_connected = false;
	if (m_output_file.is_open()) m_output_file.close();

	// clearing the metawear related vars
	mbl_mw_metawearboard_free(m_metawear_board_p);
	m_metawear_ble_interface = {0};

	// clearing qt communication vars
	m_metawear_services_p.clear();
	m_metawear_device_controller_p.reset();
	
	// clearing callback structures / vars
	m_char_update_callback_map.clear();
	bytes_callback_queue empty_queue;
	std::swap(m_char_read_callback_queue, empty_queue);
	m_disconnect_event_caller = nullptr;
	m_disconnect_handler = nullptr;

	// changing the device state
	emit device_status_change(false);

}

 QLowEnergyCharacteristic MetaWearBluetoothClient::find_characteristic(const MblMwGattChar* characteristic_struct, int& service_index, QString debug_str) const {

	// getting the uuid object for the target characteristic and the target service
	QBluetoothUuid target_characteristic_uuid = metawear_uuid_to_qt_uuid(characteristic_struct->uuid_low, characteristic_struct->uuid_high);
	QBluetoothUuid target_service_uuid = metawear_uuid_to_qt_uuid(characteristic_struct->service_uuid_low, characteristic_struct->service_uuid_high);

	// looking through the services for the specified characteristic
	service_index = -1;
	QLowEnergyCharacteristic characteristic;
	for (auto i = 0; i < m_metawear_services_p.size(); i++) {
		
		characteristic = m_metawear_services_p[i]->characteristic(target_characteristic_uuid);
		
		if (characteristic.isValid()) {
			service_index = i;
			break;
		}
	}

	// debug information about requested service and characteristic
	qDebug() << "\n ........................... find_characteristic call (begin)";
	qDebug() << "target service uuid : " << target_service_uuid.toString();
	if (characteristic.isValid()) qDebug() << "target service name : " << m_metawear_services_p[service_index]->serviceName();
	qDebug() << "target characteristic uuid : " << target_characteristic_uuid.toString();
	if (characteristic.isValid()) qDebug() << "target characteristic name : " << characteristic.name();
	qDebug() << "requests in the callback queue : " << m_char_read_callback_queue.size();
	qDebug() << "debug string : " << debug_str;
	qDebug() << "........................... find_characteristic call (end)\n";

	return characteristic;

}

 QBluetoothUuid MetaWearBluetoothClient::metawear_uuid_to_qt_uuid(const uint64_t uuid_low, const uint64_t uuid_high) const {

	 // getting a 128 bit representation of the characteristic uuid
	 uint8_t source_uuid[16];
	 quint128 converted_uuid;
	 std::memcpy(source_uuid, &uuid_low, 8);
	 std::memcpy(&(source_uuid[8]), &uuid_high, 8);
	 for (auto i = 0; i <= 15; i++) {
		 converted_uuid.data[i] = (quint8)source_uuid[15 - i];
	 }

	 // returning the coresponding UUID
	 return QBluetoothUuid(converted_uuid);
 
 }
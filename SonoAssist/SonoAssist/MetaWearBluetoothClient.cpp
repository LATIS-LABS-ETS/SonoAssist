#include "MetaWearBluetoothClient.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// MblMwBtleConnection structure functions (wrappers)

void read_gatt_char_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler) {
	(static_cast<MetaWearBluetoothClient*>(context))->read_gatt_char(caller, characteristic, handler);
}

void write_gatt_char_wrap(void* context, const void* caller, MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic,
	const uint8_t* value, uint8_t length) {
	(static_cast<MetaWearBluetoothClient*>(context))->write_gatt_char(writeType, characteristic, value, length);
}

void enable_notifications_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler,
	MblMwFnVoidVoidPtrInt ready) {
	(static_cast<MetaWearBluetoothClient*>(context))->enable_notifications(caller, characteristic, handler, ready);
}

void on_disconnect_wrap(void* context, const void* caller, MblMwFnVoidVoidPtrInt handler) {
	(static_cast<MetaWearBluetoothClient*>(context))->on_disconnect(caller, handler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// methods for the MblMwBtleConnection interface

/*
* Reads the specified BLE characteristic from the currently connected device.
* The function does nothing if no device is not connected or if the provided characteristic is invalid.
* 
* @param [in] caller Voided context variable to pass in to the call back function when the requested char data is recevied.
* @param [in] characteristic UUIDs specifying the characteristic to be read (service and char uuids).
* @param [in] handler Callback function to call when the characteristic data is received. 
*/
void MetaWearBluetoothClient::read_gatt_char(const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler) {

	int service_index;

	// proceed if the required characteristic is valid
	QLowEnergyCharacteristic target_characteristic = find_characteristic(characteristic, service_index, "read_gatt_char");
	if (target_characteristic.isValid()) {

		// inserting the required information in the characteristic call back queue
		m_char_read_callback_queue.push(std::tuple<const void*, MblMwFnIntVoidPtrArray>(caller, handler));

		// sending the read request
		m_metawear_services_p[service_index]->readCharacteristic(target_characteristic);

	}

}

/*
* Writes the specified data to the target BLE characteristic.
* The function does nothing if no device is not connected or if the provided characteristic is invalid.
* 
* @param [in] writeType Specifies if the write operation should prompt an update (not really usefull).
* @param [in] characteristic UUIDs specifying the characteristic to be writen to (service and char uuids).
* @param [in] value payload to be sent to the target characteristic 
* @param [in] length length of the value array 
*/
void MetaWearBluetoothClient::write_gatt_char(MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic,
	const uint8_t* value, uint8_t length) {

	int service_index;

	// proceed if the required characteristic is valid
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

/*
* Turns on value change notifications for the specified characteristic. 
* In the future, when the characteristic the specified handler will be called.
* The function does nothing if no device is not connected or if the provided characteristic is invalid.
*
* @param [in] caller Voided context variable to pass in to the call back function when the requested char data is recevied.
* @param [in] characteristic UUIDs specifying the characteristic to be read (service and char uuids).
* @param [in] handler function to be called when the characteristic changes value.
* @param [in] ready function to call when the enable procedure is finished (at the end of this function).
*/
void MetaWearBluetoothClient::enable_notifications(const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler,
	MblMwFnVoidVoidPtrInt ready) {

	int service_index;

	// proceed if the required characteristic is valid
	QLowEnergyCharacteristic target_characteristic = find_characteristic(characteristic, service_index, "enable_notifications");
	if (target_characteristic.isValid()) {

		// making sure the characteristic descriptor is valid
		QLowEnergyDescriptor notification = target_characteristic.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
		if (notification.isValid()) {

			// enabling notification for the descriptor
			m_metawear_services_p[service_index]->writeDescriptor(notification, QByteArray::fromHex("0100"));

			// adding an entry in the characteristic-callback map
			m_char_update_callback_map[target_characteristic.uuid().toString()] = std::tuple<const void*, MblMwFnIntVoidPtrArray>(caller, handler);
			
		}

	}

	// notifying that the enable notify task is completed
	QThread::msleep(DESCRIPTOR_WRITE_DELAY);
	ready(caller, 0);

}

/*
* Registers the provided (handler) function as a callback function when the device disconnects.
*
* @param [in] caller Voided context variable to pass in to the call back function when the requested char data is recevied.
* @param [in] handler function to register as a callback.
*/
void MetaWearBluetoothClient::on_disconnect(const void* caller, MblMwFnVoidVoidPtrInt handler) {

	// setting the handler and enabling it
	m_disconnect_handler = handler;
	m_disconnect_event_caller = caller;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// MetaWearBluetoothClient public methods

MetaWearBluetoothClient::MetaWearBluetoothClient(){

	// configuring the discovery agent
	m_discovery_agent.setLowEnergyDiscoveryTimeout(DISCOVERY_TIMEOUT);

	// hooking scanning events to their handler

	QObject::connect(&m_discovery_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, 
		this, &MetaWearBluetoothClient::device_discovered);
	
	QObject::connect(&m_discovery_agent, &QBluetoothDeviceDiscoveryAgent::finished, 
		this, &MetaWearBluetoothClient::device_discovery_finished);
	
	QObject::connect(&m_discovery_agent,
		static_cast<void (QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error),
		[this](QBluetoothDeviceDiscoveryAgent::Error error) {
			disconnect_device();
			clear_metawear_connection();
			write_debug_output("MetaWearBluetoothClient - ble device discovery level error occured, code : " + QString(error));
		});

}

MetaWearBluetoothClient::~MetaWearBluetoothClient() {
	disconnect_device();
	clear_metawear_connection();
}

void MetaWearBluetoothClient::connect_device() {

	// making sure that requirements have been loaded
	if (m_config_loaded && m_output_file_loaded && m_sensor_used) {
		
		clear_metawear_connection();

		// launching device discovery
		m_discovery_agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
		write_debug_output("MetaWearBluetoothClient - starting device scan\n");
	}

}

void MetaWearBluetoothClient::disconnect_device() {

	stop_stream();
	
	// changing the device state
	m_device_connected = false;
	emit device_status_change(false);

	write_debug_output("MetaWearBluetoothClient - client state change to disconnected\n");

}

void MetaWearBluetoothClient::start_stream() {

	// making sure device is ready to stream
	if (m_device_connected && !m_device_streaming) {

		// opening the output files
		set_output_file(m_output_folder_path);
		m_output_ori_file.open(m_output_ori_file_str, std::fstream::app);
		m_output_acc_file.open(m_output_acc_file_str, std::fstream::app);
		
		// connecting to redis (if redis enabled)
		if ((*m_config_ptr)["ext_imu_to_redis"] == "true") {
			m_redis_entry = (*m_config_ptr)["ext_imu_redis_entry"];
			m_redis_rate_div = std::atoi((*m_config_ptr)["ext_imu_redis_rate_div"].c_str());
			connect_to_redis();
		}
		
		MblMwFnData euler_angles_callback = [](void* context, const MblMwData* data) {

			MblMwEulerAngles* euler_angles = (MblMwEulerAngles*)data->value;
			MetaWearBluetoothClient* client_p = static_cast<MetaWearBluetoothClient*>(context);

			// only writting data to file in normal mode
			if (!client_p->get_stream_preview_status()) {
			
				// getting timestamps strings
				std::string reception_time_os = client_p->get_micro_timestamp();
				std::string data_time = std::to_string(data->epoch);

				// defining the output string
				std::string output_str = reception_time_os + "," + data_time + "," + std::to_string(euler_angles->heading) + ","
					+ std::to_string(euler_angles->pitch) + "," + std::to_string(euler_angles->roll) + "," + std::to_string(euler_angles->yaw)
					+ "\n";

				client_p->write_to_redis(output_str);
				client_p->m_output_ori_file << output_str;
			
			}

		};

		MblMwFnData acceleration_callback = [](void* context, const MblMwData* data) {

			MblMwCartesianFloat* acceleration = (MblMwCartesianFloat*)data->value;
			MetaWearBluetoothClient* client_p = static_cast<MetaWearBluetoothClient*>(context);

			// only writtingdata to file in normal mode
			if (!client_p->get_stream_preview_status()) {
			
				// getting timestamps strings
				std::string reception_time_os = client_p->get_micro_timestamp();
				std::string data_time = std::to_string(data->epoch);

				// defining the output string
				std::string output_str = reception_time_os + "," + data_time + "," + std::to_string(acceleration->x) + ','
					+ std::to_string(acceleration->y) + "," + std::to_string(acceleration->z) + "\n";

				client_p->m_output_acc_file << output_str;
			
			}

		};
		
		// setting up the callback for euler angles data
		auto euler_angles_sig = mbl_mw_sensor_fusion_get_data_signal(m_metawear_board_p, MBL_MW_SENSOR_FUSION_DATA_EULER_ANGLE);
		mbl_mw_datasignal_subscribe(euler_angles_sig, this, euler_angles_callback);

		// setting up the callback for cartesian acceleration
		auto acceleration_sig = mbl_mw_sensor_fusion_get_data_signal(m_metawear_board_p, MBL_MW_SENSOR_FUSION_DATA_LINEAR_ACC);
		mbl_mw_datasignal_subscribe(acceleration_sig, this, acceleration_callback);
			
		// enabling the relevant signals and starting the acquisition
		mbl_mw_sensor_fusion_enable_data(m_metawear_board_p, MBL_MW_SENSOR_FUSION_DATA_EULER_ANGLE);
		mbl_mw_sensor_fusion_enable_data(m_metawear_board_p, MBL_MW_SENSOR_FUSION_DATA_LINEAR_ACC);
		mbl_mw_sensor_fusion_start(m_metawear_board_p);

		m_device_streaming = true;
	}

}

void MetaWearBluetoothClient::stop_stream(void){

	// making sure device is streaming
	if (m_device_connected && m_device_streaming) {

		// stoping stream from board
		mbl_mw_sensor_fusion_stop(m_metawear_board_p);

		// closing the output files and redis connection
		m_output_ori_file.close();
		m_output_acc_file.close();
		if ((*m_config_ptr)["ext_imu_to_redis"] == "true") {
			disconnect_from_redis();
		}

		m_device_streaming = false;
	}

}

void MetaWearBluetoothClient::set_output_file(std::string output_folder_path) {

	try {

		m_output_folder_path = output_folder_path;

		// defining the output and sync output file paths
		m_output_ori_file_str = output_folder_path + "/ext_imu_orientation.csv";
		m_output_acc_file_str = output_folder_path + "/ext_imu_acceleration.csv";
		if (m_output_ori_file.is_open()) m_output_ori_file.close();
		if (m_output_acc_file.is_open()) m_output_acc_file.close();

		// writing the orientation output file header
		m_output_ori_file.open(m_output_ori_file_str);
		m_output_ori_file << "Reception OS time,Onboard time,Heading,Pitch,Roll,Yaw" << std::endl;
		m_output_ori_file.close();

		// writing the acceleration output file header
		m_output_acc_file.open(m_output_acc_file_str);
		m_output_acc_file << "Reception OS time,Onboard time,ACC X,ACC Y,ACC Z" << std::endl;
		m_output_acc_file.close();

		m_output_file_loaded = true;

	} catch (...) {
		write_debug_output("MetaWearBluetoothClient - error occured while setting the output file");
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// MetaWearBluetoothClient private slots

void MetaWearBluetoothClient::device_discovered(const QBluetoothDeviceInfo& device) {
	
	// getting the MAC adresses for comparison
	QString incoming_adress_str = device.address().toString();
	QString target_device_adress((*m_config_ptr)["ext_imu_ble_address"].c_str());

	// only interested in the target device
	if ((incoming_adress_str == target_device_adress) && !m_metawear_device_controller_p) {	
		m_metawear_device_controller_p = std::shared_ptr<QLowEnergyController>(QLowEnergyController::createCentral(device));
		write_debug_output("MetaWearBluetoothClient - device with address : " + incoming_adress_str + " discovered\n");
	}

}

void MetaWearBluetoothClient::device_discovery_finished(void) {
	
	// checking if the target device was  discovered
	if (m_metawear_device_controller_p != nullptr) {
		
		// hooking the relevant device event handlers

		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::connected,
			[this](void) {

				// launching service discovery for the device
				try {
					m_metawear_device_controller_p->discoverServices();
				} catch (...) {
					disconnect_device();
					clear_metawear_connection();
					write_debug_output("MetaWearBluetoothClient - error occured when trying to launch service discovery");
				}
				
			});

		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::disconnected,
			this, &MetaWearBluetoothClient::device_disconnected);

		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::serviceDiscovered,
			this, &MetaWearBluetoothClient::service_discovered);
		
		connect(m_metawear_device_controller_p.get(), &QLowEnergyController::discoveryFinished,
			this, &MetaWearBluetoothClient::service_discovery_finished, Qt::QueuedConnection);
		
		connect(m_metawear_device_controller_p.get(),
			static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
			[this](QLowEnergyController::Error error) {
				write_debug_output("MetaWearBluetoothClient - BLE device communication level error occured, code : " + QString(error));
			});

		// connecting to the target device
		try {
			m_metawear_device_controller_p->connectToDevice();
		} catch (...) {
			disconnect_device();
			clear_metawear_connection();
			write_debug_output("MetaWearBluetoothClient - error occured when trying to connect to client");
		}
		
	} else {
		disconnect_device();
		clear_metawear_connection();
		write_debug_output("MetaWearBluetoothClient - device discovery did not find the target device");
	}

}


void MetaWearBluetoothClient::device_disconnected(){

	// calling the registered callback for the MblMwBtleConnection structure
	if(m_disconnect_handler != nullptr) {
		m_disconnect_handler(m_disconnect_event_caller, 0);
	}

	// moving to a disconnected device state
	if (m_device_connected) {
		disconnect_device();
		clear_metawear_connection();
	}

	write_debug_output("MetaWearBluetoothClient - disconnected\n");
		
}

void MetaWearBluetoothClient::service_discovered(const QBluetoothUuid& gatt_uuid){

	// creating the service object from the uuid + configuring if valid
	QString service_uuid_str = gatt_uuid.toString();
	QLowEnergyService* service_p = m_metawear_device_controller_p->createServiceObject(gatt_uuid, this);
		
	if (service_p != nullptr) {

		// service objects are stored in a shared pointer vector
		int service_index = m_metawear_services_p.size();
		m_metawear_services_p.emplace_back(std::shared_ptr<QLowEnergyService>(service_p));

		// connecting the service object to the necessary callbacks

		connect(m_metawear_services_p[service_index].get(), &QLowEnergyService::characteristicRead,
			this, &MetaWearBluetoothClient::service_characteristic_read);

		connect(m_metawear_services_p[service_index].get(), &QLowEnergyService::characteristicChanged,
			this, &MetaWearBluetoothClient::service_characteristic_changed);

		connect(m_metawear_services_p[service_index].get(), &QLowEnergyService::characteristicWritten,
			this, &MetaWearBluetoothClient::service_characteristic_changed);

		connect(m_metawear_services_p[service_index].get(),
			QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error),
			[this](QLowEnergyService::ServiceError error) {
				write_debug_output("BLE service level (characteristic search) error occured : " + QString(error));
			});

		// discovering the characteristics associated to the service (ISSUE IS HERE)
		try {
			m_metawear_services_p[service_index]->discoverDetails();
		} catch (...) {
			write_debug_output("MetaWearBluetoothClient - error occured when trying to explore service details : " + service_uuid_str);
		}

		// debug service description
		write_debug_output("service with uuid : " + service_uuid_str + " discovered, info : ");
		write_debug_output("service name : " + m_metawear_services_p[service_index]->serviceName());

	}

}

void MetaWearBluetoothClient::service_discovery_finished() {
	
	// making sure atleast one service was detected
	if (!m_metawear_services_p.empty()) {

		// waiting for the details of each service to be discovered
		int ms_wait_time = 0;
		int discovered_count = 0;
		while (ms_wait_time < DISCOVER_DETAILS_DELAY) {
			discovered_count = 0;
			for (int i = 0; i < m_metawear_services_p.size(); i++) {
				if (m_metawear_services_p[i]->state() == QLowEnergyService::ServiceDiscovered) {
					discovered_count ++;
				} else {
					m_metawear_services_p[i]->discoverDetails();
				}
			}
			if (discovered_count == m_metawear_services_p.size()) break;
			ms_wait_time += 500;
			QThread::msleep(500);
		}

		// creating the object for interfacing with the metawear board
		m_metawear_ble_interface = { this, write_gatt_char_wrap, read_gatt_char_wrap, enable_notifications_wrap, on_disconnect_wrap };
		m_metawear_board_p = mbl_mw_metawearboard_create(&m_metawear_ble_interface);
		mbl_mw_metawearboard_set_time_for_response(m_metawear_board_p, METAWEARTIMEOUT);

		write_debug_output("MetaWearBluetoothClient - starting the MetaBoard initialization");

		// initializing the board + defining the initialization callback (the initialization process uses the bluetooth interface)
		mbl_mw_metawearboard_initialize(m_metawear_board_p, this, [](void* context, MblMwMetaWearBoard* board, int32_t status) -> void {

			bool connection_status = (status == 0);
			MetaWearBluetoothClient* bluetooth_client = (static_cast<MetaWearBluetoothClient*>(context));

			// configuring the board on succes
			if (connection_status) {
				mbl_mw_sensor_fusion_set_mode(board, MBL_MW_SENSOR_FUSION_MODE_NDOF);
				mbl_mw_sensor_fusion_set_acc_range(board, MBL_MW_SENSOR_FUSION_ACC_RANGE_8G);
				mbl_mw_sensor_fusion_write_config(board);
			}  else {
				bluetooth_client->disconnect_device();
				bluetooth_client->clear_metawear_connection();
			}

			// updating the connection status + notifying the main window
			bluetooth_client->set_connection_status(connection_status);
			emit bluetooth_client->device_status_change(connection_status);

			if (connection_status) bluetooth_client->write_debug_output("MetaWearBluetoothClient - MetaBoard initialization succeded");
			else bluetooth_client->write_debug_output("MetaWearBluetoothClient - MetaBoard initialization failed");

		});

	} else {
		disconnect_device();
		clear_metawear_connection();
		write_debug_output("MetaWearBluetoothClient - service discovery completed with no services found");
	}

}

/*
* Callback function for the (QLowEnergyService::characteristicRead) signal emited by all BLE services 
* Flow : 
* 1) The metaWear API calls read_gatt_char_wrap() -> read_gatt_char() which sends out a characteristic read request to the device and adds the specified handler to the callback queue
* 2) The MetaWear device responds to the request with the appropriate data and trigger this callback function
* 3) This function pulls the oldest handler from the callback queue and calls it with the received data
*/
void MetaWearBluetoothClient::service_characteristic_read(const QLowEnergyCharacteristic& descriptor, const QByteArray& value) {
	
	write_debug_output("MetaWearBluetoothClient - service_characteristic_read callback - (begin) ");

	// making sure request information is available
	if(!m_char_read_callback_queue.empty()) {
	
		// getting the callback and context for the corresponding (oldest) read request
		std::tuple<const void*, MblMwFnIntVoidPtrArray> info_tuple = m_char_read_callback_queue.front();
		m_char_read_callback_queue.pop();

		// calling the associated call back function
		try {
			std::get<1>(info_tuple)(
				std::get<0>(info_tuple),
				(uint8_t*)value.data(),
				(uint8_t)value.length()
			);
		} catch (...) {};
		
	}

	write_debug_output("MetaWearBluetoothClient - service_characteristic_read callback - (end) ");

}


/*
* Callback function for the (QLowEnergyService::characteristicChanged) and (QLowEnergyService::characteristicWritten) signals emited by all BLE services
* Flow :
* 1) The metaWear API calls write_gatt_char_wrap() -> write_gatt_char() which sends out a characteristic write request to the device
* 2) The MetaWear device confirms that data was written to the characteristic, trigerring this callback
* 3) This calls the registered callback function for the characteristic (if one was registered[) 
*/
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
		try {
			std::get<1>(callback_map_entry->second)(
				std::get<0>(callback_map_entry->second),
				converted_data,
				newValue.length()
			);
		} catch (...) {}
		
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// conveniance functions

void MetaWearBluetoothClient::clear_metawear_connection() {

	// clearing qt communication vars
	if (m_metawear_device_controller_p != nullptr) {
		m_metawear_device_controller_p->disconnectFromDevice();
		m_metawear_services_p.clear();
		m_metawear_device_controller_p.reset();
	}

	// clearing callback structures / vars
	m_char_update_callback_map.clear();
	bytes_callback_queue empty_queue;
	std::swap(m_char_read_callback_queue, empty_queue);
	m_disconnect_event_caller = nullptr;
	m_disconnect_handler = nullptr;

	// clearing the metawear related vars
	if (m_metawear_board_p != nullptr) {
		m_metawear_ble_interface = { 0 };
		mbl_mw_metawearboard_free(m_metawear_board_p);
		m_metawear_board_p = nullptr;
	}

}

 QLowEnergyCharacteristic MetaWearBluetoothClient::find_characteristic(const MblMwGattChar* characteristic_struct, int& service_index, QString debug_str) {

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
	write_debug_output("........................... find_characteristic call (begin)");
	write_debug_output("target service uuid : " + target_service_uuid.toString());
	if (characteristic.isValid()) write_debug_output("target service name : " + m_metawear_services_p[service_index]->serviceName());
	write_debug_output("target characteristic uuid : " + target_characteristic_uuid.toString());
	if (characteristic.isValid()) write_debug_output("target characteristic name : " + characteristic.name());
	write_debug_output("requests in the callback queue : " + QString(int(m_char_read_callback_queue.size())));
	write_debug_output("debug string : " + debug_str);
	write_debug_output("........................... find_characteristic call (end)");

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
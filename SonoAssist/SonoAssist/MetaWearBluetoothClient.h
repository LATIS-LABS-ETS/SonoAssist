#pragma once

#include <queue>
#include <tuple>
#include <memory>
#include <string>
#include <chrono>
#include <exception>
#include <fstream>

#include <QThread>
#include <QtGlobal>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>

#include <cpp_redis/cpp_redis>

#include "metawear/core/data.h"
#include "metawear/core/types.h"
#include "metawear/core/datasignal.h"
#include "metawear/core/metawearboard.h"
#include "metawear/sensor/accelerometer.h"
#include "metawear/sensor/sensor_fusion.h"

#define DISCOVERYTIMEOUT 5000
#define DISCOVER_DETAILS_DELAY 1000
#define DESCRIPTOR_WRITE_DELAY 500

typedef std::map<std::string, std::string> config_map;
typedef std::map<QString, std::tuple<const void*, MblMwFnIntVoidPtrArray>> bytes_callback_map;
typedef std::queue<std::tuple<const void*, MblMwFnIntVoidPtrArray>> bytes_callback_queue;

// functions for the (MblMwBtleConnection) structure
void read_gatt_char_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler);
void write_gatt_char_wrap(void* context, const void* caller, MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic, const uint8_t* value, uint8_t length);
void enable_notifications_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler, MblMwFnVoidVoidPtrInt ready);
void on_disconnect_wrap(void* context, const void* caller, MblMwFnVoidVoidPtrInt handler);

class MetaWearBluetoothClient : public QObject {

	Q_OBJECT

	public:
	
		MetaWearBluetoothClient();
		~MetaWearBluetoothClient();

		// setters and getters
		bool get_device_status();
		void set_device_status(bool state);
		void set_output_file(std::string output_file_path, std::string extension);
		void set_configuration(std::shared_ptr<config_map> config_ptr);

		void clear_communication(void);

		// ui interface functions
		void stop_data_stream(void);
		void start_data_stream(void);
		void connect_to_metawear_board(void);

		// metawear interface functions
		void read_gatt_char(const void* caller,
			const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler);
		void write_gatt_char(const void* caller, MblMwGattCharWriteType writeType,
			const MblMwGattChar* characteristic, const uint8_t* value, uint8_t length);
		void enable_notifications(const void* caller, const MblMwGattChar* characteristic,
			MblMwFnIntVoidPtrArray handler, MblMwFnVoidVoidPtrInt ready);
		void on_disconnect(const void* caller, MblMwFnVoidVoidPtrInt handler);

		// output stream vars
		int m_redis_rate_div = 2;
		int m_redis_data_count = 1;
		std::ofstream m_output_file;
		std::string m_redis_entry = "";
		cpp_redis::client m_redis_client;

		// metawear communication attributes
		MblMwBtleConnection m_metawear_ble_interface = { 0 };
		MblMwMetaWearBoard* m_metawear_board_p = nullptr;

	private slots:

		// ble device slots
		void device_disconnected();
		void device_discovered(const QBluetoothDeviceInfo& device);
		
		// ble service slots
		void service_discovery_finished();
		void service_discovered(const QBluetoothUuid& gatt);
		
		// service characteristic slots
		void service_characteristic_read(const QLowEnergyCharacteristic& descriptor, const QByteArray& value);
		void service_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue);

	signals:
		void device_status_change(bool is_connected);

	private:

		bool m_device_connected = false;
		bool m_device_streaming = false;
		
		// config vars 
		bool m_config_loaded = false;
		std::shared_ptr<config_map> m_config_ptr;

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_output_file_str = "";

		// device disconnect handling vars
		const void* m_disconnect_event_caller = nullptr;
		MblMwFnVoidVoidPtrInt m_disconnect_handler = nullptr;

		// ble communication objects
		QBluetoothDeviceDiscoveryAgent m_discovery_agent;
		std::vector<std::shared_ptr<QLowEnergyService>> m_metawear_services_p;
		std::shared_ptr<QLowEnergyController> m_metawear_device_controller_p = nullptr;
		
		// callback structure
		bytes_callback_map m_char_update_callback_map;
		bytes_callback_queue m_char_read_callback_queue;

		// convenience functions
		QBluetoothUuid metawear_uuid_to_qt_uuid(const uint64_t uuid_low, const uint64_t uuid_high) const;
		QLowEnergyCharacteristic find_characteristic(const MblMwGattChar* characteristic_struct, int& service_index, QString debug_str="") const;

};
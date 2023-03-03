#pragma once

#include "SensorDevice.h"

#include <queue>
#include <tuple>
#include <memory>
#include <string>
#include <fstream>

#include <QThread>
#include <QtGlobal>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>

#include "metawear/core/data.h"
#include "metawear/core/types.h"
#include "metawear/core/datasignal.h"
#include "metawear/core/metawearboard.h"
#include "metawear/sensor/accelerometer.h"
#include "metawear/sensor/sensor_fusion.h"

#define DISCOVER_DETAILS_DELAY 5000
#define DESCRIPTOR_WRITE_DELAY 500
#define DISCOVERY_TIMEOUT 5000
#define METAWEARTIMEOUT 500

using bytes_callback_map = std::map<QString, std::tuple<const void*, MblMwFnIntVoidPtrArray>>;
using bytes_callback_queue = std::queue<std::tuple<const void*, MblMwFnIntVoidPtrArray>>;

/*******************************************************************************
* WRAPPER FUNCTIONS FOR THE (MblMwBtleConnection) STRUCTURE
******************************************************************************/

void read_gatt_char_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler);
void write_gatt_char_wrap(void* context, const void* caller, MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic, const uint8_t* value, uint8_t length);
void enable_notifications_wrap(void* context, const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler, MblMwFnVoidVoidPtrInt ready);
void on_disconnect_wrap(void* context, const void* caller, MblMwFnVoidVoidPtrInt handler);

/**
* Class to enable communication with the MetaWear MetaMotionC gyroscope/magnometer/accelerometer (IMU) sensor.
* 
* Communication with the MetaWear C sensor is done via BLE.
* This class can't utilise threads since it utilises the Bluetooth interface provided by Qt. 
* The call back functions provided by this interface can only run in the main thread.
* This class does not implement the acquisition preview functionality
*/
class MetaWearBluetoothClient : public SensorDevice {

	public:
	
		MetaWearBluetoothClient(int device_id, std::string device_description, std::string redis_state_entry, std::string log_file_path);
		~MetaWearBluetoothClient();
		
		/*******************************************************************************
		* SENSOR DEVICE OVERRIDES
		******************************************************************************/

		void stop_stream(void) override;
		void start_stream(void) override;
		void connect_device(void) override;
		void disconnect_device(void) override;
		void set_output_file(const std::string& output_folder) override;

		/*******************************************************************************
		* METHODS FOR THE (MblMwBtleConnection) INTERFACE
		******************************************************************************/

		/**
		* Reads the specified BLE characteristic from the currently connected device.
		* The function does nothing if no device is not connected or if the provided characteristic is invalid.
		*
		* \param caller Voided context variable to pass in to the call back function when the requested char data is recevied.
		* \param characteristic UUIDs specifying the characteristic to be read (service and char uuids).
		* \param handler Callback function to call when the characteristic data is received.
		*/
		void read_gatt_char(const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler);

		/**
		* Writes the specified data to the target BLE characteristic.
		* The function does nothing if no device is not connected or if the provided characteristic is invalid.
		*
		* \param writeType Specifies if the write operation should prompt an update (not really usefull).
		* \param characteristic UUIDs specifying the characteristic to be writen to (service and char uuids).
		* \param value payload to be sent to the target characteristic
		* \param length length of the value array
		*/
		void write_gatt_char(MblMwGattCharWriteType writeType, const MblMwGattChar* characteristic, const uint8_t* value, uint8_t length);

		/**
		* Turns on value change notifications for the specified characteristic.
		* In the future, when the characteristic the specified handler will be called.
		* The function does nothing if no device is not connected or if the provided characteristic is invalid.
		*
		* \param caller Voided context variable to pass in to the call back function when the requested char data is recevied.
		* \param characteristic UUIDs specifying the characteristic to be read (service and char uuids).
		* \param handler function to be called when the characteristic changes value.
		* \param ready function to call when the enable procedure is finished (at the end of this function).
		*/
		void enable_notifications(const void* caller, const MblMwGattChar* characteristic, MblMwFnIntVoidPtrArray handler, MblMwFnVoidVoidPtrInt ready);

		/**
		* Registers the provided (handler) function as a callback function when the device disconnects.
		*
		* \param caller Voided context variable to pass in to the call back function when the requested char data is recevied.
		* \param handler function to register as a callback.
		*/
		void on_disconnect(const void* caller, MblMwFnVoidVoidPtrInt handler);

		// file output attributes + redis
		std::ofstream m_output_ori_file;
		std::ofstream m_output_acc_file;
		std::string m_redis_entry = "";

		// metawear communication attributes
		MblMwMetaWearBoard* m_metawear_board_p = nullptr;
		MblMwBtleConnection m_metawear_ble_interface = { 0 };

	private:
		
		/*******************************************************************************
		* CONVENIANCE FUNCTIONS
		******************************************************************************/

		void clear_metawear_connection(void);
		QBluetoothUuid metawear_uuid_to_qt_uuid(const uint64_t uuid_low, const uint64_t uuid_high) const;
		QLowEnergyCharacteristic find_characteristic(const MblMwGattChar* characteristic_struct, int& service_index, const QString& debug_str = "");
	
		// synchronization status var
		bool m_device_sync_streaming = false;

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_output_ori_file_str = "";
		std::string m_output_acc_file_str = "";

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

	private slots:

		/*******************************************************************************
		* (MetaWearBluetoothClient) PRIVATE SLOTS
		******************************************************************************/

		// ble device slots
		void device_disconnected(void);
		void device_discovery_finished(void);
		void device_discovered(const QBluetoothDeviceInfo& device);

		// ble service slots
		void service_discovery_finished();
		void service_discovered(const QBluetoothUuid& gatt);

		// service characteristic slots

		/**
		* Callback function for the (QLowEnergyService::characteristicRead) signal emited by all BLE services
		* Flow :
		*	1) The metaWear API calls read_gatt_char_wrap() -> read_gatt_char() which sends out a characteristic read request to the device and adds the specified handler to the callback queue
		*	2) The MetaWear device responds to the request with the appropriate data and trigger this callback function
		*	3) This function pulls the oldest handler from the callback queue and calls it with the received data
		*/
		void service_characteristic_read(const QLowEnergyCharacteristic& descriptor, const QByteArray& value);

		/**
		* Callback function for the (QLowEnergyService::characteristicChanged) and (QLowEnergyService::characteristicWritten) signals emited by all BLE services
		* Flow :
		*	1) The metaWear API calls write_gatt_char_wrap() -> write_gatt_char() which sends out a characteristic write request to the device
		*	2) The MetaWear device confirms that data was written to the characteristic, trigerring this callback
		*	3) This calls the registered callback function for the characteristic (if one was registered[)
		*/
		void service_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue);

};
cmake_minimum_required (VERSION 3.8)

project("SonoAssist")

add_definitions(-DUNICODE -D_UNICODE)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH C:/Qt/5.14.2/msvc2017_64)

set(DEPENDENCIES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies)
set(DEPENDENCIES_INCLUDES
	${DEPENDENCIES_DIR}/clarius_listener/src/include
	${DEPENDENCIES_DIR}/MetaWear-SDK-Cpp-0.18.4/src
	${DEPENDENCIES_DIR}/stream_engine_windows_x64_4.1.0.3/include
)
set(DEPENDENCIES_LIBS
	${DEPENDENCIES_DIR}/clarius_listener/lib/listen.lib
	${DEPENDENCIES_DIR}/MetaWear-SDK-Cpp-0.18.4/dist/Release/lib/x64/MetaWear.Win32.lib
	${DEPENDENCIES_DIR}/stream_engine_windows_x64_4.1.0.3/lib/tobii/tobii_stream_engine.lib
)
include_directories(${DEPENDENCIES_INCLUDES})

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS Xml REQUIRED)
find_package(Qt5 COMPONENTS Bluetooth REQUIRED)

conan_basic_setup()

add_executable(SonoAssist WIN32
	"SonoAssist.ui"
	"SensorDevice.cpp" "SensorDevice.h"
	"GazeTracker.cpp" "GazeTracker.h"
	"OSKeyDetector.cpp" "OSKeyDetector.h"
	"ScreenRecorder.cpp" "ScreenRecorder.h"
	"RGBDCameraClient.cpp" "RGBDCameraClient.h"
	"process_management.cpp" "process_management.h"
	"MetaWearBluetoothClient.cpp" "MetaWearBluetoothClient.h"
	"ClariusProbeClient.cpp" "ClariusProbeClient.h"
	"SonoAssist.cpp" "SonoAssist.h"
	"main.cpp" "main.h"
	"SonoAssist.rc"
)

target_link_libraries(SonoAssist ${CONAN_LIBS} ${DEPENDENCIES_LIBS} Qt5::Widgets Qt5::Xml Qt5::Bluetooth)
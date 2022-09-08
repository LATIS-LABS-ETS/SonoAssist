'''
This script displays the orientation data of an IMU recorded by SonoAssist.

Usage
-----
python3 display_recorded_imu.py (path to the acquisition folder)
'''

import argparse

from sonopy.imu import IMUDataManager, OrientationScene


if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # loading the IMU (orientation) data
    avg_window = 0.10
    imu_data = IMUDataManager(args.acquisition_dir, avg_window=avg_window)
    
    # initializing the graphics display
    scene = OrientationScene(update_pause_time=avg_window, display_triangles=True)
    
    # updating the arrow display by feeding in IMU orientation data
    for i in range(len(imu_data)):
        imu_acquisition = imu_data[i]
        scene.update_dynamic_arrow(float(imu_acquisition["Roll"]), float(imu_acquisition["Pitch"]), float(imu_acquisition["Yaw"]))
        scene.update_display()
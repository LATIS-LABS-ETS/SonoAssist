import os
import argparse
import numpy as np

from sonopy.file_management import SonoFolderManager

# parsing script arguments
parser = argparse.ArgumentParser()
parser.add_argument("config_path", help="path to the .json config file")
args = parser.parse_args()

# constants
n_ms_in_s = 1000

def mean_acquisition_rate(data_frame):

    '''
    Parameters
    ----------
    data_frame (pandas.DataFrame) : acquisition data
    '''

    # getting time measures
    rate_measurements = []
    timestamps = data_frame["Time (ms)"]
    for i in range(len(data_frame.index) - 1):
        measurement = 1 / ((timestamps[i+1] - timestamps[i]) / n_ms_in_s)
        rate_measurements.append(measurement)

    # getting mean rate
    rate_measurements = np.array(rate_measurements)
    return rate_measurements.mean()


if __name__ == "__main__":

    # getting access to the acquisition output files
    folder_manager = SonoFolderManager(args.config_path)

    # measuring fos rates for the different sources
    
    clarius_data = folder_manager.load_clarius_data()  
    if clarius_data is not None :
        mean_fps = mean_acquisition_rate(clarius_data)
        print(f"Clarius data avg rate : {mean_fps} Hz")

    sc_data = folder_manager.load_screen_rec_data()  
    if sc_data is not None :
        mean_fps = mean_acquisition_rate(sc_data)
        print(f"Screen recorder data avg rate : {mean_fps} Hz")

    gaze_data = folder_manager.load_gaze_data()  
    if gaze_data is not None :
        mean_fps = mean_acquisition_rate(gaze_data)
        print(f"Gaze data avg rate : {mean_fps} Hz")

    (ext_ori_data, ext_acc_data) = folder_manager.load_ext_imu_data()  
    if (ext_ori_data is not None) and (ext_acc_data is not None):
        ori_mean_fps = mean_acquisition_rate(gaze_data)
        acc_mean_fps = mean_acquisition_rate(ext_acc_data)
        print(f"External IMU orientation data avg rate : {ori_mean_fps} Hz")
        print(f"External IMU acceleration data avg rate : {acc_mean_fps} Hz")
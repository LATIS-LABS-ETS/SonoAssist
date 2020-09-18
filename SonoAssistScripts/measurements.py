import os
import argparse
import numpy as np

from sonopy.file_management import SonoFolderManager

# parsing script arguments
parser = argparse.ArgumentParser()
parser.add_argument("config_path", help="path to the .json config file")
args = parser.parse_args()

def measure_acquisition_rate(display_title, data_frame):

    '''
    Prints out (avg, min, max)

    Parameters
    ----------
    display_title (str) : header for the measurements print out
    data_frame (pandas.DataFrame) : acquisition data
    '''

    # constants
    n_min_max = 5
    n_us_in_s = 1000000

    # getting timming data
    timestamps = data_frame["Time (us)"]
    n_timestamps = len(data_frame.index)

    # getting time diffs between received data
    rate_measurements = []
    for i in range(n_timestamps-1):
        measurement = 1 / ((timestamps[i+1] - timestamps[i]) / n_us_in_s)
        rate_measurements.append(measurement)

    # getting avg rate via n acquisitions in total time
    # getting variation via measurement time diffs
    avg_rate = n_timestamps / ((timestamps[n_timestamps-1] - timestamps[0]) / n_us_in_s)
    avg_variation = sum(rate_measurements) / len(rate_measurements)

    # getting max rates
    max_rates = []
    for i in range(n_min_max):
        max_rates.append(max(rate_measurements))
        del rate_measurements[rate_measurements.index(max_rates[-1])]

    # getting min rates
    min_rates = []
    for i in range(n_min_max):
        min_rates.append(min(rate_measurements))
        del rate_measurements[rate_measurements.index(min_rates[-1])]  
    
    print(f"\n\n{display_title} : \n")
    print(f"Average rate : {avg_rate} (Hz)")
    print(f"Average variation : {avg_variation} (Hz)")
    print(f"Top {n_min_max} max rates (Hz) : {max_rates}")
    print(f"Top {n_min_max} min rates (Hz) : {min_rates}")
    print("\n")
    

if __name__ == "__main__":

    # getting access to the acquisition output files
    folder_manager = SonoFolderManager(args.config_path)

    # measuring fos rates for the different sources
    
    clarius_data = folder_manager.load_clarius_data()  
    if clarius_data is not None :
        measure_acquisition_rate("Clarius probe rates", clarius_data)
       
    sc_data = folder_manager.load_screen_rec_data()  
    if sc_data is not None :
        measure_acquisition_rate("Screen recorder rates", sc_data)

    gaze_data = folder_manager.load_gaze_data()  
    if gaze_data is not None :
        measure_acquisition_rate("Eye tracker rates", gaze_data)

    (ext_ori_data, ext_acc_data) = folder_manager.load_ext_imu_data()  
    if (ext_ori_data is not None) and (ext_acc_data is not None):
        measure_acquisition_rate("External IMU orientation rates", ext_ori_data)
        measure_acquisition_rate("External IMU acceleration rates", ext_acc_data)
'''
Measurement of the acquisition rates

Usage : 
    python3 measurements.py (path to the acquisition folder)
'''

import os
import math
import argparse
import numpy as np

from sonopy.file_management import SonoFolderManager


def measure_acquisition_rate_stats(data_frame, time_col_name="Time (us)"):

    '''
    Calculates the following metrics related to the acquisition rate of the provided data: 
        - avg acquisition rate
        - avg time difference between acquisitions
        - plot of time differences in time
        - simple list of all time diffs

    Parameters
    ----------
    data_frame (pandas.DataFrame) : acquisition data
    time_col_name (str) : name of the dataframe column containing the time stamps foreach entry

    Returns
    -------
    (avg rate, avg time diff, time diff plot, time diff list)
    '''

    # constants
    n_us_in_s = 1000000
    plot_percentage_divide = 2

    # getting timming data + removing rows with no timestamp entries
    data_frame.drop(data_frame.index[data_frame[time_col_name] == " "], inplace=True)
    data_frame.reset_index(drop=True, inplace=True)
    timestamps = data_frame[time_col_name].astype(int)
    n_timestamps = len(timestamps)

    # getting time diffs between received data
    rate_measurements = []
    for i in range(n_timestamps-1):
        measurement = 1 / ((timestamps[i+1] - timestamps[i]) / n_us_in_s)
        rate_measurements.append(measurement)

    # defining a plot of time diff values (5% X divisions)
    plot = {"X" : [], "Y" : []}
    
    # filling the x axis
    plot["X"] = list(range(plot_percentage_divide, 100, plot_percentage_divide))
    plot["X"].append(100)
    
    # filling the y axis
    start_index = 0
    n_iterations = len(plot["X"])
    n_measurements = len(rate_measurements)
    measure_range = math.floor(n_measurements * (plot_percentage_divide / 100))
    
    for iter_i in range(n_iterations):

        # defining the end index for the current rnge of values
        end_index = 0
        if iter_i == n_iterations-1 :
            end_index = n_measurements - 1
        else : 
            end_index = start_index + measure_range

        # getting avg value for the current range
        diff_sum = 0
        for i in range(start_index, end_index):
            diff_sum += rate_measurements[i]
        diff_sum /= (end_index - start_index)
        plot["Y"].append(diff_sum)

        # pushing the start index for the next iteration
        start_index += measure_range

    # getting avg rate via n acquisitions in total time
    # getting variation via measurement time diffs
    avg_rate = n_timestamps / ((timestamps[n_timestamps-1] - timestamps[0]) / n_us_in_s)
    avg_variation = sum(rate_measurements) / len(rate_measurements)

    return (avg_rate, avg_variation, plot, rate_measurements)


if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Path to the acquisition folder")
    args = parser.parse_args()

    if os.path.isdir(args.acquisition_dir):
    
        stat_container = {}
        folder_manager = SonoFolderManager(args.acquisition_dir)        
        
        clarius_data = folder_manager.load_clarius_data()
        if clarius_data is not None :
            stat_container["clarius"] = measure_acquisition_rate_stats(clarius_data, "Display OS time")
            avg = stat_container["clarius"][0]
            print(f"Clarius avg rate : {avg}")

        sc_data = folder_manager.load_screen_rec_data() 
        if sc_data is not None : 
            stat_container["sc"] = measure_acquisition_rate_stats(sc_data)
            avg = stat_container["sc"][0]
            print(f"Screen recorder avg rate : {avg}")
            
        (gaze_data, head_data) = folder_manager.load_eye_tracker_data()
        if gaze_data is not None :
            stat_container["gaze"] = measure_acquisition_rate_stats(gaze_data, "Reception OS time")
            avg = stat_container["gaze"][0]
            print(f"Eye tracker avg rate : {avg}")
        
        (ext_ori_data, ext_acc_data) = folder_manager.load_ext_imu_data()
        if ext_ori_data is not None :
            stat_container["ext_imu"] = measure_acquisition_rate_stats(ext_ori_data, "Reception OS time")
            avg = stat_container["ext_imu"][0]
            print(f"External imu avg rate : {avg}")
'''
Characterisation of the measurements acquired by the acquisition tool

    - Config 1 : External IMU, Eye tracker, RGBD camera and Clarius probe (540 x 960)
    - Config 2 : External IMU, Eye tracker, RGBD camera and Clarius probe (720 x 1280)
    - Config 3 : External IMU, Eye tracker, RGBD camera, Screen recorder (1080 x 1920)

Usage : 

    python3 measurements.py (path to parent folder of the acquisition folders)
'''

import os
import math
import argparse
import numpy as np

from sonopy.file_management import SonoFolderManager


def measure_acquisition_rate_stats(data_frame, time_col_name="Time (us)"):

    '''
    Calculates the following metrics related to the acquisition rate : 
        
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

    # getting timming data
    timestamps = data_frame[time_col_name]
    n_timestamps = len(data_frame.index)

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
    
    for i in range(n_iterations):

        # defining the end index for the current rnge of values
        end_index = 0
        if i == n_iterations-1 :
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


def load_all_stats(parent_folder_path):

    '''
    Loads the acquisition rate statistics for all acquisition folders in the provided parent folder

    Parameters
    ----------
    parent_folder_path (string) : path to the parent folder holding the acquisition folders

    Returns
    -------
    (config1_stats, config2_stats, config3_stats)

    '''

    # defining a container for each config type
    config1_stats = {}
    config2_stats = {}
    config3_stats = {}

    # going through the acquisition folders
    for element in os.listdir(parent_folder_path):
        acquisition_folder = os.path.join(parent_folder_path, element)
        if os.path.isdir(acquisition_folder):

            # getting a reference to the proper stat container
            stat_container = None
            if "config1" in element: stat_container = config1_stats
            elif "config2" in element: stat_container = config2_stats
            else: stat_container = config3_stats

            # getting access to the acquisition output files
            folder_manager = SonoFolderManager(acquisition_folder)

            # loading data from the different files
            rgbd_data = folder_manager.load_rgbd_data()
            sc_data = folder_manager.load_screen_rec_data()
            clarius_data = folder_manager.load_clarius_data()
            (gaze_data, head_data) = folder_manager.load_eye_tracker_data()
            (ext_ori_data, ext_acc_data) = folder_manager.load_ext_imu_data()

            # clarius average rates
            stat_key = "clarius"
            if not stat_key in stat_container: stat_container[stat_key] = []
            if clarius_data is not None :
                stat_container[stat_key].append(measure_acquisition_rate_stats(clarius_data, "Display OS time"))
            
           # screen recorder average rates
            stat_key = "sc"
            if not stat_key in stat_container: stat_container[stat_key] = []
            if sc_data is not None :
                stat_container[stat_key].append(measure_acquisition_rate_stats(sc_data))
                
            # gaze data
            stat_key = "gaze"
            if not stat_key in stat_container: stat_container[stat_key] = []
            if gaze_data is not None :
                stat_container[stat_key].append(measure_acquisition_rate_stats(gaze_data, "Reception OS time"))

            # ext ori data
            stat_key = "ext_imu"
            if not stat_key in stat_container: stat_container[stat_key] = []
            if ext_ori_data is not None :
                stat_container[stat_key].append(measure_acquisition_rate_stats(ext_ori_data, "Reception OS time"))

    return (config1_stats, config2_stats, config3_stats)


if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("target_dir", help="Directory containing the acquisition folders")
    args = parser.parse_args()

    # loading acquisition stats
    (config1_stats, config2_stats, config3_stats) = load_all_stats(args.target_dir)

    # getting mean rate values example
    
    config1_clarius_rates = [element[0] for element in config1_stats["clarius"]]
    print(f"Clarius (540 x 920) rates : {config1_clarius_rates}")
    
    config2_clarius_rates = [element[0] for element in config2_stats["clarius"]]
    print(f"Clarius (720 x 1280) rates : {config2_clarius_rates}")
    
    config3_sc_rates = [element[0] for element in config3_stats["sc"]]
    print(f"Screen recorder rates rates : {config3_sc_rates}")

    # getting plots examples
    
    config1_clarius_plots = [element[2] for element in config1_stats["clarius"]]
    print("Clarius (540 x 920) plots : \n")
    for plot in config1_clarius_plots:
        print("\n", plot, "\n")

    config1_eyetracker_plots = [element[2] for element in config1_stats["gaze"]]
    print("Config1 eyetracker plots : \n")
    for plot in config1_eyetracker_plots:
        print("\n", plot, "\n")
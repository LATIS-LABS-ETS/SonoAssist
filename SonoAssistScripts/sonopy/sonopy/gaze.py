import math
import numpy as np
import pandas as pd

from sonopy.file_management import SonoFolderManager


class GazeDataManager():


    ''' Manages acquired data from the tobii 4c eyetracker '''

    # os acquisition time field name
    os_acquisition_time = "OS acquisition time"
    # head position slicing percentage
    head_data_slice_percentage = 10
    # screen physical dimensions (meters)
    screen_width = 0.35
    screen_height = 0.20
    # filter gaze speed (degrees / second)
    max_gaze_speed = 30


    def __init__(self, acquisition_dir_path):

        ''' 
        Parameters
        ----------
        acquisition_dir_path (str) : path to the acquisition directory
        '''

        # loading eye tracker data from the acquisition folder
        self.folder_manager = SonoFolderManager(acquisition_dir_path)
        (self.gaze_data, self.head_pos_data) = self.folder_manager.load_eye_tracker_data()

        # converting timestamps from str to numeric
        self.gaze_data["Reception OS time"] = pd.to_numeric(self.gaze_data["Reception OS time"], errors="coerce")
        self.gaze_data["Reception tobii time"] = pd.to_numeric(self.gaze_data["Reception tobii time"], errors="coerce")
        self.gaze_data["Onboard time"] = pd.to_numeric(self.gaze_data["Onboard time"], errors="coerce")
        self.head_pos_data["Reception OS time"] = pd.to_numeric(self.head_pos_data["Reception OS time"], errors="coerce")
        self.head_pos_data["Reception tobii time"] = pd.to_numeric(self.head_pos_data["Reception tobii time"], errors="coerce")
        self.head_pos_data["Onboard time"] = pd.to_numeric(self.head_pos_data["Onboard time"], errors="coerce")

        # getting length of data
        self.n_gaze_acquisitions = len(self.gaze_data.index)
        self.n_head_acquisitions = len(self.head_pos_data.index)

        # defining data containers
        self.avg_head_positions = []

        # data processing steps
        self.calculate_os_acquisition_time()
        self.calculate_avg_head_positions()
        self.filter_gaze_data(max_gaze_speed=self.max_gaze_speed, screen_width=self.screen_width, 
            screen_height=self.screen_height)


    def calculate_os_acquisition_time(self):

        ''' Computes the timestamp at which the data (head and gaze) was acquired for the OS domain '''
        
        for gaze_i in range(self.n_gaze_acquisitions):
            self.gaze_data.loc[gaze_i ,self.os_acquisition_time] = (self.gaze_data.loc[gaze_i, "Reception OS time"] - 
                (self.gaze_data.loc[gaze_i, "Reception tobii time"] - self.gaze_data.loc[gaze_i, "Onboard time"]))

        for head_i in range(self.n_head_acquisitions):
            self.head_pos_data.loc[head_i, self.os_acquisition_time] = (self.head_pos_data.loc[head_i, "Reception OS time"] - 
                (self.head_pos_data.loc[head_i, "Reception tobii time"] - self.head_pos_data.loc[head_i, "Onboard time"]))


    def calculate_avg_head_positions(self):

        ''' 
        Calculates the average head position for every 10% slice of data 
        Measurements are converted from (mm) to (m)
        '''

        n_slices = 100 // self.head_data_slice_percentage
        slice_size = self.n_head_acquisitions // n_slices

        for slice_i in range(n_slices):

            avg_data = [None, None]
            start_index = slice_i * slice_size

            if slice_i == n_slices-1:
                avg_data[0] = np.mean(self.head_pos_data.loc[start_index : , "Z"]) / 1000
                avg_data[1] = self.head_pos_data.loc[self.n_head_acquisitions-1, self.os_acquisition_time]
            else :
                end_index = start_index + slice_size
                avg_data[0] = np.mean(self.head_pos_data.loc[start_index : end_index, "Z"]) / 1000
                avg_data[1] = self.head_pos_data.loc[end_index, self.os_acquisition_time]

            self.avg_head_positions.append(tuple(avg_data))


    def filter_gaze_data(self, max_gaze_speed=30, screen_width=40, screen_height=20):

        ''' 
        Removes gaze points associated with a gaze speed higher than (max_gaze_speed)

        Parameters
        ----------
        max_gaze_speed (float) : the max gaze speed in (degress / second)
        screen_width (float) : physical width of the screen
        screen_height (float) : physical height of the screen
        '''

        # calculating the max speeds for every average head position (m / s)
        max_speeds = []
        for avg_data in self.avg_head_positions:
            speed_data = [None , None]
            speed_data[0] = (2 * math.tan(math.radians(max_gaze_speed)/2)) / avg_data[0]
            speed_data[1] = avg_data[1]
            max_speeds.append(tuple(speed_data))

        # finding points with excessive speeds
        drop_indexes = []
        for gaze_i in range(self.n_gaze_acquisitions - 1):

            first_point = self.gaze_data.iloc[gaze_i]
            second_point = self.gaze_data.iloc[gaze_i + 1]

            # calculating speed for the x and y directions
            time_diff = (second_point[self.os_acquisition_time] - first_point[self.os_acquisition_time]) / 1000000
            x_speed = (abs(second_point["X"] - first_point["X"]) * screen_width) / time_diff
            y_speed = (abs(second_point["Y"] - first_point["Y"]) * screen_height) / time_diff

            # getting the proper speed limit
            max_speed = max_speeds[-1][0]
            for speed_data in max_speeds:
                if first_point[self.os_acquisition_time] <= speed_data[1]:
                    max_speed = speed_data[0]
                    break

            # marking gaze points with excessive speeds for deletion
            if (x_speed >= max_speed) or (y_speed >= max_speed):
                drop_indexes.append(gaze_i)
                if gaze_i not in drop_indexes : drop_indexes.append(gaze_i)

        # removing points with excessive speeds
        self.gaze_data.drop(drop_indexes, inplace=True)
        self.gaze_data.reset_index(drop=True, inplace=True)
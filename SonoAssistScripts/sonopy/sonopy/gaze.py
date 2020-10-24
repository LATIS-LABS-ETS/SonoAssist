import math
import numpy as np
import pandas as pd

from sonopy.config import ConfigurationManager
from sonopy.file_management import SonoFolderManager


class GazeDataManager():

    ''' Manages acquired data from the tobii 4c eyetracker '''

    # on display coordinates field names
    x_display_coord = "X display"
    y_display_coord = "Y display"

    # os acquisition time field name
    os_acquisition_time = "OS acquisition time"


    def __init__(self, acquisition_dir_path, config_file_path, filter_gaze_data=True):

        ''' 
        Parameters
        ----------
        acquisition_dir_path (str) : path to the acquisition directory
        config_file_path (str) : path to the configuration (.json) file
        filter_gaze_data (bool) : when true, the loaded gaze data is filtered in the constructor (speed and position)
        '''

        self.filter_gaze_data = filter_gaze_data

        # loading configurations
        self.config_manager = ConfigurationManager(config_file_path)
        self.saliency_map_width = self.config_manager["saliency_map_width"]
        self.saliency_point_max_reach = self.config_manager["saliency_point_max_reach"]

        # loading data from the acquisition folder
        self.folder_manager = SonoFolderManager(acquisition_dir_path)
        self.output_params = self.folder_manager.load_output_params()
        (self.gaze_data, self.head_pos_data) = self.folder_manager.load_eye_tracker_data()

        # calulating saliency map generation params
        self.saliency_size_factor = self.saliency_map_width / self.output_params["display_width"]
        self.saliency_map_height = int(self.output_params["display_height"] * self.saliency_size_factor)
        self.saliency_point_px_range = int(self.saliency_point_max_reach / self.saliency_size_factor)

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
        self.avg_v_angle_distances = []
        self.gaze_point_gaussians = []
        
        # data processing steps
        self.calculate_os_acquisition_time()
        self.calculate_avg_position_stats()

        # filtering gaze data according to speed and position
        if self.filter_gaze_data:
            self.filter_gaze_position()
            self.filter_gaze_speed()


    def calculate_os_acquisition_time(self):

        ''' Computes the timestamp at which the data (head and gaze) was acquired for the OS domain '''
        
        for gaze_i in range(self.n_gaze_acquisitions):
            self.gaze_data.loc[gaze_i ,self.os_acquisition_time] = (self.gaze_data.loc[gaze_i, "Reception OS time"] - 
                (self.gaze_data.loc[gaze_i, "Reception tobii time"] - self.gaze_data.loc[gaze_i, "Onboard time"]))

        for head_i in range(self.n_head_acquisitions):
            self.head_pos_data.loc[head_i, self.os_acquisition_time] = (self.head_pos_data.loc[head_i, "Reception OS time"] - 
                (self.head_pos_data.loc[head_i, "Reception tobii time"] - self.head_pos_data.loc[head_i, "Onboard time"]))


    def calculate_avg_position_stats(self):

        ''' 
        Calculates : 
            - user's average head position (m)
            - distance (m) associated with 1 degree of visual angle for each head position
            - 2D gaussian distribution representing a gaze point for each head position
            * head position averages are calculated for every 10% slice of data 
        '''

        # calculating the size of the slices for averaging
        n_slices = 100 // self.config_manager["head_data_slice_percentage"]
        slice_size = self.n_head_acquisitions // n_slices

        # calulating avg head positions
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

        # calculating avg values of 1 deg of visual angle
        for avg_head_pos_data in self.avg_head_positions:
            angle_data = [None , None]
            angle_data[0] = (2 * math.tan(math.radians(1) / 2)) / avg_head_pos_data[0]
            angle_data[1] = avg_head_pos_data[1]
            self.avg_v_angle_distances.append(tuple(angle_data))

        # defining a gaze point gaussian template for all head position values
        for v_angle_data in self.avg_v_angle_distances:

            gaussian_data = [None, None]
            gaussian_data[1] = v_angle_data[1]

            # defining the gaussian function grid edges
            if not (self.saliency_point_px_range % 2): self.saliency_point_px_range -= 1
            grid_max = self.saliency_point_px_range // 2
            grid_min = - (self.saliency_point_px_range // 2)
            
            # defining the gaussian function grid
            x, y = np.meshgrid(
                np.linspace(grid_min, grid_max, self.saliency_point_max_reach), 
                np.linspace(grid_min, grid_max, self.saliency_point_max_reach))

            # calculating the visual angle (1 degree) size in pixels
            sigma = (v_angle_data[0] / self.config_manager["phys_screen_width"]) * self.output_params["display_width"]

            # defining the gaussian function
            gaussian_data[0] = np.exp(-(x*x+y*y) / (2.0 * sigma**2))
            self.gaze_point_gaussians.append(gaussian_data)


    def filter_gaze_speed(self):

        ''' Removes gaze points associated with a gaze speed higher than (config -> max_gaze_speed) '''

        # calculating the max speeds for every average head position (m / s)
        max_speeds = []
        for v_angle_data in self.avg_v_angle_distances:
            speed_data = [None , None]
            speed_data[1] = v_angle_data[1]
            speed_data[0] = self.config_manager["max_gaze_speed"] * v_angle_data[0]
            max_speeds.append(tuple(speed_data))

        # finding points with excessive speeds
        drop_indexes = []
        for gaze_i in range(self.n_gaze_acquisitions - 1):

            first_point = self.gaze_data.iloc[gaze_i]
            second_point = self.gaze_data.iloc[gaze_i + 1]

            # calculating speed for the x and y directions
            time_diff = (second_point[self.os_acquisition_time] - first_point[self.os_acquisition_time]) / 1000000
            x_speed = (abs(second_point["X"] - first_point["X"]) * self.config_manager["phys_screen_width"]) / time_diff
            y_speed = (abs(second_point["Y"] - first_point["Y"]) * self.config_manager["phys_screen_height"]) / time_diff

            # getting the proper speed limit
            max_speed = max_speeds[-1][0]
            for speed_data in max_speeds:
                if first_point[self.os_acquisition_time] <= speed_data[1]:
                    max_speed = speed_data[0]
                    break

            # marking gaze points with excessive speeds for deletion
            if (x_speed >= max_speed) or (y_speed >= max_speed):
                drop_indexes.append(gaze_i+1)
                if gaze_i not in drop_indexes : drop_indexes.append(gaze_i)

        # removing points with excessive speeds
        self.gaze_data.drop(drop_indexes, inplace=True)
        self.gaze_data.reset_index(drop=True, inplace=True)
        self.n_gaze_acquisitions = len(self.gaze_data.index)


    def filter_gaze_position(self):

        ''' Removing gaze points out of bounds of the US image display '''

        # calculating the US image borders (px)
        top_border = self.output_params["display_y"]
        left_border = self.output_params["display_x"]
        right_border = left_border + self.output_params["display_width"]
        bottom_border = top_border + self.output_params["display_height"]

        drop_indexes = []
        for gaze_i in range(self.n_gaze_acquisitions):

            x_screen_coord = round(self.gaze_data.loc[gaze_i, "X"] * self.output_params["screen_width"])
            y_screen_coord = round(self.gaze_data.loc[gaze_i, "Y"] * self.output_params["screen_height"])
            
            # in bounds points get new coordinate entries
            if (x_screen_coord > left_border) and (x_screen_coord < right_border) and\
               (y_screen_coord > top_border) and (y_screen_coord < bottom_border):
                self.gaze_data.loc[gaze_i, self.y_display_coord] = (y_screen_coord - top_border) / self.output_params["screen_height"]
                self.gaze_data.loc[gaze_i, self.x_display_coord] = (x_screen_coord - left_border) / self.output_params["screen_width"]

            # collecting the indexes of out of bounds points
            else: drop_indexes.append(gaze_i)
                
        # droping out of bounds points
        self.gaze_data.drop(drop_indexes, inplace=True)
        self.gaze_data.reset_index(drop=True, inplace=True)
        self.n_gaze_acquisitions = len(self.gaze_data.index)


    def generate_saliency_map(self, timestamp, time_span=100000):

        ''' 
        Generates a saliency map with the gaze data arround the provided timestamp.
        This function should only be called if gaze data was filtered beforehand.
        A saliency map requires a minimum of (config -> saliency_map_min_points) gaze points.

        Parameters
        ----------
        timestamp (int) : (us) the os timestamp arround which to get the gaze data
        time_span (int) : (us) the span of time around the provided timestamp considered

        Returns
        -------
        (np.array or None) : saliency map or None if no gaze data is near the provided timestamp
        '''

        saliency_map = None

        # making sure gaze data was filtered
        if not self.filter_gaze_data:
            raise ValueError("This method should only be called if the gaze was filtered (filter_gaze_data=True)")

        # getting the gaze data around the time stamp
        lower_bound_time = timestamp + time_span
        upper_bound_time = timestamp - time_span
        gaze_point_indexes = self.gaze_data.index[(self.gaze_data[self.os_acquisition_time] <= lower_bound_time) & 
                                                  (self.gaze_data[self.os_acquisition_time] >= upper_bound_time)]

        # making sure valid gaze data is available
        if len(gaze_point_indexes) > self.config_manager["saliency_map_min_points"]:

            # getting the proper gaussian template
            gaussian_point = self.gaze_point_gaussians[-1][0]
            for gaussian_data in self.gaze_point_gaussians:
                if timestamp <= gaussian_data[1]:
                    gaussian_point = gaussian_data[0]
                    break
            
            # generating the saliency map
            saliency_map = np.zeros((self.saliency_map_height, self.saliency_map_width))
            for gaze_i in gaze_point_indexes:

                # getting the top left corner of the gaussian in saliency map coordinates
                gaze_point = self.gaze_data.iloc[gaze_i]
                x_display_position = int(round(gaze_point[self.x_display_coord] * self.saliency_map_width))
                y_display_position = int(round(gaze_point[self.y_display_coord] * self.saliency_map_height))
                corner_top = y_display_position - (self.saliency_point_max_reach - 1) // 2
                corner_left = x_display_position - (self.saliency_point_max_reach - 1) // 2

                # placing the gaussian nf the saliency map
                for gauss_x, x in enumerate(range(corner_left, corner_left + self.saliency_point_max_reach)):
                    for gauss_y, y in enumerate(range(corner_top, corner_top + self.saliency_point_max_reach)):
                        if ((x >= 0) and (x < self.saliency_map_width)) and ((y >= 0) and (y < self.saliency_map_height)):
                            saliency_map[y, x] += gaussian_point[gauss_y, gauss_x]

            # normalizing the values of the saliency map
            saliency_map /= np.sum(saliency_map)
        
        return saliency_map
import os
import os.path
import pandas as pd

from .config import ConfigurationManager

class SonoFolderManager:

    ''' Allows acces to the different data/files in a SonoAssit recording folder '''

    # expected data file names in the output folder
    acc_file_name = "gyro_acceleration.csv"
    eyetracker_file_name = "eye_tracker.csv"
    rgbd_video_file_name = "camera_data.bag"
    orientation_file_name = "gyro_orientation.csv"

    def __init__(self, config_path):

        '''
        Parameters
        ----------
        config_path (str) : path to configuration file
        '''

        # loading configurations
        self.config_path = config_path
        self.config_manager = ConfigurationManager(config_path)

        # defining data file paths
        sono_output_folder = self.config_manager.config_data["recording_folder"]
        self.acc_file_path = os.path.join(sono_output_folder, self.acc_file_name)
        self.eyetracker_file_path = os.path.join(sono_output_folder, self.eyetracker_file_name)
        self.rgbd_video_file_path = os.path.join(sono_output_folder, self.rgbd_video_file_name)
        self.orientation_file_path = os.path.join(sono_output_folder, self.orientation_file_name)


    def get_video_file_path(self):
        return self.rgbd_video_file_path

    def load_gaze_data(self):
        return pd.read_csv(self.eyetracker_file_path)
  
    def load_acc_data(self):
        return pd.read_csv(self.acc_file_path)

    def load_orientation_data(self):
        return pd.read_csv(self.orientation_file_path)
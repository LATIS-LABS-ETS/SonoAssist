import os
import os.path
import pandas as pd

from .config import ConfigurationManager

class SonoFolderManager:

    ''' Allows acces to the different data/files in a SonoAssit recording folder '''

    # defining the expected data file names in the output folder
    folder_file_paths = {
        "clarius_data" : "clarius_data.csv",
        "clarius_video" : "clarius_images.avi",
        "ext_imu_ori" : "ext_imu_orientation.csv",
        "ext_imu_acc" : "ext_imu_acceleration.csv",
        "eyetracker_data" : "eye_tracker.csv",
        "rgbd_video" : "camera_data.bag",
        "screen_rec_data" : "screen_recorder_data.csv",
        "screen_rec_video" : "screen_recorder_images.avi"
    }
    
    def __init__(self, config_path):

        '''
        Parameters
        ----------
        config_path (str) : path to configuration file
        '''

        # loading configurations
        self.config_manager = ConfigurationManager(config_path)

        # defining the folder file paths
        sono_output_folder = self.config_manager.config_data["input_folder"]
        for key in self.folder_file_paths.keys():
            self.folder_file_paths[key] = os.path.join(sono_output_folder, self.folder_file_paths[key])


    # defining the loading functions for the csv files

    def load_gaze_data(self):
        
        gaze_data = None
        try:
            gaze_data = pd.read_csv(self.folder_file_paths["eyetracker_data"])
        except : pass

        return gaze_data

    def load_ext_imu_data(self):

        acc_data = None 
        ori_data = None
        try :
            ori_data = pd.read_csv(self.folder_file_paths["ext_imu_ori"]) 
            acc_data = pd.read_csv(self.folder_file_paths["ext_imu_acc"])
        except : pass

        return (ori_data, acc_data)

    def load_clarius_data(self):

        probe_data = None
        try :
            probe_data = pd.read_csv(self.folder_file_paths["clarius_data"]) 
        except : pass

        return probe_data

    def load_screen_rec_data(self):

        sc_data = None
        try :
            sc_data =  pd.read_csv(self.folder_file_paths["screen_rec_data"])
        except : pass

        return sc_data
import os
import json
import copy
import os.path
import pandas as pd

class SonoFolderManager:

    ''' Allows acces to the different data/files in a SonoAssit recording folder '''

    # defining the expected data file names in the output folder
    folder_file_paths_definition = {
        "clarius_data" : "clarius_data.csv",
        "clarius_video" : "clarius_images.avi",
        "ext_imu_ori" : "ext_imu_orientation.csv",
        "ext_imu_acc" : "ext_imu_acceleration.csv",
        "eyetracker_gaze" : "eye_tracker_gaze.csv",
        "eyetracker_head" : "eye_tracker_head.csv",
        "rgbd_video" : "RGBD_camera_data.bag",
        "rgbd_index" : "RGBD_camera_index.csv",
        "screen_rec_data" : "screen_recorder_data.csv",
        "screen_rec_video" : "screen_recorder_images.avi",
        "output_params" : "sono_assist_output_params.json"
    }

    
    def __init__(self, acq_folder_path):

        '''
        Parameters
        ----------
        acq_folder_path (str) : path to acquisition folder
        '''

        self.acq_folder_path = acq_folder_path
        
        # defining the folder file paths
        self.folder_file_paths = copy.deepcopy(self.folder_file_paths_definition)
        for key in self.folder_file_paths.keys():
            self.folder_file_paths[key] = os.path.join(acq_folder_path, self.folder_file_paths[key])


    def __getitem__(self, key):

        ''' Easy acces to full acquisition folder file paths '''

        return self.folder_file_paths[key]


    def check_loaded_data(self, data):

        ''' Makes sure loaded data is properly formatted '''

        checked_data = None

        try:
            if isinstance(data, pd.DataFrame) and len(data.index) > 0 :
                checked_data = data
        except: pass

        return checked_data 

    # defining the loading functions for the csv files

    def load_eye_tracker_data(self):
        
        gaze_data = None
        head_data = None

        try:
            gaze_data = self.check_loaded_data(pd.read_csv(self.folder_file_paths["eyetracker_gaze"]))
            head_data = self.check_loaded_data(pd.read_csv(self.folder_file_paths["eyetracker_head"]))
        except : pass

        return (gaze_data, head_data)
        
    def load_ext_imu_data(self):

        acc_data = None 
        ori_data = None
        try :
            ori_data = self.check_loaded_data(pd.read_csv(self.folder_file_paths["ext_imu_ori"]))
            acc_data = self.check_loaded_data(pd.read_csv(self.folder_file_paths["ext_imu_acc"]))
        except : pass

        return (ori_data, acc_data)

    def load_clarius_data(self):

        probe_data = None
        try :
            probe_data = self.check_loaded_data(pd.read_csv(self.folder_file_paths["clarius_data"]))
        except : pass

        return probe_data

    def load_rgbd_data(self):

        camera_data = None
        try:
            camera_data = self.check_loaded_data(pd.read_csv(self.folder_file_paths["rgbd_index"]))
        except: pass

        return camera_data

    def load_screen_rec_data(self):

        sc_data = None
        try :
            sc_data =  self.check_loaded_data(pd.read_csv(self.folder_file_paths["screen_rec_data"]))
        except : pass

        return sc_data

    def load_output_params(self):

        param_data = None
        try:
            with open(self.folder_file_paths["output_params"]) as param_file:
                param_data = json.load(param_file)
        except: pass

        return param_data
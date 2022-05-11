import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.axes3d import Axes3D
from scipy.spatial.transform import Rotation as R

from sonopy.file_management import SonoFolderManager


class IMUDataManager:

    ''' Manages data from the external IMUs '''

    n_us_in_s = 1000000

    # Onboard time : number of milliseconds since epoch (according to the board clock)
    # Reception OS Timer: number of microseconds since epoch 
    df_time_fields = ["Reception OS time", "Onboard time"]
    
    quaternion_axes = ["qx", "qy", "qz", "qw"]
    df_orientation_fields = {"X" : "Pitch", "Y" : "Roll", "Z" : "Yaw"}


    def __init__(self, acquisition_dir_path, quaternions=False, avg_data=True, avg_window=0.5):

        ''' 
        Parameters
        ----------
        acquisition_dir_path (str) : path to the acquisition directory
        quaternions (bool) : when True, entries to express the sensor's orientation in quaternions are added to every sample
        avg_data (bool) : when True, imu data is averaged in (avg_window) intervals
        avg_window (float): time window in seconds for the averaging
        '''

        self.avg_window = avg_window
        self.quaternions = quaternions

        # loading IMU data
        self.folder_manager = SonoFolderManager(acquisition_dir_path)
        self.ori_df, _ = self.folder_manager.load_ext_imu_data()
        
        # converting all columns from str to numeric
        for col_name in self.ori_df.columns:
            self.ori_df[col_name] = pd.to_numeric(self.ori_df[col_name], errors="coerce")

        # droping initial empty acquisitions
        self.n_acquisitions = len(self.ori_df.index)

        if avg_data: self._avg_imu_data()


    def _get_nearest_index(self, target_time, time_col_name="Reception OS time"):

        ''' 
        Returns the index associated to the acquisition closest to the provided timestamp

        Parameters
        ----------
        target_time (int) : timestamp (us)
        time_col_name (str) : identifier for the column to use for the timestamp calculations

        Returns
        -------
        (int) : index for the nearest video frame and IMU acquisitions
        '''

        after_index = -1
        before_index = -1
        nearest_index = None

        # getting the before and after indexes
        for i in range(self.n_acquisitions):
            if self.ori_df.loc[i, time_col_name] > target_time:
                after_index = i
                before_index = i-1
                break
        
        # specified time is before acquisition start
        if (before_index == -1) and (after_index == 0) : 
            nearest_index = 0
        # specified time is after acquisition stop
        elif (before_index == -1) and (after_index == -1):
            nearest_index = self.n_acquisitions - 1

        # specified time is during acquisition
        else:

            after_time = self.ori_df.loc[after_index, time_col_name]
            before_time = self.ori_df.loc[before_index, time_col_name]
    
            # choosing between the valid indexes before and after the specified time
            if before_time == target_time:
                nearest_index = before_index 
            else :
                after_index = before_index + 1
                after_time = self.ori_df.loc[after_index, time_col_name]
                if (target_time - before_time) <= (after_time - target_time):
                    nearest_index = before_index
                else:
                    nearest_index = after_index
        
        return nearest_index


    def _avg_imu_data(self, time_col_name="Reception OS time"):

        ''' Averages the IMU acquisitions over windows of (self.avg_window) seconds in length '''

        # setting up time calculations        
        window_size_us = self.avg_window * self.n_us_in_s
        ori_start_time = self.ori_df.loc[0, time_col_name] 

        # defining iteration vars     
        ori_seg_end_i, ori_seg_start_i = 0, 0
        
        # creating empty dataframes to contain the avg data
        avg_ori_df = pd.DataFrame(columns = self.ori_df.columns)

        end_reached = False
        while not end_reached:

            # getting the start and end indexes for the averaging
            ori_start_time += window_size_us
            ori_seg_start_i = ori_seg_end_i
            ori_seg_end_i = self._get_nearest_index(ori_start_time, time_col_name=time_col_name)

            # checking if the end of data was reached
            if ori_seg_end_i >= (self.n_acquisitions - 1):
                end_reached = True
                ori_seg_end_i = self.n_acquisitions - 1
                
            # collecting the data between the start and end points
            avg_container = {}
            n_avg_acq = ori_seg_end_i - ori_seg_start_i + 1
            for i in range(ori_seg_start_i, ori_seg_end_i+1, 1):

                row_data = self.ori_df.iloc[i]
                for field, val in row_data.items():
                    
                    if not field in self.df_time_fields:
                        if not field in avg_container: avg_container[field] = []
                        avg_container[field].append(val)

            # averaging the data between the start and end points
            for field in avg_container.keys():
                avg_container[field] = [sum(avg_container[field]) / n_avg_acq]

            # adding a row in the avg data dataframe
            for time_field in self.df_time_fields: 
                avg_container[time_field] = self.ori_df.loc[ori_seg_start_i, time_field]
            avg_ori_df = avg_ori_df.append(pd.DataFrame.from_dict(avg_container), ignore_index=True)

        # swaping the original data frame for the averaged one
        self.ori_df = avg_ori_df
        self.n_acquisitions = len(self.ori_df.index)


    def __len__(self):
        
        return self.n_acquisitions


    def __getitem__(self, key):
        
        ''' 
        Enables the indexing of acquisitions from the dataframe
        
        Parameters
        ----------
        key (int) : acquisition index

        Returns
        -------
        (dict) : IMU orientation data (angles are in degrees)
        '''

        if not isinstance(key, int):
            raise ValueError("Index key has to be an int")

        if not (key < self.n_acquisitions and key >= 0) :
            raise ValueError("Index is out of range")
        
        indexed_data = dict(self.ori_df.iloc[key])
        
        # converting orientation fields to quaternions if required
        if self.quaternions:
            euler_orientation = [indexed_data[self.df_orientation_fields[axis]] for axis in "XYZ"]
            quaternion_orientation = R.from_euler('xyz', euler_orientation, degrees=True).as_quat()
            for i, axis in enumerate(self.quaternion_axes):
                indexed_data[axis] = quaternion_orientation[i]

        return indexed_data


    def get_data_at_time(self, target_time):
        
        ''' 
        Returns the orientation data from the closest acquisition from the provided timestamp

        Parameters
        ----------
        target_time (int) : timestamp (us)

        Returns
        -------
        (dict) : IMU orientation data
        '''
        
        return self[self._get_nearest_index(target_time)]



class OrientationScene:

    ''' Manages the display of an IMU's orientation '''

    def __init__(self, update_pause_time=0.05, display_triangles=False):

        '''
        update_pause_time (float) : time (in seconds) between graphic updates
        '''

        self.display_triangles = display_triangles
        self.update_pause_time = update_pause_time

        # defining the initial state of the display arrows
        # the base of the arrows / vectors are at the [0,0,0] point
        self.arrow_starting_pos = [0 ,0 , 1]
        self.support_starting_pos = [0.25, 0, 1]
        self.static_arrow = self.arrow_starting_pos
        self.dynamic_arrow = self.arrow_starting_pos

        # defining arrows parallel to the display ones (to define a plane)
        self.static_arrow_2 = self.support_starting_pos
        self.dynamic_arrow_2 = self.support_starting_pos

        # setting up the display and first display
        self.fig = plt.figure()
        self.ax = Axes3D(self.fig)
        self.fig.show()


    def update_dynamic_arrow(self, roll, pitch, yaw):
        
        self.dynamic_arrow = self.update_arrow(self.arrow_starting_pos, roll, pitch, yaw)
        self.dynamic_arrow_2 = self.update_arrow(self.support_starting_pos, roll, pitch, yaw)


    def update_static_arrow(self, roll, pitch, yaw):
        
        self.static_arrow = self.update_arrow(self.arrow_starting_pos, roll, pitch, yaw)
        self.static_arrow_2 = self.update_arrow(self.support_starting_pos, roll, pitch, yaw)


    def update_arrow(self, starting_pos, roll, pitch, yaw):

        ''' 
        Applying a (roll, pitch, yaw) rotation to the provided vector 
        [roll (rx), pitch (ry), yaw (rz)]
        '''

        r_mat = R.from_euler('XYZ', [roll, pitch, yaw], degrees=True)
        return r_mat.apply(starting_pos)
            

    def update_display(self):

        ''' Updating the position of the arrows on the display '''

        plt.cla()

        # plotting arrows or triangles
        if not self.display_triangles:
            self.ax.plot([0, self.dynamic_arrow[0]], [0, self.dynamic_arrow[1]], [0, self.dynamic_arrow[2]], "-o", color="#0331fc", ms=4, mew=0.5)
            self.ax.plot([0, self.static_arrow[0]],[0, self.static_arrow[1]], [0, self.static_arrow[2]], '-o', color="#000000")
        
        else:

            # plotting the dynamic arrows
            dynamic_third_point = [2 * self.dynamic_arrow[i] - self.dynamic_arrow_2[i] for i in range(3)]
            self.ax.plot([0, self.dynamic_arrow[0]], [0, self.dynamic_arrow[1]], [0, self.dynamic_arrow[2]], "-o", color="orange", ms=4, mew=0.5)
            self.ax.plot([0, self.dynamic_arrow_2[0]],[0, self.dynamic_arrow_2[1]], [0, self.dynamic_arrow_2[2]], '-o', color="orange", ms=4, mew=0.5)
            self.ax.plot([0, dynamic_third_point[0]],[0, dynamic_third_point[1]], [0, dynamic_third_point[2]], '-o', color="orange", ms=4, mew=0.5)
            
            # plotting the static arrows
            static_third_point = [2 * self.static_arrow[i] - self.static_arrow_2[i] for i in range(3)]
            self.ax.plot([0, self.static_arrow[0]], [0, self.static_arrow[1]], [0, self.static_arrow[2]], "-o", color="blue", ms=4, mew=0.5)
            self.ax.plot([0, self.static_arrow_2[0]],[0, self.static_arrow_2[1]], [0, self.static_arrow_2[2]], '-o', color="blue", ms=4, mew=0.5)
            self.ax.plot([0, static_third_point[0]],[0, static_third_point[1]], [0, static_third_point[2]], '-o', color="blue", ms=4, mew=0.5)
        
        self.ax.set_xlim(-1, 1)
        self.ax.set_ylim(-1, 1)
        self.ax.set_zlim(-1, 1)

        plt.draw()
        plt.pause(self.update_pause_time)
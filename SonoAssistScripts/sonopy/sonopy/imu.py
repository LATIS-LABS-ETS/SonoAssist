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

        nearest_index = None
        after_index, before_index = -1, -1

        # getting the before and after indexes
        for i in range(self.n_acquisitions):
            if self.ori_df.loc[i, time_col_name] > target_time:
                after_index = i
                before_index = i-1
                break
        
        # handling the edge cases
        if (before_index == -1) and (after_index == 0): nearest_index = 0
        elif (before_index == -1) and (after_index == -1): nearest_index = self.n_acquisitions - 1

        # specified time is during acquisition
        # choosing between the valid indexes before and after the specified time
        else:

            after_time = self.ori_df.loc[after_index, time_col_name]
            before_time = self.ori_df.loc[before_index, time_col_name]

            if before_time == target_time: nearest_index = before_index

            elif (target_time - before_time) <= (after_time - target_time):
                nearest_index = before_index

            else: nearest_index = after_index
        
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


    @staticmethod        
    def quaternion_multiply(quaternion1, quaternion0):
    
        ''' Utility function for multiplying quaternions '''
    
        # https://stackoverflow.com/questions/39000758/how-to-multiply-two-quaternions-by-python-or-numpy
        x0, y0, z0, w0 = quaternion0
        x1, y1, z1, w1 = quaternion1
        return np.array([x1 * w0 + y1 * z0 - z1 * y0 + w1 * x0,
                        -x1 * z0 + y1 * w0 + z1 * x0 + w1 * y0,
                         x1 * y0 - y1 * x0 + z1 * w0 + w1 * z0, 
                        -x1 * x0 - y1 * y0 - z1 * z0 + w1 * w0], dtype=np.float64)


class OrientationScene:

    ''' Manages the display of an IMU's orientation '''

    # us plane position when the probe is clipped to the mount
    arrow_starting_pos = [-1 ,0 , 0]
    support_starting_pos = [-1, 0.25, 0]

    # defining arrow color (up to 3 ... then requires override)
    arrow_colors = ["#a83232", "#71a832", "#8e32a8"]

    def __init__(self, n_dynamic_arrows=1, update_pause_time=0.05, display_triangles=False, fig_title="IMU orientation"):

        '''
        Parameters
        ----------
        n_dynamic_arrows (int) : number of dynamic arrows drawn in the scene
        update_pause_time (float) : time (in seconds) between graphic updates
        display_triangles (bool) : when True, a plane (3 arrows) is displayed instead of an axis (1 arrow)
        fig_title (string) : title to be set for the animated figure
        '''

        self.n_dynamic_arrows = n_dynamic_arrows
        self.display_triangles = display_triangles
        self.update_pause_time = update_pause_time

        # defining the dynamic and static arrows
        # the main arrows are the center arrows and the support arrows are slanted (they help define a plain)

        self.static_arrow = self.arrow_starting_pos.copy()
        self.static_support_arrow = self.support_starting_pos.copy()

        self.dynamic_arrows, self.dynamic_support_arrows = [], []
        for _ in range(self.n_dynamic_arrows):
            self.dynamic_arrows.append(self.arrow_starting_pos.copy())
            self.dynamic_support_arrows.append(self.support_starting_pos.copy())            
        
        # setting up the display and first display
        self.fig = plt.figure(figsize=(8, 8))
        self.fig.suptitle(fig_title, fontsize=16)
        self.ax = Axes3D(self.fig)
        self.fig.show()


    def _update_arrow(self, starting_pos, roll, pitch, yaw):

        ''' 
        Applying a (roll, pitch, yaw) rotation to the provided vector 
        [roll (rx), pitch (ry), yaw (rz)]
        '''

        r_mat = R.from_euler('XYZ', [roll, pitch, yaw], degrees=True)
        return r_mat.apply(starting_pos)


    def update_dynamic_arrow(self, roll, pitch, yaw, index=0):
        
        ''' Provided angles must be in degrees '''

        if index < 0 or index >= self.n_dynamic_arrows:
            raise ValueError("Invalid arrow index provided.")

        self.dynamic_arrows[index] = self._update_arrow(self.arrow_starting_pos, roll, pitch, yaw)
        self.dynamic_support_arrows[index] = self._update_arrow(self.support_starting_pos, roll, pitch, yaw)


    def update_static_arrow(self, roll, pitch, yaw):
        
        ''' Provided angles must be in degrees '''

        self.static_arrow = self._update_arrow(self.arrow_starting_pos, roll, pitch, yaw)
        self.static_support_arrow = self._update_arrow(self.support_starting_pos, roll, pitch, yaw)


    def update_display(self):

        ''' Updates the position of the arrows on the display '''

        plt.cla()

        # only plotting the cental arrows
        if not self.display_triangles:
            self.ax.plot([0, self.static_arrow[0]],[0, self.static_arrow[1]], [0, self.static_arrow[2]], '-o', color="blue")
            for a_i in range(self.n_dynamic_arrows):
                self.ax.plot([0, self.dynamic_arrows[a_i][0]], [0, self.dynamic_arrows[a_i][1]], [0, self.dynamic_arrows[a_i][2]],
                             "-o", color=self.arrow_colors[a_i], ms=4, mew=0.5)
        
        # plotting 3 arrows (triangles)
        else:

            # plotting the dynamic triangles
            for a_i in range(self.n_dynamic_arrows):
                dynamic_third_point = [2 * self.dynamic_arrows[a_i][i] - self.dynamic_support_arrows[a_i][i] for i in range(3)]
                self.ax.plot([0, self.dynamic_arrows[a_i][0]], [0, self.dynamic_arrows[a_i][1]], [0, self.dynamic_arrows[a_i][2]], 
                             "-o", color=self.arrow_colors[a_i], ms=4, mew=0.5)
                self.ax.plot([0, self.dynamic_support_arrows[a_i][0]],[0, self.dynamic_support_arrows[a_i][1]], [0, self.dynamic_support_arrows[a_i][2]],
                             '-o', color=self.arrow_colors[a_i], ms=4, mew=0.5)
                self.ax.plot([0, dynamic_third_point[0]],[0, dynamic_third_point[1]], [0, dynamic_third_point[2]],
                             '-o', color=self.arrow_colors[a_i], ms=4, mew=0.5)
            
            # plotting the static triangle
            static_third_point = [2 * self.static_arrow[i] - self.static_support_arrow[i] for i in range(3)]
            self.ax.plot([0, self.static_arrow[0]], [0, self.static_arrow[1]], [0, self.static_arrow[2]], "-o", color="blue", ms=4, mew=0.5)
            self.ax.plot([0, self.static_support_arrow[0]],[0, self.static_support_arrow[1]], [0, self.static_support_arrow[2]], '-o', color="blue", ms=4, mew=0.5)
            self.ax.plot([0, static_third_point[0]],[0, static_third_point[1]], [0, static_third_point[2]], '-o', color="blue", ms=4, mew=0.5)
        
        self.ax.set_xlim(-1, 1)
        self.ax.set_ylim(-1, 1)
        self.ax.set_zlim(-1, 1)

        plt.draw()
        plt.pause(self.update_pause_time)
'''
This script displays the orientation data from an IMU, in real time, as its being recorded by SonoAssist.

Usage
-----
python3 display_live_imu.py
'''

import time
import redis
import numpy as np

from sonopy.imu import OrientationScene


class IMUProbeRedisModel:

    ''' Enables the acces of IMU (MetaMotion C) data through Redis server '''

    def __init__(self):

        self.data_key = "ext_imu_data"
        
        # connecting to redis and waiting for data
        self.r_connection = redis.StrictRedis(decode_responses=False)
        while not self.r_connection.exists(self.data_key) == 1:
            time.sleep(0.1)


    def data_available(self):
        return self.r_connection.llen(self.data_key)


    def get_imu_data(self):

        ''' 
        Returns timestamps and imu readings corresponding to one acquisition point
            - timestamps: (reception time, onboard time)
            - orientation_data: (heading, yaw, pitch, roll) - (in degrees)
        '''

        times = None
        orientation_data = None

        if self.r_connection.llen(self.data_key) > 1:

            data_str = self.r_connection.lpop(self.data_key).decode('UTF-8')
            if not data_str == "":
                entries = data_str.split(",")
                times = tuple([int(entry.strip()) for entry in entries[:2]])
                orientation_data = tuple([float(entry.strip()) for entry in entries[2:]])

        return times, orientation_data


if __name__ == "__main__":
    
    # initializing the graphics display + redis model
    scene = OrientationScene(update_pause_time=0.05, display_triangles=True)
    us_model = IMUProbeRedisModel()
    
    while True:

        if us_model.data_available():

            # pulling IMU orientation data from Redis
            times, orientation_data = us_model.get_imu_data()
        
            # updating the display
            if orientation_data:
                scene.update_dynamic_arrow(orientation_data[1], orientation_data[2], orientation_data[3])
                scene.update_display()
                print(f"queue size : {us_model.r_connection.llen(us_model.data_key)}")

        else: time.sleep(0.05)
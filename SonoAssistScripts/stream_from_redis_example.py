'''
Example script for streaming SonoAssist acquisition data from Redis DB.

This script should be executed on Windows with a running Redis server while the SonoAssist tool is acquiring data.

Usage
-----
python3 stream_from_redis_example.py
'''

import cv2
import time
import redis
import numpy as np
import matplotlib.pyplot as plt


class USProbeRedisModel:

    ''' Enables the acces of US Probe data through Redis server '''

    def __init__(self, img_width=1260, img_height=720):

        self.img_width = img_width
        self.img_height = img_height

        # defining the de relevant redis keys
        self.imu_data_key = "us_probe_imu_data"
        self.img_data_key = "us_probe_img_data"

        # connecting to redis and waiting for data
        self.r_connection = redis.StrictRedis(decode_responses=False)
        while not self.r_connection.exists(self.imu_data_key) == 1:
            time.sleep(0.1)


    def data_available(self):

        '''  Returns True if new probe data is available '''

        return self.r_connection.llen(self.imu_data_key) > 1


    def get_imu_data(self):

        ''' 
        Returns timestamps and imu readings corresponding to one acquisition point
            - timestamps : (reception time, display time, onboard time)
            - imu readings : [(gx, gy, gz, ax, ay, az, mx, my, mz, qw, qx, qy, qz), ... ]
        '''

        times = None
        readings = []

        try:
            
            if self.r_connection.llen(self.imu_data_key) > 1:

                data_str = self.r_connection.lpop(self.imu_data_key).decode('UTF-8')
                if not data_str == "":
                    
                    for line_str in data_str.split("\n")[:-1]:
                        entries = line_str.split(",")
                        if not times: times = tuple([int(entry.strip()) for entry in entries[:3]])
                        readings.append(tuple([float(entry.strip()) for entry in entries[3:]]))
        
        except: pass

        return times, readings


    def get_img_data(self): 

        ''' Returns the most recent acquired image by the probe '''

        image = None

        try:

            img_bytes = self.r_connection.get(self.img_data_key)
            if not self.r_connection.get(self.img_data_key) == b'':

                image = np.frombuffer(img_bytes, np.uint8)
                image = image.reshape((self.img_height, self.img_width))
            
        except: pass

        return image


if __name__ == "__main__":

    us_model = USProbeRedisModel()
    
    while True:

        if us_model.data_available():

            times, imu_readings = us_model.get_imu_data()
            probe_image = us_model.get_img_data()

            print(times)
            print(f"queue size : {us_model.r_connection.llen(us_model.imu_data_key)}")

        time.sleep(0.01)
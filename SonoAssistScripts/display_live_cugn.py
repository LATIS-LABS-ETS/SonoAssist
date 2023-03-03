''' This script displays the CUGN's movement predictions, in real time, as the model is being evaluated in SonoAssist '''

import math
import time
import redis
import numpy as np

from sonopy.imu import OrientationScene


class CUGNRedisModel:

    ''' Enables the acces of CUGN data through Redis server '''

    def __init__(self):

        self.data_key = "cugn_mov_pred"
        
        # connecting to redis and waiting for data
        self.r_connection = redis.StrictRedis(decode_responses=False)
        while not self.r_connection.exists(self.data_key) == 1:
            time.sleep(0.1)


    def count_data_entries(self):
        return self.r_connection.llen(self.data_key)


    def get_cugn_data(self):

        ''' 
        Fetches the oldest available CUGN model prediction

        Returns
        -------
        pred_data: list([yaw, pitch, roll])
            probe movement prediction (in degrees) or None
        '''

        pred_data = None

        try:
            data_str = self.r_connection.lpop(self.data_key).decode('UTF-8')
            pred_data = [math.degrees(float(entry.strip())) for entry in data_str.split(",")]
            print(pred_data)
            #pred_data = [2*element for element in pred_data]
        except: pass

        return pred_data


if __name__ == "__main__":
    
    n_entries = 0
    display_divide = 2

    # initializing the graphics display + redis model
    cugn_model = CUGNRedisModel()
    scene = OrientationScene(update_pause_time=0.01, display_triangles=True)
    
    while True:
    
        n_entries = cugn_model.count_data_entries()
        for i in range(n_entries):
            
            # pulling model prediction data from Redis
            pred_data = cugn_model.get_cugn_data()
        
            # updating the display
            if pred_data and (i % 2 == 0):
                scene.update_dynamic_arrow(*pred_data)
                scene.update_display()
            
            time.sleep(0.005)
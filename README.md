# SonoAsist

## SonoAssist recorder data flow diagram
![](Media/data_flow.png)
<br>

## SonoAssist recorder configuration
#### This section details every entry in the SonoAssist recorder configuration file.
Note that for all Redis related configurations, the [redis executable](https://github.com/dmajkic/redis/downloads) must be running if data is to be streamed to Redis
#### Parameter descriptions
|Entry Name|Description|
|:--- |:---|
|ext_imu_active|**("true" or "false")** When set to **true**, the recorder app will acquire data from the external IMU unit.|
|us_probe_active|**("true" or "false")** When set to **true**, the recorder app will acquire data from the Clarius ultrasound probe (IMU and US images).|
|rgb_camera_active|**("true" or "false")** When set to **true**, the recorder app will acquire data from the Intel realsense RGBD camera.|
|eye_tracker_active|**("true" or "false")** When set to **true**, the recorder app will acquire data from the Tobii 4c eye tracker.|
|screen_recorder_active|**("true" or "false")** When set to **true**, the recorder app will acquire data from the built-in screen recorder.|
|us_image_main_display_width|**(Interger)** Defines the width of the ultrasound image display (Clarius probe).|
|us_image_main_display_height|**(Interger)** Defines the height of the ultrasound image display (Clarius probe).|
|measure_eye_tracker_accuracy|**("true" or "false")** When set to **true**, a target image will appear on all 4 corners of the US image display. These red dots are useful for the eye tracker precision measurement script. The default targets are red dots.| 
|eye_tracker_to_redis|**("true" or "false")** When set to **true**, the recorder app will export data streamed from the eye tracker to Redis.|
|eye_tracker_redis_rate_div|**(Integer)** In theory, the Tobii 4C eye tracker can reach a sampling rate of 90Hz. If someone is using the Redis interface for testing or visualization purposes, they may not want to read data at this frequency. This parameter divides the hardware sampling rate to set the new frequency at which data will be written to Redis. Ex: if the actual hardware sampling rate is 60Hz and "eye_tracker_redis_rate_div" is set to 2, gaze data will be written to Redis at a frequency of 30Hz.|
|eye_tracker_redis_entry|**(String)** Defines the key for the Redis list containing the eye tracker data.|
|gyroscope_to_redis |**("true" or "false")** When set to **true**, the recorder app will export data streamed from the gyroscope to Redis.|
|gyroscope_redis_rate_div|**(Integer)** same principle as the "eye_tracker_redis_rate_div" parameter.|
|gyroscope_redis_entry|**(String)** Defines the key for the Redis list containing the gyroscope data.|
|us_probe_ip_address|**(String)** Defines the IP address of the Clarius ultrasound probe to connect to.|
|gyroscope_ble_address|**(String)** Defines the Bluetooth address of the gyroscope device (MetaWear C) to connect to.|
|eye_tracker_target_path|**(String)** Defines the path to the image used as the eye tracker targets (for accuracy measurements).|
|eye_tracker_crosshairs_path|**(String)** Defines the path to the image used as the eye tracker crosshairs int the preview display.|

## Development notes 
+ **Connecting to the Clarius probes**
    + The (L738-K-1711-A1500) probe network password: @ATFjm4d
    + Make sure custom firewall rules are activated
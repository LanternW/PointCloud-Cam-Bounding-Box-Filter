# PointCloud-Cam-Bounding-Box-Filter

A ros package.

In config/params.yaml :
  
    [cam_topic] is the topic name of camera image
    [lidar_topic] is the topic name of raw datas of livox lidar
    [tc_l] is the transform matrix from lidar frame to camera frame
    [camera_internal_matrix] is as its name.
    
point cloud filtered output : /roi_point_cloud


Step to install :

    cd catkin_ws/src
    
    git clone 
    
    cd ..
    
    catkin_make
    
run :
    
    source devel/setup.bash
    
    roslaunch pc_measure pc_measure_node.launch
    

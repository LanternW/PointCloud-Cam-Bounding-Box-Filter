#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <geometry_msgs/Twist.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <Eigen/Dense> 
#include <cv_bridge/cv_bridge.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <tf/transform_listener.h>
#include <vector>
#include <string>
using namespace std;
using namespace Eigen;

#define FALSE 0
#define TRUE  1

#define IMG_LENGTH 1920
#define IMG_HEIGHT 1080
// using namespace cv;

ros::Subscriber livox_lidar_sub , pixel_sub;
ros::Subscriber camera_image_sub;
ros::Publisher  ctd_pc_pub;

cv::Mat curr_image;

bool has_img = FALSE;
bool use_img_color = FALSE;

Matrix4d Tc_l ;        // coordinate in Tl to Tc.
Matrix3d K    ;        // camera internal matrix

vector<Vector3d> pc_map[IMG_HEIGHT][IMG_LENGTH];

string cam_topic, lidar_topic;

//tf::TransformListener tf_listener; 
//tf::StampedTransform tf_transform;


void init(ros::NodeHandle& n)
{
    /* default param matrix for example rosbag
    Tc_l << -0.00468494, -0.999917,   -0.012006,   0.0322699,
            -0.00147814,  0.012013,   -0.999927,   0.0363099,
             0.999988,   -0.00466685, -0.0015343, -0.0154693,
                    0,           0,           0,           1;

    K    << 1364.45,        0.0,      958.327,
                0.0,    1366.46,      535.074,
                0.0,        0.0,          1.0;
    */
    double *list4_4 = new double[16];
    double *list3_3 = new double[9];
    XmlRpc::XmlRpcValue param_list;

    if(!n.getParam("/pc_measure/use_img_color", use_img_color)){ ROS_ERROR("Failed to get parameter from server."); }
    if(!n.getParam("/pc_measure/cam_topic", cam_topic)){ ROS_ERROR("Failed to get parameter from server."); }
    if(!n.getParam("/pc_measure/lidar_topic", lidar_topic)){ ROS_ERROR("Failed to get parameter from server."); }

    if(!n.getParam("/pc_measure/tc_l", param_list)){ ROS_ERROR("Failed to get parameter from server."); }
    for (size_t i = 0; i < param_list.size(); ++i) 
    {
        XmlRpc::XmlRpcValue tmp_value = param_list[i];
        if(tmp_value.getType() == XmlRpc::XmlRpcValue::TypeDouble)
            list4_4[i] = (double(tmp_value));
    }

    if(!n.getParam("/pc_measure/camera_internal_matrix", param_list)){ ROS_ERROR("Failed to get parameter from server."); }
    for (size_t i = 0; i < param_list.size(); ++i) 
    {
        XmlRpc::XmlRpcValue tmp_value = param_list[i];
        if(tmp_value.getType() == XmlRpc::XmlRpcValue::TypeDouble)
            list3_3[i] = (double(tmp_value));
    }
    
    Tc_l = Map<Matrix4d>(list4_4).transpose();
    K    = Map<Matrix3d>(list3_3).transpose();
    //Tc_l = Tc_l.transpose();
    //K    = K.transpose();

    cout << "load params successfully as: \n";
    cout << "Tc_l : " << Tc_l << "\n\n K : " << K << "\n\n use img color: " << use_img_color << endl;
    cout << "cam topic : " << cam_topic << "\nlidar topic : " << lidar_topic  << endl;
    
    ROS_INFO("\n====pc_measure_node init====\n");
}

void pushBackPoint(Vector4d pl, int w, int h) {

    if (h < IMG_HEIGHT && h >= 0 && w < IMG_LENGTH && w >= 0){ 
        Vector3d pl_( pl(0) , pl(1), pl(2) );
        pc_map[h][w].push_back(pl_);
    }
}

void clearPCMap() {

    for(int x = 0 ; x < IMG_LENGTH ; x++){
        for(int y = 0 ; y < IMG_HEIGHT ; y++){
            pc_map[y][x].clear();
        }
    }
}

void imageCallback(const sensor_msgs::Image::ConstPtr& img) {

    cv_bridge::CvImagePtr cv_ptr;
    try
    { 
        cv_ptr = cv_bridge::toCvCopy(img, sensor_msgs::image_encodings::RGB8); 
    } 

    catch(cv_bridge::Exception& e)
    { 
        ROS_ERROR("cv_bridge exception: %s", e.what()); 
        return; 
    } 

    curr_image = cv_ptr -> image ; 
    //cout << "got image from the camera" << endl;
    has_img = TRUE;

    /*
    sensor_msgs::PointCloud2 output_pc_ros;
    pcl::PointCloud<pcl::PointXYZRGB> output_pc;
    int points_count = 0;
    for (int y = 0 ; y < IMG_HEIGHT ; y++){
        for(int x = 0 ; x < IMG_LENGTH ; x++){
            points_count += pc_map[y][x].size();
        }
    }
    //cout << points_count << "points total." << endl;
    output_pc.resize(points_count);

    int index = 0;
    for (int y = 0 ; y < IMG_HEIGHT ; y++){
        for(int x = 0 ; x < IMG_LENGTH ; x++){
            for(int i = 0 ; i < pc_map[y][x].size(); i++){
                Vector3d point = pc_map[y][x].at(i);
                output_pc.points[index].x = point(0);
                output_pc.points[index].y = point(1);
                output_pc.points[index].z = point(2);
                output_pc.points[index].r = 100;
                output_pc.points[index].g = 100;
                output_pc.points[index].b = 100;
                index++;
            }
        }
    }
    
    pcl::toROSMsg( output_pc ,output_pc_ros);
    output_pc_ros.header.frame_id = "camera_init";
    ctd_pc_pub.publish(output_pc_ros);
    */

    clearPCMap();
} 


void pointCloudCallback(const sensor_msgs::PointCloud2::ConstPtr& pc_now){

    if (has_img == FALSE){return ;}

    pcl::PointCloud<pcl::PointXYZ> laserCloudIn;
    pcl::fromROSMsg( *pc_now, laserCloudIn );

    for(int i = 0; i < laserCloudIn.points.size() ; i++)
    {
        Vector4d pl, pc;
        Vector3d pc_, v;

        pl(0) = laserCloudIn.points[i].x;
        pl(1) = laserCloudIn.points[i].y;
        pl(2) = laserCloudIn.points[i].z;
        pl(3) = 1;
        pc = Tc_l * pl;

        //pc = pl;
        pc_(0) = pc(0) / pc(2);
        pc_(1) = pc(1) / pc(2);
        pc_(2) = 1;

        v = K * pc_;
        int w = v(0);
        int h = v(1);

        pushBackPoint( pl, w,h );

    }

}

void pixelCallback(const geometry_msgs::Twist::ConstPtr& msg){
    
    int w0 = msg -> linear.x;
    int h0 = msg -> linear.y;
    int w1 = msg -> angular.x;
    int h1 = msg -> angular.y;

    //cout << "Tc_l : " << Tc_l << "\n\n K : " << K << endl;

    if ( w0 < 0 || w0 >= IMG_LENGTH || h0 < 0 || h0 >= IMG_HEIGHT ){return;}
    if ( w1 < 0 || w1 >= IMG_LENGTH || h1 < 0 || h1 >= IMG_HEIGHT ){return;}
    
    sensor_msgs::PointCloud2 output_pc_ros;
    pcl::PointCloud<pcl::PointXYZRGB> output_pc;

    int points_count = 0;
    for(int y = h0 ; y < h1 ; y++){
        for(int x = w0 ; x < w1 ; x++){
            points_count += pc_map[y][x].size();
        }
    }
    cout << points_count << "points find." << endl;
    output_pc.resize(points_count);
    int index = 0;
    for(int y = h0 ; y < h1 ; y++){
        for(int x = w0 ; x < w1 ; x++){
            for(int i = 0 ; i < pc_map[y][x].size(); i++){

                Vector3d point = pc_map[y][x].at(i);
                output_pc.points[index].x = point(0);
                output_pc.points[index].y = point(1);
                output_pc.points[index].z = point(2);
                if (use_img_color){
                    cv::Vec3b color = curr_image.at<cv::Vec3b>(y, x);
                    output_pc.points[index].r = color[0];
                    output_pc.points[index].g = color[1];
                    output_pc.points[index].b = color[2];
                }
                else{
                    output_pc.points[index].r = 255;
                    output_pc.points[index].g = 255;
                    output_pc.points[index].b = 0;
                }
                index ++;
            }
        }
    }

    pcl::toROSMsg( output_pc ,output_pc_ros);
    output_pc_ros.header.frame_id = "camera_init";
    ctd_pc_pub.publish(output_pc_ros);

}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "pc_measure");
    ros::NodeHandle nh;

    init(nh);
    livox_lidar_sub =
        nh.subscribe(lidar_topic , 1000, pointCloudCallback);
    
    camera_image_sub =
        nh.subscribe(cam_topic , 1000, imageCallback);

    pixel_sub =
        nh.subscribe("/image_pixel" , 10, pixelCallback);

    ctd_pc_pub =
        nh.advertise<sensor_msgs::PointCloud2>("/roi_point_cloud", 1000);

    ros::spin();
    return 0;
}
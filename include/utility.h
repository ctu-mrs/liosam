#ifndef UTILITY_H
#define UTILITY_H

#include <ros/ros.h>
#include <nodelet/nodelet.h>

#include <std_msgs/Header.h>
#include <std_msgs/Float64.h>
#include <std_msgs/Float64MultiArray.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/NavSatFix.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#include <opencv2/core/core.hpp>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/impl/search.hpp>
#include <pcl/range_image/range_image.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/common/common.h>
#include <pcl/common/transforms.h>
#include <pcl/registration/icp.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/crop_box.h>
#include <pcl_conversions/pcl_conversions.h>

#include <tf2_ros/transform_broadcaster.h>
#include <tf2/convert.h>

#include <vector>
#include <cmath>
#include <algorithm>
#include <queue>
#include <deque>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cfloat>
#include <iterator>
#include <sstream>
#include <string>
#include <limits>
#include <iomanip>
#include <array>
#include <thread>
#include <mutex>

#include <mrs_lib/param_loader.h>
#include <mrs_lib/attitude_converter.h>
#include <mrs_lib/transformer.h>

#include <mrs_msgs/Float64ArrayStamped.h>

#include "liosam/cloud_info.h"

using namespace std;

typedef pcl::PointXYZI PointType;

/*//{ addNamespace() */
  void addNamespace(const std::string &ns, std::string &s) {
    const bool add_ns = s.rfind("/") != 0 && s.rfind(ns) != 0;
    if (add_ns)
      s = ns + "/" + s;
  }
/*//}*/

/*//{ publishCloud() */
sensor_msgs::PointCloud2 publishCloud(ros::Publisher *thisPub, pcl::PointCloud<PointType>::Ptr thisCloud, ros::Time thisStamp, std::string thisFrame) {

  sensor_msgs::PointCloud2::Ptr tempCloud = boost::make_shared<sensor_msgs::PointCloud2>();
  pcl::toROSMsg(*thisCloud, *tempCloud);
  tempCloud->header.stamp    = thisStamp;
  tempCloud->header.frame_id = thisFrame;

  if (thisPub->getNumSubscribers() > 0) {
    try {
      thisPub->publish(tempCloud);
    }
    catch (...) {
      ROS_ERROR("[LioSam]: Exception caught during publishing topic %s.", thisPub->getTopic().c_str());
    }
  }
  return *tempCloud;
}

template <typename T>
double ROS_TIME(T msg) {
  return msg->header.stamp.toSec();
}
/*//}*/

/*//{ imuAngular2rosAngular() */
template <typename T>
void imuAngular2rosAngular(sensor_msgs::Imu *thisImuMsg, T *angular_x, T *angular_y, T *angular_z) {
  *angular_x = thisImuMsg->angular_velocity.x;
  *angular_y = thisImuMsg->angular_velocity.y;
  *angular_z = thisImuMsg->angular_velocity.z;
}
/*//}*/

/*//{ imuAccel2rosAccel() */
template <typename T>
void imuAccel2rosAccel(sensor_msgs::Imu *thisImuMsg, T *acc_x, T *acc_y, T *acc_z) {
  *acc_x = thisImuMsg->linear_acceleration.x;
  *acc_y = thisImuMsg->linear_acceleration.y;
  *acc_z = thisImuMsg->linear_acceleration.z;
}
/*//}*/

/*//{ imuRPY2rosRPY() */
template <typename T>
void imuRPY2rosRPY(sensor_msgs::Imu *thisImuMsg, T *rosRoll, T *rosPitch, T *rosYaw) {
  double         imuRoll, imuPitch, imuYaw;
  tf2::Quaternion orientation;
  tf2::fromMsg(thisImuMsg->orientation, orientation);
  tf2::Matrix3x3(orientation).getRPY(imuRoll, imuPitch, imuYaw);

  *rosRoll  = imuRoll;
  *rosPitch = imuPitch;
  *rosYaw   = imuYaw;
}
/*//}*/

/*//{ pointDistance() */
float pointDistance(PointType p) {
  return sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
}

float pointDistance(PointType p1, PointType p2) {
  return sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y) + (p1.z - p2.z) * (p1.z - p2.z));
}
/*//}*/

/*//{ imuConverter() */
  sensor_msgs::Imu imuConverter(const sensor_msgs::Imu &imu_in, const Eigen::Matrix3d &extRot, const Eigen::Quaterniond &extQRPY, const bool imuProvidesOrientation) {

    sensor_msgs::Imu imu_out = imu_in;

    // rotate acceleration
    Eigen::Vector3d acc(imu_in.linear_acceleration.x, imu_in.linear_acceleration.y, imu_in.linear_acceleration.z);
    acc                           = extRot * acc;
    imu_out.linear_acceleration.x = acc.x();
    imu_out.linear_acceleration.y = acc.y();
    imu_out.linear_acceleration.z = acc.z();

    // rotate gyroscope
    Eigen::Vector3d gyr(imu_in.angular_velocity.x, imu_in.angular_velocity.y, imu_in.angular_velocity.z);
    gyr                        = extRot * gyr;
    imu_out.angular_velocity.x = gyr.x();
    imu_out.angular_velocity.y = gyr.y();
    imu_out.angular_velocity.z = gyr.z();

    // rotate roll pitch yaw
    Eigen::Quaterniond q_from(imu_in.orientation.w, imu_in.orientation.x, imu_in.orientation.y, imu_in.orientation.z);
    Eigen::Quaterniond q_final = q_from * extQRPY;
    imu_out.orientation.x      = q_final.x();
    imu_out.orientation.y      = q_final.y();
    imu_out.orientation.z      = q_final.z();
    imu_out.orientation.w      = q_final.w();

    if (imuProvidesOrientation && sqrt(q_final.x() * q_final.x() + q_final.y() * q_final.y() + q_final.z() * q_final.z() + q_final.w() * q_final.w()) < 0.1) {
      ROS_ERROR("Invalid quaternion, please use a 9-axis IMU!");
      ros::shutdown();
    }

    return imu_out;
  }
/*//}*/

/*//{ findLidar2ImuTf() */
void findLidar2ImuTf(std::shared_ptr<mrs_lib::Transformer> transformer, const string &lidarFrame, const string &imuFrame, const string &baselinkFrame, Eigen::Matrix3d &extRot, Eigen::Quaterniond &extQRPY, geometry_msgs::TransformStamped &tfLidar2Baselink, geometry_msgs::TransformStamped &tfLidar2Imu) {

    /*//{ find lidar -> base_link transform */
    if (lidarFrame != baselinkFrame) {

      bool tf_found = false;
      while (!tf_found) {
        try {
          auto res = transformer->getTransform(lidarFrame, baselinkFrame, ros::Time(0));
          if (res) {
            tfLidar2Baselink = res.value();
            tf_found = true;
          } else {
            ROS_WARN_THROTTLE(3.0, "Waiting for transform from: %s, to: %s.", lidarFrame.c_str(), baselinkFrame.c_str());
          }
        }
        catch (tf2::TransformException ex) {
          ROS_ERROR("%s",ex.what());
        }
      }

      ROS_INFO("Found transform from: %s, to: %s.", lidarFrame.c_str(), baselinkFrame.c_str());

    } else {

      tfLidar2Baselink.transform.translation.x = 0;
      tfLidar2Baselink.transform.translation.y = 0;
      tfLidar2Baselink.transform.translation.z = 0;
      tfLidar2Baselink.transform.rotation.x = 0;
      tfLidar2Baselink.transform.rotation.y = 0;
      tfLidar2Baselink.transform.rotation.z = 0;
      tfLidar2Baselink.transform.rotation.w = 1;
    }
    /*//}*/

    /*//{ find lidar -> imu transform */
    if (imuFrame == baselinkFrame) {

      tfLidar2Imu = tfLidar2Baselink;
      ROS_INFO("Found transform from: %s, to: %s.", lidarFrame.c_str(), imuFrame.c_str());

    } else if (imuFrame != lidarFrame) {

      bool tf_found = false;
      while (!tf_found) {
        try {
          auto res = transformer->getTransform(lidarFrame, imuFrame, ros::Time(0));
          if (res) {
            tfLidar2Imu = res.value();
            tf_found = true;
          } else {
            ROS_WARN_THROTTLE(3.0, "Waiting for transform from: %s, to: %s.", lidarFrame.c_str(), baselinkFrame.c_str());
          }
        }
        catch (tf2::TransformException ex) {
          ROS_WARN_THROTTLE(3.0, "Waiting for transform from: %s, to: %s.", lidarFrame.c_str(), imuFrame.c_str());
        }
      }

      ROS_INFO("Found transform from: %s, to: %s.", lidarFrame.c_str(), imuFrame.c_str());

    } else {

      tfLidar2Imu.transform.translation.x = 0;
      tfLidar2Imu.transform.translation.y = 0;
      tfLidar2Imu.transform.translation.z = 0;
      tfLidar2Imu.transform.rotation.x = 0;
      tfLidar2Imu.transform.rotation.y = 0;
      tfLidar2Imu.transform.rotation.z = 0;
      tfLidar2Imu.transform.rotation.w = 1;
    }
    /*//}*/

    extQRPY = Eigen::Quaterniond(tfLidar2Imu.transform.rotation.w, tfLidar2Imu.transform.rotation.x, tfLidar2Imu.transform.rotation.y, tfLidar2Imu.transform.rotation.z);
    extRot  = Eigen::Matrix3d(extQRPY);

    ROS_INFO("Transform from: %s, to: %s.", lidarFrame.c_str(), baselinkFrame.c_str());
    ROS_INFO("   xyz: (%0.1f, %0.1f, %0.1f); xyzq: (%0.1f, %0.1f, %0.1f, %0.1f)", tfLidar2Baselink.transform.translation.x, tfLidar2Baselink.transform.translation.y,
             tfLidar2Baselink.transform.translation.z, tfLidar2Baselink.transform.rotation.x, tfLidar2Baselink.transform.rotation.y, tfLidar2Baselink.transform.rotation.z,
             tfLidar2Baselink.transform.rotation.w);
    ROS_INFO("Transform from: %s, to: %s.", lidarFrame.c_str(), imuFrame.c_str());
    ROS_INFO("   xyz: (%0.1f, %0.1f, %0.1f); xyzq: (%0.1f, %0.1f, %0.1f, %0.1f)", tfLidar2Imu.transform.translation.x, tfLidar2Imu.transform.translation.y,
             tfLidar2Imu.transform.translation.z, tfLidar2Imu.transform.rotation.x, tfLidar2Imu.transform.rotation.y, tfLidar2Imu.transform.rotation.z,
             tfLidar2Imu.transform.rotation.w);

  }
/*//}*/

/*//{ containsNan() */
bool containsNan(const tf2::Transform& tf) {
  return isnan(tf.getOrigin().getX()) && isnan(tf.getOrigin().getY()) && isnan(tf.getOrigin().getZ()) && isnan(tf.getRotation().getX()) && isnan(tf.getRotation().getY()) && isnan(tf.getRotation().getZ()) && isnan(tf.getRotation().getW());
}
/*//}*/

#endif // UTILITY_H

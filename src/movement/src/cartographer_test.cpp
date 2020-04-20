//
// Created by eldar on 02.04.2020.
//

#include "ros/ros.h"
#include "cartographer_ros_msgs/SubmapList.h"
#include "tf/transform_listener.h"
#include "nav_msgs/OccupancyGrid.h"
#include <cmath>
#include "cartographer_ros_msgs/SubmapQuery.h"

ros::ServiceClient submapQueryClient;

void submapCallback(const cartographer_ros_msgs::SubmapList::ConstPtr& list)
{
    for (auto s : list->submap) {
        ROS_INFO("submap: %d", s.submap_index);
        ROS_INFO("X: %f", s.pose.position.x);
        ROS_INFO("Y: %f", s.pose.position.y);
        ROS_INFO("Z: %f", s.pose.position.z);
        cartographer_ros_msgs::SubmapQuery service;
        service.request.submap_index = s.submap_index;
        service.request.trajectory_id = s.trajectory_id;
        if (submapQueryClient.call(service)) {
            ROS_INFO("Submap version: %d", service.response.submap_version);
            for (const auto& texture : service.response.textures) {
                ROS_INFO("Resolution: %f, width: %d, height: %d", texture.resolution, texture.width, texture.height);
                auto o = texture.slice_pose.orientation;
                auto p = texture.slice_pose.position;
                ROS_INFO("Slice pose: position. X = %f, Y = %f, Z = %f", p.x, p.y, p.z);
                ROS_INFO("Slice pose: orientation. X = %f, Y = %f, Z = %f, W = %f", o.x, o.y, o.z, o.w);
                int i = 0;
                int j = 0;
                for (auto cell : texture.cells) {
                    printf("%d ", cell);
                    i++;
                    j++;
                    if (i >= texture.width) {
                        putchar('\n');
                        i = 0;
                    }
                }
                ROS_INFO("Texture cells: %d", j);
            }
        } else {
            ROS_ERROR("Error of submap query client!");
        }

    }
}

void occupancyGridCallback(const nav_msgs::OccupancyGrid::ConstPtr& grid)
{
    /*int width = grid->info.width;
    int i = 0;
    auto position = grid->info.origin.position;
    auto orientation = grid->info.origin.orientation;
    for (auto &p : grid->data)
    {
        printf("%d ", p);
        i++;
        if (i >= width) {
            putchar('\n');
            i = 0;
        }
    }
    ROS_INFO("X = %f OX = %f", position.x, orientation.x);
    ROS_INFO("Y = %f OY = %f", position.y, orientation.y);
    ROS_INFO("Z = %f OZ = %f", position.z, orientation.z);
    ROS_INFO("OW = %f", orientation.w);*/
}

void transformPoint(const tf::TransformListener& listener) {
    geometry_msgs::PointStamped laser_point;

    //laser_point.header.frame_id = "base_laser";
    laser_point.header.frame_id = "base_link";

    laser_point.header.stamp = ros::Time();
    laser_point.point.x = 0.0;
    laser_point.point.y = 0.0;
    laser_point.point.z = 0.0;

    try {
        geometry_msgs::PointStamped base_point;
        //listener.transformPoint("base_link", laser_point, base_point);
        listener.transformPoint("map", laser_point, base_point);

        ROS_INFO("base_link: (%.2f, %.2f, %.2f)",
                 base_point.point.x, base_point.point.y, base_point.point.z);
    } catch(tf::TransformException& ex) {
        ROS_ERROR("Received an exception trying to transform a point from \"base_link\" to \"map\": %s", ex.what());
    }
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "cartographer_test");
    ros::NodeHandle nodeHandle;
    ros::Subscriber subscriber = nodeHandle.subscribe("submap_list", 1000, submapCallback);
    ros::Subscriber subscriber1 = nodeHandle.subscribe("map", 1000, occupancyGridCallback);
    tf::TransformListener listener(nodeHandle);
    //ros::Timer timer = nodeHandle.createTimer(ros::Duration(1.0), boost::bind(&transformPoint, boost::ref(listener)));
    tf::StampedTransform transform_bot;

    submapQueryClient = nodeHandle.serviceClient<cartographer_ros_msgs::SubmapQuery>("submap_query");

    while (ros::ok())
    {
        listener.waitForTransform("base_link", "map", ros::Time(0), ros::Duration(1.0));
        listener.lookupTransform("base_link", "map", ros::Time(0), transform_bot);

        double bot_x = transform_bot.getOrigin().x();
        double bot_y = transform_bot.getOrigin().y();

        double roll, pitch, yaw;
        transform_bot.getBasis().getRPY(roll, pitch, yaw);

        double bot_dir = yaw * 180.0 / M_PI;

        /*ROS_INFO("Bot X: %f", bot_x);
        ROS_INFO("Bot Y: %f", bot_y);
        ROS_INFO("Bot Z: %f", bot_dir);*/
        sleep(1);
        ros::spinOnce();
    }
    return EXIT_SUCCESS;
}

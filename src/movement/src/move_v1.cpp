//
// Created by eldar on 20.04.2020.
//

#include "ros/ros.h"
#include "../../gpio_jetson_service/include/gpio_jetson_service/commands.hpp"
#include "pcl_ros/point_cloud.h"
#include "gpio_jetson_service/gpio_srv.h"
#include <sensor_msgs/LaserScan.h>
#include <vector>

bool backward, left, forward, right;
ros::ServiceClient gpio_client;

void ydLidarPointsCallback(const sensor_msgs::LaserScanConstPtr& message) {
    /*if (message->ranges[360] > 0 && message->ranges[360] < 0.2f) {
    }*/
    for (int i = 0; i < 720; i++) {
        if (message->ranges[i] > 0 && message->ranges[i] < 0.3f) {
            if (i > 270 && i < 450) {
                ROS_WARN("Backward!");
                backward = true;
            } else
            if (i > 90 && i < 270) {
                ROS_WARN("Left!");
                left = true;
            } else
            if (i > 630 || i < 90) {
                ROS_WARN("Forward!");
                forward = true;
            } else
            if (i > 450 && i < 630) {
                ROS_WARN("Right!");
                right = true;
            }
        }
    }
}

void gpio_command(const uint8_t command) {
    gpio_jetson_service::gpio_srv service;
    service.request.command = command;
    gpio_client.call(service);
}

void movement() {
    if (forward) {
        gpio_command(MoveCommands::FULL_STOP);
    } else {
        gpio_command(MoveCommands::FORWARD_LOW);
    }
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "movement");
    ros::NodeHandle nodeHandle;
    ros::Subscriber ydlidarPointsSub =
            nodeHandle.subscribe<sensor_msgs::LaserScan>("/scan", 1000, ydLidarPointsCallback);
    gpio_client = nodeHandle.serviceClient<gpio_jetson_service::gpio_srv>("gpio_jetson_service");
    gpio_jetson_service::gpio_srv service;
    service.request.command = MoveCommands::FULL_STOP;
    gpio_client.call(service);
    while (ros::ok()) {
        movement();
        ros::spinOnce();
    }
    return EXIT_SUCCESS;
}
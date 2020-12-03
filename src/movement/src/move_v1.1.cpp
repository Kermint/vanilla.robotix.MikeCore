//
// Created by eldar on 20.04.2020.
//

#include "ros/ros.h"
#include "../../gpio_jetson_service/include/gpio_jetson_service/commands.hpp"
#include "pcl_ros/point_cloud.h"
#include "gpio_jetson_service/gpio_srv.h"
#include <sensor_msgs/LaserScan.h>
#include <vector>
#include "tf/transform_listener.h"
#include "inference/Bboxes.h"

#define IMAGE_WIDTH 300
#define IMAGE_HEIGHT 300
#define FAULT 20

enum ROBOT_STATES {
    FREE_RIDE, AVOID_OBSTACLE, TARGETING_OBJECT, MOVE_TO_OBJECT, STUCK, STOP, DETECTING_OBJECT
} robot_state;

// Free check
bool backward, left, forward, right;

// TODO REMOVE
bool object_detected, chasis_correction;

// Average on sides of robot
float backward_m = 0, left_m = 0, forward_m = 0, right_m = 0;

// Last position and angle of robot
double x = 0, y = 0, r = 0;

double transform_time_sec;
ros::ServiceClient gpio_client;
tf::TransformListener* transformListener;
tf::StampedTransform transform_bot;

// TODO remove it
float image_middle_x, image_middle_y;

// State where to go TODO may be delete it
int min_left_right = 0;

// Full turn on the right direction
bool can_switch_side = true;

// When we last time see the target object
ros::Time time_last_object;

// Counter of frames when we see the object
int object_detected_count = 0;

// Time when the last chage was applied
ros::Time last_change_state;

// Counter of spins from last change state
unsigned int spins_last_change_state;

uint8_t last_move_command;

void gpio_command(const uint8_t command) {
    if (command != last_move_command) {
        gpio_jetson_service::gpio_srv service;
        service.request.command = MoveCommands::FULL_STOP;
        gpio_client.call(service);
    }
    gpio_jetson_service::gpio_srv service2;
    service2.request.command = command;
    gpio_client.call(service2);
    last_move_command = command;
}

void inferenceCallback(const inference::BboxesConstPtr &bboxes) {
    if(bboxes->bboxes.empty()) {
        return;
    }
//    ROS_WARN("Bboxes got! Size: %lu", bboxes->bboxes.size());
    inference::Bbox bbox;
    if (bboxes->bboxes.size() > 1) {
        float max_score = 0;
        unsigned long max_score_index = 0;
        for (unsigned long i = 0; i < bboxes->bboxes.size(); i++) {
//            ROS_WARN("Object %s with score %f.", bboxes->bboxes[i].label.c_str(), bboxes->bboxes[i].score);
            if (max_score < bboxes->bboxes[i].score) {

                if (bboxes->bboxes[i].label != "bottle") continue;

                max_score = bboxes->bboxes[i].score;
                max_score_index = i;
            }
        }
        bbox = bboxes->bboxes[max_score_index];
    } else bbox = bboxes->bboxes[0];

    if (bbox.label != "bottle") return;
    object_detected_count++;
    object_detected = true;

    if (object_detected_count < 5) {
        return;
    }

    ROS_WARN("Selected object %s with score %f and (%f,%f,%f,%f).", bbox.label.c_str(), bbox.score, bbox.x_min, bbox.y_min, bbox.x_max, bbox.y_max);
    float x1 = bbox.x_min;
    float y1 = bbox.y_min;
    float x2 = bbox.x_max;
    float y2 = bbox.y_max;

    float object_center_x = (x1 + x2) / 2;
    float object_center_y = (y1 + y2) / 2;
    ROS_WARN("Object center (%f,%f).", object_center_x, object_center_y);

    chasis_correction = false;

    gpio_command(MoveCommands::FULL_STOP);
    usleep(100000);

    /*if (x1 < FAULT || x2 > IMAGE_WIDTH - FAULT || y1 < FAULT || y2 > IMAGE_HEIGHT - FAULT) {
        return;
    }*/

    if (object_center_x < image_middle_x - FAULT) {
        chasis_correction = true;
        ROS_WARN("Follow left to the object...");
        gpio_command(MoveCommands::RIGHT_FORWARD_MIDDLE);
        usleep(50000);
        gpio_command(MoveCommands::FULL_STOP);
        usleep(50000);
    }

    if (object_center_x > image_middle_x + FAULT) {
        chasis_correction = true;
        ROS_WARN("Follow right to the object...");
        gpio_command(MoveCommands::LEFT_FORWARD_MIDDLE);
        usleep(50000);
        gpio_command(MoveCommands::FULL_STOP);
        usleep(50000);
    }

//    if (!chasis_correction) {
//        if (x1 < FAULT || x2 > IMAGE_WIDTH - FAULT || y1 < FAULT || y2 > IMAGE_HEIGHT - FAULT) {
//            gpio_command(MoveCommands::FULL_STOP);
//        } else {
//            gpio_command(MoveCommands::FORWARD_LOW);
//        }
//    }

    time_last_object = ros::Time::now();
    if (!chasis_correction) {
        gpio_command(MoveCommands::FORWARD_MIDDLE);
        usleep(100000);
    }

}

void ydLidarPointsCallback(const sensor_msgs::LaserScanConstPtr& message) {
    float backward_lm = 0, left_lm = 0, forward_lm = 0, right_lm = 0;
    for (int i = 0; i < 719; ++i) {
        /*if (message->ranges[i] > 1) {
            ROS_WARN("Range on %d > 1 !!! and equals %f", i, message->ranges[i]);
        }*/
        if (i > 270 && i < 450) {
            backward_lm += message->ranges[i] > 0 ? message->ranges[i] : 1;
        } else
            if (i > 90 && i < 270) {
//        if (i > 90 && i < 180) {
            left_lm += message->ranges[i] > 0 ? message->ranges[i] : 1;
        } else
        if (i > 630 || i < 90) {
            forward_lm += message->ranges[i] > 0 ? message->ranges[i] : 1;
        } else
            if (i > 450 && i < 630) {
//        if (i > 540 && i < 630) {
            right_lm += message->ranges[i] > 0 ? message->ranges[i] : 1;
        }
    }
    backward_m = backward_lm / 180;
//    left_m = left_lm / 180;
    left_m = left_lm / 90;
    forward_m = forward_lm / 180;
//    right_m = right_lm / 180;
    right_m = right_lm / 90;
    /*if (message->ranges[360] > 0 && message->ranges[360] < 0.2f) {
    }*/
    left = right = backward = forward = false;
    for (int i = 0; i < 720; i++) {
        if (message->ranges[i] > 0 && message->ranges[i] <= 0.2f) {
            if (i > 270 && i < 450) {
                //ROS_WARN("Backward obstacle");
                //ROS_INFO("Range %f", message->ranges[i]);
                backward = true;
                return;
            } else
            if (i > 90 && i < 270) {
                //ROS_WARN("Left obstacle");
                //ROS_INFO("Range %f", message->ranges[i]);
                left = true;
                return;
            } else
//            if (i > 630 || i < 90) {
            if (i > 675 || i < 45) {
                ROS_WARN("Forward obstacle");
                //ROS_INFO("Range %f", message->ranges[i]);
                forward = true;
                return;
            } else
            if (i > 450 && i < 630) {
                //ROS_WARN("Right obstacle");
                //ROS_INFO("Range %f", message->ranges[i]);
                right = true;
                return;
            }
        }
    }
}

void movement() {
    if (object_detected) return;
    if (forward) {
        if (can_switch_side)
            min_left_right = left_m <= right_m ? 0 : 1;
        can_switch_side = false;
        switch (min_left_right) {
            case 0:
                ROS_WARN("Going to the left side");
                gpio_command(MoveCommands::RIGHT_FORWARD_MIDDLE);
                usleep(300000);
//                sleep(1);
//                gpio_command(MoveCommands::FORWARD_LOW);
                break;
            case 1:
                ROS_WARN("Going to the right side");
                gpio_command(MoveCommands::LEFT_FORWARD_MIDDLE);
                usleep(300000);
//                sleep(1);
//                gpio_command(MoveCommands::FORWARD_LOW);
                break;
            default:
                ROS_ERROR("Case doesn't exist!");
        }
    } else {
        gpio_command(MoveCommands::FORWARD_MIDDLE);
        can_switch_side = true;
    }
}

void stuck_detect() {
    ros::Time now = ros::Time::now();
    if (now.toSec() - transform_time_sec < 1) {
        return;
    }
//    if (object_detected) return;
    //ROS_WARN("Time passed!");
    try {
        transformListener->waitForTransform("base_link", "map", ros::Time(0), ros::Duration(1.0));
        transformListener->lookupTransform("base_link", "map", ros::Time(0), transform_bot);
    } catch (tf2::LookupException &exception) {
        ROS_ERROR("error: %s", exception.what());
        return;
    }
    //ROS_WARN("Wait for transform passed");

    double bot_x = transform_bot.getOrigin().x();
    double bot_y = transform_bot.getOrigin().y();

    double roll, pitch, yaw;
    transform_bot.getBasis().getRPY(roll, pitch, yaw);

    double bot_dir = yaw * 180.0 / M_PI;

    double dX = std::abs(bot_x - x);
    double dY = std::abs(bot_y - y);
    double dR = std::abs(bot_dir - r);

    if (dX < 0.05 && dY < 0.05 && dR < 3.0 && !object_detected) {
        ROS_WARN("Stuck detected!!!");
        gpio_command(MoveCommands::BACKWARD_FAST);
        sleep(1);
        ros::spinOnce();
        forward = true;
        movement();
    }

    ROS_WARN("dX = %f dY = %f dR = %f", dX, dY, dR);

    x = bot_x;
    y = bot_y;
    r = bot_dir;
    transform_time_sec = ros::Time::now().toSec();
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "movement");
    image_middle_x = IMAGE_WIDTH / 2.0;
    image_middle_y = IMAGE_HEIGHT / 2.0;
    ros::NodeHandle nodeHandle;
    //sleep(5);
    transform_time_sec = ros::Time::now().toSec();
    transformListener = new tf::TransformListener(nodeHandle);
    ros::Subscriber ydlidarPointsSub =
            nodeHandle.subscribe<sensor_msgs::LaserScan>("/scan", 1, ydLidarPointsCallback);
    ros::Subscriber inferenceSub =
            nodeHandle.subscribe<inference::Bboxes>("/bboxes", 1, inferenceCallback);
    gpio_client = nodeHandle.serviceClient<gpio_jetson_service::gpio_srv>("gpio_jetson_service");
    gpio_jetson_service::gpio_srv service;
    service.request.command = MoveCommands::FULL_STOP;
    gpio_client.call(service);
    while (ros::ok()) {
        //ROS_WARN("%f", ros::Time::now().toSec() - time_last_object.toSec());
        /*if (ros::Time::now().toSec() - time_last_object.toSec() > 1) {
            object_detected = false;
        }*/
        if (!object_detected) {
            movement();
        }
        stuck_detect();
        //ROS_INFO("Forward: %f, Left: %f, Right: %f, Backward: %f", forward_m, left_m, right_m, backward_m);
        object_detected = false;
        ros::spinOnce();
    }
    gpio_command(MoveCommands::FULL_STOP);
    return EXIT_SUCCESS;
}